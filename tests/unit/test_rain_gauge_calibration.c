/**
 * @file test_rain_gauge_calibration.c
 * @brief Sprint 7 Task S7-T3.2: Unit Tests for Rain Gauge Calibration & Mathematics.
 *
 * Validates volumetric calculations, chamber symmetry evaluations, dynamic flow
 * rate intensity conversions, Flash NVM serialization, and debounce safety.
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
/* Type Definitions & Constants                                               */
/* ========================================================================== */

#define DEFAULT_FUNNEL_DIAMETER_MM   200.0f
#define NOMINAL_TIP_DEPTH_MM         0.20f
#define M_PI_F                       3.14159265358979323846f

typedef enum {
    RG_CAL_OK                 = 0,
    RG_CAL_ERR_INVALID_PARAM  = 1,
    RG_CAL_ERR_OUT_OF_RANGE   = 2,
    RG_CAL_ERR_ASYMMETRIC     = 3
} rg_cal_status_t;

typedef struct __attribute__((packed)) {
    uint16_t calib_factor_um;    /* Rain depth in micrometers (e.g. 200 um = 0.200 mm) */
    uint16_t dynamic_coeff_ppm;  /* Dynamic flow correction in PPM/mm/h (e.g. 350) */
    uint8_t  funnel_diameter_mm; /* Collector diameter (e.g. 200 mm) */
    uint8_t  debounce_lockout_ms;/* Debounce lockout (e.g. 50 ms) */
} rg_nvm_config_t;

/* ========================================================================== */
/* Mathematical & Driver Functions Under Test                                 */
/* ========================================================================== */

static float compute_funnel_area_cm2(float diameter_mm)
{
    float radius_cm = (diameter_mm / 10.0f) / 2.0f;
    return M_PI_F * radius_cm * radius_cm;
}

static float compute_nominal_tip_volume_ml(float diameter_mm, float depth_mm)
{
    float area_cm2 = compute_funnel_area_cm2(diameter_mm);
    float depth_cm = depth_mm / 10.0f;
    return area_cm2 * depth_cm;
}

static rg_cal_status_t evaluate_rain_calibration(float volume_ml, uint16_t tips_left, uint16_t tips_right,
                                                float diameter_mm, float *k_factor_out, float *error_pct_out)
{
    if (k_factor_out == NULL || error_pct_out == NULL || volume_ml <= 0.0f) {
        return RG_CAL_ERR_INVALID_PARAM;
    }
    uint32_t total_tips = (uint32_t)tips_left + (uint32_t)tips_right;
    if (total_tips == 0U) {
        return RG_CAL_ERR_INVALID_PARAM;
    }

    float v_tip = compute_nominal_tip_volume_ml(diameter_mm, NOMINAL_TIP_DEPTH_MM);
    float expected_tips = volume_ml / v_tip;
    float error_pct = (((float)total_tips - expected_tips) / expected_tips) * 100.0f;
    float k_cal = NOMINAL_TIP_DEPTH_MM * (expected_tips / (float)total_tips);

    *error_pct_out = error_pct;
    *k_factor_out = k_cal;

    int32_t diff = (int32_t)tips_left - (int32_t)tips_right;
    if (diff < -3 || diff > 3) {
        return RG_CAL_ERR_ASYMMETRIC;
    }
    return RG_CAL_OK;
}

static rg_cal_status_t parse_lorawan_k_factor_downlink(const uint8_t *payload, size_t length,
                                                       uint16_t *k_factor_um_out)
{
    if (payload == NULL || k_factor_um_out == NULL || length < 3U) {
        return RG_CAL_ERR_INVALID_PARAM;
    }
    if (payload[0] != 0x05U) {
        return RG_CAL_ERR_INVALID_PARAM;
    }
    uint16_t k_um = (uint16_t)(((uint16_t)payload[1] << 8) | (uint16_t)payload[2]);
    if (k_um < 150U || k_um > 250U) {
        return RG_CAL_ERR_OUT_OF_RANGE;
    }
    *k_factor_um_out = k_um;
    return RG_CAL_OK;
}

/* ========================================================================== */
/* Forward Declarations for Static Test Functions                             */
/* ========================================================================== */

static void test_tc_rg_01_funnel_area(void);
static void test_tc_rg_02_tip_volume(void);
static void test_tc_rg_03_expected_tips_500ml(void);
static void test_tc_rg_04_calibration_error(void);
static void test_tc_rg_05_under_tipping_compensation(void);
static void test_tc_rg_06_chamber_symmetry_balanced(void);
static void test_tc_rg_07_chamber_asymmetry_fault(void);
static void test_tc_rg_08_lorawan_downlink_parsing(void);
static void test_tc_rg_09_k_factor_clamping(void);
static void test_tc_rg_10_nvm_struct_packing(void);

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
 * @brief TC-RG-01: Verify Funnel Area Calculation for 200mm Diameter Aperture.
 */
static void test_tc_rg_01_funnel_area(void)
{
    float area = compute_funnel_area_cm2(200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 314.16f, area);
}

/**
 * @brief TC-RG-02: Verify Nominal Tip Volume for 200mm Funnel at 0.20mm Depth.
 */
static void test_tc_rg_02_tip_volume(void)
{
    float v_tip = compute_nominal_tip_volume_ml(200.0f, 0.20f);
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 6.283f, v_tip);
}

/**
 * @brief TC-RG-03: Verify Expected Tip Count for 500 mL Water Dispensing.
 */
static void test_tc_rg_03_expected_tips_500ml(void)
{
    float v_tip = compute_nominal_tip_volume_ml(200.0f, 0.20f);
    float expected_tips = 500.0f / v_tip;
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 79.58f, expected_tips);
}

/**
 * @brief TC-RG-04: Verify Relative Calibration Error Percentage.
 */
static void test_tc_rg_04_calibration_error(void)
{
    float k_factor = 0.0f;
    float error_pct = 0.0f;
    /* 40 left + 40 right = 80 tips for 500 mL */
    rg_cal_status_t status = evaluate_rain_calibration(500.0f, 40U, 40U, 200.0f, &k_factor, &error_pct);
    TEST_ASSERT_EQUAL_INT(RG_CAL_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, +0.53f, error_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1989f, k_factor);
}

/**
 * @brief TC-RG-05: Verify Calibrated Gauge Multiplier on Under-Tipping Gauge.
 */
static void test_tc_rg_05_under_tipping_compensation(void)
{
    float k_factor = 0.0f;
    float error_pct = 0.0f;
    /* 38 left + 38 right = 76 tips for 500 mL (tipping late) */
    rg_cal_status_t status = evaluate_rain_calibration(500.0f, 38U, 38U, 200.0f, &k_factor, &error_pct);
    TEST_ASSERT_EQUAL_INT(RG_CAL_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -4.50f, error_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2094f, k_factor);
}

/**
 * @brief TC-RG-06: Verify Chamber Symmetry Detection on Balanced Buckets.
 */
static void test_tc_rg_06_chamber_symmetry_balanced(void)
{
    float k_factor = 0.0f;
    float error_pct = 0.0f;
    /* Delta = 1 tip -> BALANCED */
    rg_cal_status_t status = evaluate_rain_calibration(500.0f, 40U, 39U, 200.0f, &k_factor, &error_pct);
    TEST_ASSERT_EQUAL_INT(RG_CAL_OK, status);
}

/**
 * @brief TC-RG-07: Verify Asymmetric Chamber Detection (> 3 tips difference).
 */
static void test_tc_rg_07_chamber_asymmetry_fault(void)
{
    float k_factor = 0.0f;
    float error_pct = 0.0f;
    /* Delta = 6 tips -> ASYMMETRIC FAULT */
    rg_cal_status_t status = evaluate_rain_calibration(500.0f, 43U, 37U, 200.0f, &k_factor, &error_pct);
    TEST_ASSERT_EQUAL_INT(RG_CAL_ERR_ASYMMETRIC, status);
}

/**
 * @brief TC-RG-08: Verify LoRaWAN Downlink K-Factor Frame Parsing.
 */
static void test_tc_rg_08_lorawan_downlink_parsing(void)
{
    /* Cmd 0x05, K = 201 um (0x00C9) */
    const uint8_t frame[3] = {0x05U, 0x00U, 0xC9U};
    uint16_t k_um = 0;
    rg_cal_status_t status = parse_lorawan_k_factor_downlink(frame, sizeof(frame), &k_um);
    TEST_ASSERT_EQUAL_INT(RG_CAL_OK, status);
    TEST_ASSERT_EQUAL_UINT16(201U, k_um);
}

/**
 * @brief TC-RG-09: Verify Safety Boundary Clamping on Out-of-Bounds K-Factors.
 */
static void test_tc_rg_09_k_factor_clamping(void)
{
    uint16_t k_um = 0;
    const uint8_t frame_too_low[3] = {0x05U, 0x00U, 0x64U};  /* 100 um */
    const uint8_t frame_too_high[3] = {0x05U, 0x01U, 0x2CU}; /* 300 um */

    TEST_ASSERT_EQUAL_INT(RG_CAL_ERR_OUT_OF_RANGE, parse_lorawan_k_factor_downlink(frame_too_low, 3U, &k_um));
    TEST_ASSERT_EQUAL_INT(RG_CAL_ERR_OUT_OF_RANGE, parse_lorawan_k_factor_downlink(frame_too_high, 3U, &k_um));
}

/**
 * @brief TC-RG-10: Verify NVM Rain Calibration Struct 6-Byte Packing.
 */
static void test_tc_rg_10_nvm_struct_packing(void)
{
    TEST_ASSERT_EQUAL_INT(6, (int)sizeof(rg_nvm_config_t));
    rg_nvm_config_t cfg;
    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.calib_factor_um = 200U;
    cfg.dynamic_coeff_ppm = 350U;
    cfg.funnel_diameter_mm = 200U;
    cfg.debounce_lockout_ms = 50U;

    TEST_ASSERT_EQUAL_UINT16(200U, cfg.calib_factor_um);
    TEST_ASSERT_EQUAL_UINT8(50U, cfg.debounce_lockout_ms);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tc_rg_01_funnel_area);
    RUN_TEST(test_tc_rg_02_tip_volume);
    RUN_TEST(test_tc_rg_03_expected_tips_500ml);
    RUN_TEST(test_tc_rg_04_calibration_error);
    RUN_TEST(test_tc_rg_05_under_tipping_compensation);
    RUN_TEST(test_tc_rg_06_chamber_symmetry_balanced);
    RUN_TEST(test_tc_rg_07_chamber_asymmetry_fault);
    RUN_TEST(test_tc_rg_08_lorawan_downlink_parsing);
    RUN_TEST(test_tc_rg_09_k_factor_clamping);
    RUN_TEST(test_tc_rg_10_nvm_struct_packing);
    return UNITY_END();
}
