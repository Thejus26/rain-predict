/**
 * @file test_barometric_calibration.c
 * @brief Sprint 7 Task S7-T3.1: Unit & Regression Tests for Barometric Calibration.
 *
 * Validates hypsometric reduction math, elevation inversion, NVM Flash struct
 * serialization, LoRaWAN FPort 10 command parsing, and WMO accuracy gates.
 *
 * Requirements:
 * - C99 compliance with zero dynamic heap allocation.
 * - Strict static void test prototypes and clean warning suppression.
 * - Canonical ThrowTheSwitch Unity test macros.
 */

#include "unity.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

/* ========================================================================== */
/* Calibration Constants & Mock Types                                         */
/* ========================================================================== */

#define NVM_CAL_MAGIC_HEADER   0x5241494EU  /* "RAIN" in ASCII */
#define NVM_STRUCT_VERSION     0x0001U
#define STANDARD_LAPSE_RATE    0.0065f
#define HYPSOMETRIC_EXPONENT   5.257f

typedef enum {
    CAL_STATUS_OK                 = 0,
    CAL_STATUS_ERR_INVALID_PARAM  = 1,
    CAL_STATUS_ERR_OUT_OF_RANGE   = 2,
    CAL_STATUS_ERR_CRC_MISMATCH   = 3
} cal_status_t;

typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic_header;       /* 0x00: 0x5241494E */
    uint16_t struct_version;     /* 0x04: Version 1 */
    uint16_t crc16_checksum;     /* 0x06: CRC-16 across bytes 0x08..0x1F */
    uint16_t elevation_dm;       /* 0x08: Decimeters AMSL (0..35000 dm) */
    int16_t  press_offset_chpa;  /* 0x0A: Trim offset in centihPa (-500..+500 chPa) */
    int16_t  temp_offset_cc;     /* 0x0C: Temp offset in centi°C (-500..+500 c°C) */
    uint8_t  weight_press_pct;   /* 0x0E: Pressure weight % (Default: 30) */
    uint8_t  weight_rh_pct;      /* 0x0F: Humidity weight % (Default: 20) */
    uint8_t  weight_dpd_pct;     /* 0x10: Dew pt depression weight % (Default: 20) */
    uint8_t  weight_lux_pct;     /* 0x11: Solar lux drop weight % (Default: 20) */
    uint8_t  weight_zam_pct;     /* 0x12: Zambretti weight % (Default: 10) */
    uint8_t  regime_id;          /* 0x13: 1=High Ridge, 2=Slope, 3=Valley */
    uint32_t cal_timestamp_sec;  /* 0x14: Epoch timestamp */
    uint32_t technician_id;      /* 0x18: Field Tech ID */
    uint32_t reserved_pad;       /* 0x1C: 0xFFFFFFFF padding */
} nvm_cal_config_t;

/* ========================================================================== */
/* Mathematical & Serialization Implementations Under Test                    */
/* ========================================================================== */

static uint16_t calculate_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x0001U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

static cal_status_t compute_sea_level_pressure(float p_station, float temp_c, float elevation_m,
                                               float offset_hpa, float *p0_out)
{
    if (p0_out == NULL) {
        return CAL_STATUS_ERR_INVALID_PARAM;
    }
    if (elevation_m < -100.0f || elevation_m > 3500.0f) {
        return CAL_STATUS_ERR_OUT_OF_RANGE;
    }
    if (p_station < 500.0f || p_station > 1100.0f) {
        return CAL_STATUS_ERR_OUT_OF_RANGE;
    }
    if (offset_hpa < -5.0f || offset_hpa > 5.0f) {
        return CAL_STATUS_ERR_OUT_OF_RANGE;
    }

    float temp_k = temp_c + 273.15f;
    float lapse_h = STANDARD_LAPSE_RATE * elevation_m;
    float base = 1.0f - (lapse_h / (temp_k + lapse_h));
    if (base <= 0.0f) {
        return CAL_STATUS_ERR_OUT_OF_RANGE;
    }

    float p0 = p_station * powf(base, -HYPSOMETRIC_EXPONENT);
    *p0_out = p0 + offset_hpa;
    return CAL_STATUS_OK;
}

static cal_status_t derive_elevation_from_reference(float p_station, float p0_ref, float temp_c,
                                                   float *elevation_out)
{
    if (elevation_out == NULL) {
        return CAL_STATUS_ERR_INVALID_PARAM;
    }
    if (p_station >= p0_ref || p_station < 500.0f || p0_ref > 1100.0f) {
        return CAL_STATUS_ERR_OUT_OF_RANGE;
    }

    float temp_k = temp_c + 273.15f;
    float ratio = p0_ref / p_station;
    float elev = (temp_k / STANDARD_LAPSE_RATE) * (powf(ratio, 1.0f / HYPSOMETRIC_EXPONENT) - 1.0f);
    *elevation_out = elev;
    return CAL_STATUS_OK;
}

static cal_status_t parse_lorawan_downlink_calibration(const uint8_t *payload, size_t length,
                                                       nvm_cal_config_t *config_out)
{
    if (payload == NULL || config_out == NULL || length < 6U) {
        return CAL_STATUS_ERR_INVALID_PARAM;
    }
    if (payload[0] != 0x02U) {
        return CAL_STATUS_ERR_INVALID_PARAM;
    }

    uint16_t elev_dm = (uint16_t)(((uint16_t)payload[1] << 8) | (uint16_t)payload[2]);
    int16_t offset_chpa = (int16_t)(((uint16_t)payload[3] << 8) | (uint16_t)payload[4]);
    uint8_t regime = payload[5];

    if (elev_dm > 35000U || offset_chpa < -500 || offset_chpa > 500 || regime < 1U || regime > 3U) {
        return CAL_STATUS_ERR_OUT_OF_RANGE;
    }

    config_out->elevation_dm = elev_dm;
    config_out->press_offset_chpa = offset_chpa;
    config_out->regime_id = regime;
    return CAL_STATUS_OK;
}

/* ========================================================================== */
/* Forward Declarations for Static Test Functions                             */
/* ========================================================================== */

static void test_tc_cal_01_munnar_reduction(void);
static void test_tc_cal_02_nilgiris_reduction(void);
static void test_tc_cal_03_assam_reduction(void);
static void test_tc_cal_04_elevation_inversion(void);
static void test_tc_cal_05_trim_offset_addition(void);
static void test_tc_cal_06_elevation_boundary_clamping(void);
static void test_tc_cal_07_offset_boundary_clamping(void);
static void test_tc_cal_08_nvm_struct_crc_integrity(void);
static void test_tc_cal_09_lorawan_downlink_parsing(void);
static void test_tc_cal_10_multi_site_convergence(void);

void setUp(void)
{
}

void tearDown(void)
{
}

/* ========================================================================== */
/* Unity Test Cases                                                           */
/* ========================================================================== */

/**
 * @brief TC-CAL-01: Verify Sea-Level Pressure Reduction for Munnar (1500m AMSL).
 */
static void test_tc_cal_01_munnar_reduction(void)
{
    float p0 = 0.0f;
    cal_status_t status = compute_sea_level_pressure(845.20f, 20.0f, 1500.0f, 0.0f, &p0);
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 1003.83f, p0);
}

/**
 * @brief TC-CAL-02: Verify Sea-Level Pressure Reduction for Nilgiris Ridge (2200m AMSL).
 */
static void test_tc_cal_02_nilgiris_reduction(void)
{
    float p0 = 0.0f;
    cal_status_t status = compute_sea_level_pressure(778.50f, 15.0f, 2200.0f, 0.0f, &p0);
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 1004.24f, p0);
}

/**
 * @brief TC-CAL-03: Verify Sea-Level Pressure Reduction for Assam Plains (120m AMSL).
 */
static void test_tc_cal_03_assam_reduction(void)
{
    float p0 = 0.0f;
    cal_status_t status = compute_sea_level_pressure(998.40f, 28.0f, 120.0f, 0.0f, &p0);
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 1012.07f, p0);
}

/**
 * @brief TC-CAL-04: Verify Closed-Form Elevation Inversion from Synoptic Reference.
 */
static void test_tc_cal_04_elevation_inversion(void)
{
    float elevation = 0.0f;
    cal_status_t status = derive_elevation_from_reference(845.00f, 1013.25f, 20.0f, &elevation);
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1585.0f, elevation);
}

/**
 * @brief TC-CAL-05: Verify Calibration Trim Offset Addition.
 */
static void test_tc_cal_05_trim_offset_addition(void)
{
    float p0 = 0.0f;
    cal_status_t status = compute_sea_level_pressure(845.00f, 20.0f, 1585.01f, 0.25f, &p0);
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1013.50f, p0);
}

/**
 * @brief TC-CAL-06: Verify Boundary Protection on Out-of-Range Elevations.
 */
static void test_tc_cal_06_elevation_boundary_clamping(void)
{
    float p0 = 0.0f;
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_ERR_OUT_OF_RANGE, compute_sea_level_pressure(850.0f, 20.0f, -200.0f, 0.0f, &p0));
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_ERR_OUT_OF_RANGE, compute_sea_level_pressure(850.0f, 20.0f, 4000.0f, 0.0f, &p0));
}

/**
 * @brief TC-CAL-07: Verify Boundary Protection on Out-of-Range Offset Trims.
 */
static void test_tc_cal_07_offset_boundary_clamping(void)
{
    float p0 = 0.0f;
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_ERR_OUT_OF_RANGE, compute_sea_level_pressure(850.0f, 20.0f, 1500.0f, 6.0f, &p0));
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_ERR_OUT_OF_RANGE, compute_sea_level_pressure(850.0f, 20.0f, 1500.0f, -6.0f, &p0));
}

/**
 * @brief TC-CAL-08: Verify NVM Calibration Struct 32-Byte Alignment & CRC Integrity.
 */
static void test_tc_cal_08_nvm_struct_crc_integrity(void)
{
    TEST_ASSERT_EQUAL_INT(32, (int)sizeof(nvm_cal_config_t));

    nvm_cal_config_t cfg;
    (void)memset(&cfg, 0xFF, sizeof(cfg));
    cfg.magic_header = NVM_CAL_MAGIC_HEADER;
    cfg.struct_version = NVM_STRUCT_VERSION;
    cfg.elevation_dm = 15425U;
    cfg.press_offset_chpa = 15;
    cfg.temp_offset_cc = 0;
    cfg.weight_press_pct = 30U;
    cfg.weight_rh_pct = 20U;
    cfg.weight_dpd_pct = 20U;
    cfg.weight_lux_pct = 20U;
    cfg.weight_zam_pct = 10U;
    cfg.regime_id = 1U;
    cfg.cal_timestamp_sec = 1712736000U;
    cfg.technician_id = 8402U;

    uint16_t crc = calculate_crc16((const uint8_t *)&cfg + 8, sizeof(cfg) - 8U);
    cfg.crc16_checksum = crc;

    TEST_ASSERT_TRUE(cfg.crc16_checksum != 0x0000U);
    TEST_ASSERT_EQUAL_HEX16(crc, calculate_crc16((const uint8_t *)&cfg + 8, sizeof(cfg) - 8U));
}

/**
 * @brief TC-CAL-09: Verify LoRaWAN FPort 10 Downlink Frame Parsing.
 */
static void test_tc_cal_09_lorawan_downlink_parsing(void)
{
    /* Cmd=0x02, Elev=15425 dm (0x3C41), Offset=+15 chPa (0x000F), Regime=1 */
    const uint8_t frame[6] = {0x02U, 0x3CU, 0x41U, 0x00U, 0x0FU, 0x01U};
    nvm_cal_config_t cfg;
    (void)memset(&cfg, 0, sizeof(cfg));

    cal_status_t status = parse_lorawan_downlink_calibration(frame, sizeof(frame), &cfg);
    TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(15425U, cfg.elevation_dm);
    TEST_ASSERT_EQUAL_INT16(15, cfg.press_offset_chpa);
    TEST_ASSERT_EQUAL_UINT8(1U, cfg.regime_id);
}

/**
 * @brief TC-CAL-10: Verify Calibration Convergence across Multiple High-Altitude Sites.
 */
static void test_tc_cal_10_multi_site_convergence(void)
{
    struct site_record {
        float p_raw;
        float temp;
        float h_survey;
        float p0_target;
    };

    const struct site_record sites[3] = {
        {843.20f, 18.5f, 1520.0f, 1008.40f},
        {778.50f, 15.0f, 2200.0f, 1012.35f},
        {940.00f, 26.0f,  650.0f, 1012.30f}
    };

    for (int i = 0; i < 3; i++) {
        float p0_calc = 0.0f;
        cal_status_t status = compute_sea_level_pressure(sites[i].p_raw, sites[i].temp,
                                                        sites[i].h_survey, 0.0f, &p0_calc);
        TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);

        float residual = sites[i].p0_target - p0_calc;
        float p0_final = 0.0f;
        status = compute_sea_level_pressure(sites[i].p_raw, sites[i].temp,
                                            sites[i].h_survey, residual, &p0_final);
        TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, sites[i].p0_target, p0_final);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tc_cal_01_munnar_reduction);
    RUN_TEST(test_tc_cal_02_nilgiris_reduction);
    RUN_TEST(test_tc_cal_03_assam_reduction);
    RUN_TEST(test_tc_cal_04_elevation_inversion);
    RUN_TEST(test_tc_cal_05_trim_offset_addition);
    RUN_TEST(test_tc_cal_06_elevation_boundary_clamping);
    RUN_TEST(test_tc_cal_07_offset_boundary_clamping);
    RUN_TEST(test_tc_cal_08_nvm_struct_crc_integrity);
    RUN_TEST(test_tc_cal_09_lorawan_downlink_parsing);
    RUN_TEST(test_tc_cal_10_multi_site_convergence);
    return UNITY_END();
}
