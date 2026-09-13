/**
 * @file    test_bsp_adc.c
 * @brief   Unit test verification suite for Battery ADC & Solar Harvesting Telemetry (S3-T4.4).
 * @details Validates internal factory VREFINT calibration, PB1 gated divider timing & standby leakage,
 *          8x oversampling, compensated fixed-point battery millivolts, 8-segment LiFePO4 SoC mapping,
 *          battery health states, adaptive sleep throttling, LoRaWAN Byte 11 bit-packing,
 *          and solar energy harvesting classification.
 */

#include "unity.h"
#include "bsp_adc.h"
#include "power_mgr.h"
#include "bsp_power_rails.h"
#include "board_config.h"

/** @brief Local helper macro for integer tolerance assertion within +/- delta */
#define TEST_ASSERT_UINT16_WITHIN(delta, expected, actual) \
    TEST_ASSERT_TRUE(((uint32_t)(actual) >= ((uint32_t)(expected) - (uint32_t)(delta))) && \
                     ((uint32_t)(actual) <= ((uint32_t)(expected) + (uint32_t)(delta))))

void setUp(void) {
    board_test_reset();
    bsp_power_rails_test_reset();
    bsp_adc_test_reset();
    power_mgr_test_reset();

    (void)board_gpio_init();
    (void)bsp_power_rails_init();
    (void)bsp_adc_init();
    (void)power_mgr_init();
}

void tearDown(void) {
    (void)bsp_adc_deinit();
    (void)bsp_power_rails_all_off();
}

/**
 * @brief TC-S3-T4.4-01: Header Inclusion, Definitions & Initialization Lifecycle.
 */
static void test_bsp_adc_init_and_constants(void) {
    /* Verify hardware constants */
    TEST_ASSERT_EQUAL_UINT32(3000U, BSP_ADC_VREFINT_CAL_VOLTAGE_MV);
    TEST_ASSERT_EQUAL_UINT32(2U, BSP_ADC_VBAT_DIVIDER_SCALE);
    TEST_ASSERT_EQUAL_UINT32(2U, BSP_ADC_DIVIDER_SETTLING_MS);
    TEST_ASSERT_EQUAL_UINT32(8U, BSP_ADC_OVERSAMPLING_COUNT);
    TEST_ASSERT_EQUAL_UINT32(4095U, BSP_ADC_FULL_SCALE_12BIT);
    TEST_ASSERT_EQUAL_UINT16(1660U, BSP_ADC_DEFAULT_VREFINT_CAL);

    /* Verify threshold constants */
    TEST_ASSERT_EQUAL_UINT32(3250U, POWER_BATTERY_OPTIMAL_THRESHOLD_MV);
    TEST_ASSERT_EQUAL_UINT32(3000U, POWER_BATTERY_LOW_THRESHOLD_MV);
    TEST_ASSERT_EQUAL_UINT32(3450U, POWER_BATTERY_FLOAT_THRESHOLD_MV);
    TEST_ASSERT_EQUAL_UINT32(50U, POWER_BATTERY_NIGHT_LUX_THRESHOLD);
    TEST_ASSERT_EQUAL_UINT32(1000U, POWER_BATTERY_HARVEST_LUX_THRESHOLD);

    /* Verify initialized state */
    TEST_ASSERT_TRUE(bsp_adc_test_is_initialized());

    /* De-init and verify */
    status_t status = bsp_adc_deinit();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(bsp_adc_test_is_initialized());

    /* Re-init */
    status = bsp_adc_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(bsp_adc_test_is_initialized());
}

/**
 * @brief TC-S3-T4.4-02: Factory VREFINT_CAL Memory Lookup & Sanity Fallbacks.
 */
static void test_bsp_adc_factory_cal_read(void) {
    /* 1. Default factory calibration */
    TEST_ASSERT_EQUAL_UINT16(1660U, bsp_adc_get_vrefint_factory_cal());

    /* 2. Valid calibration values in [1000, 3000] */
    bsp_adc_test_set_factory_cal(1500U);
    TEST_ASSERT_EQUAL_UINT16(1500U, bsp_adc_get_vrefint_factory_cal());

    bsp_adc_test_set_factory_cal(1850U);
    TEST_ASSERT_EQUAL_UINT16(1850U, bsp_adc_get_vrefint_factory_cal());

    bsp_adc_test_set_factory_cal(1000U);
    TEST_ASSERT_EQUAL_UINT16(1000U, bsp_adc_get_vrefint_factory_cal());

    bsp_adc_test_set_factory_cal(3000U);
    TEST_ASSERT_EQUAL_UINT16(3000U, bsp_adc_get_vrefint_factory_cal());

    /* 3. Out-of-range / corrupted memory fallback (< 1000 or > 3000) */
    bsp_adc_test_set_factory_cal(0U);
    TEST_ASSERT_EQUAL_UINT16(1660U, bsp_adc_get_vrefint_factory_cal());

    bsp_adc_test_set_factory_cal(999U);
    TEST_ASSERT_EQUAL_UINT16(1660U, bsp_adc_get_vrefint_factory_cal());

    bsp_adc_test_set_factory_cal(3001U);
    TEST_ASSERT_EQUAL_UINT16(1660U, bsp_adc_get_vrefint_factory_cal());

    bsp_adc_test_set_factory_cal(65535U);
    TEST_ASSERT_EQUAL_UINT16(1660U, bsp_adc_get_vrefint_factory_cal());
}

/**
 * @brief TC-S3-T4.4-03: Battery Divider Gating Timing & Zero-Standby Leakage.
 */
static void test_bsp_adc_divider_gating_timing(void) {
    uint16_t raw_vbat = 0;

    /* Verify standby state: divider rail disabled, PB1 HIGH (inactive) */
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE));

    uint32_t delay_calls_before = bsp_power_rails_test_get_delay_calls();

    /* Perform raw battery acquisition */
    status_t status = bsp_adc_read_vbat_raw(&raw_vbat);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(2048U, raw_vbat);

    /* Verify that 2 ms RC stabilization delay was executed */
    TEST_ASSERT_EQUAL_UINT32(delay_calls_before + 1U, bsp_power_rails_test_get_delay_calls());
    TEST_ASSERT_EQUAL_UINT32(BSP_ADC_DIVIDER_SETTLING_MS, bsp_power_rails_test_get_last_delay_ms());

    /* Verify post-conversion state: divider rail safely disabled, PB1 HIGH */
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE));
}

/**
 * @brief TC-S3-T4.4-04: Battery Voltage Calculation & Supply Compensation Accuracy.
 */
static void test_bsp_adc_vbat_mv_accuracy(void) {
    uint16_t vbat_mv = 0;

    /* Case A: Nominal 3.0V VDDA (Cal = 1660, Raw Vref = 1660) */
    bsp_adc_test_set_factory_cal(1660U);
    bsp_adc_test_set_vrefint_raw(1660U);

    /* Raw VBAT = 2048 -> Vbat = (2 * 3000 * 1660 * 2048) / (4095 * 1660) = 3000.73 -> 3001 mV */
    bsp_adc_test_set_vbat_raw(2048U);
    status_t status = bsp_adc_read_vbat_mv(&vbat_mv);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_UINT16_WITHIN(15U, 3000U, vbat_mv);

    /* 3.30V Battery: Raw VBAT = (3300 * 4095) / 6000 = 2252 */
    bsp_adc_test_set_vbat_raw(2252U);
    status = bsp_adc_read_vbat_mv(&vbat_mv);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_UINT16_WITHIN(15U, 3300U, vbat_mv);

    /* 2.80V Battery: Raw VBAT = (2800 * 4095) / 6000 = 1911 */
    bsp_adc_test_set_vbat_raw(1911U);
    status = bsp_adc_read_vbat_mv(&vbat_mv);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_UINT16_WITHIN(15U, 2800U, vbat_mv);

    /* 3.65V Battery (Full Charge): Raw VBAT = (3650 * 4095) / 6000 = 2491 */
    bsp_adc_test_set_vbat_raw(2491U);
    status = bsp_adc_read_vbat_mv(&vbat_mv);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_UINT16_WITHIN(15U, 3650U, vbat_mv);

    /* Case B: VDDA Drift Compensation (VDDA = 3.30V -> Raw Vref = (3000 * 1660)/3300 = 1509) */
    bsp_adc_test_set_vrefint_raw(1509U);
    /* At VDDA=3.30V and Vbat=3.30V, Vadc=1.65V -> Raw VBAT = (1.65 / 3.30) * 4095 = 2048 */
    bsp_adc_test_set_vbat_raw(2048U);
    status = bsp_adc_read_vbat_mv(&vbat_mv);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_UINT16_WITHIN(15U, 3300U, vbat_mv);
}

/**
 * @brief TC-S3-T4.4-05: LiFePO4 8-Segment Non-Linear State of Charge (SoC) Mapping.
 */
static void test_power_mgr_battery_soc_mapping(void) {
    /* Below Hardware Cutoff (< 2500 mV) */
    TEST_ASSERT_EQUAL_UINT8(0U, power_mgr_battery_calc_soc(0U));
    TEST_ASSERT_EQUAL_UINT8(0U, power_mgr_battery_calc_soc(2000U));
    TEST_ASSERT_EQUAL_UINT8(0U, power_mgr_battery_calc_soc(2499U));

    /* Segment 0: 2500 mV - 2999 mV (0% to 4%) */
    TEST_ASSERT_EQUAL_UINT8(0U, power_mgr_battery_calc_soc(2500U));
    TEST_ASSERT_EQUAL_UINT8(3U, power_mgr_battery_calc_soc(2800U));
    TEST_ASSERT_EQUAL_UINT8(4U, power_mgr_battery_calc_soc(2999U));

    /* Segment 1: 3000 mV - 3099 mV (5% to 9%) */
    TEST_ASSERT_EQUAL_UINT8(5U, power_mgr_battery_calc_soc(3000U));
    TEST_ASSERT_EQUAL_UINT8(7U, power_mgr_battery_calc_soc(3050U));
    TEST_ASSERT_EQUAL_UINT8(9U, power_mgr_battery_calc_soc(3099U));

    /* Segment 2: 3100 mV - 3199 mV (10% to 19%) */
    TEST_ASSERT_EQUAL_UINT8(10U, power_mgr_battery_calc_soc(3100U));
    TEST_ASSERT_EQUAL_UINT8(15U, power_mgr_battery_calc_soc(3150U));
    TEST_ASSERT_EQUAL_UINT8(19U, power_mgr_battery_calc_soc(3199U));

    /* Segment 3: 3200 mV - 3249 mV (20% to 39%) */
    TEST_ASSERT_EQUAL_UINT8(20U, power_mgr_battery_calc_soc(3200U));
    TEST_ASSERT_EQUAL_UINT8(30U, power_mgr_battery_calc_soc(3225U));
    TEST_ASSERT_EQUAL_UINT8(39U, power_mgr_battery_calc_soc(3249U));

    /* Segment 4: 3250 mV - 3299 mV (40% to 69%) */
    TEST_ASSERT_EQUAL_UINT8(40U, power_mgr_battery_calc_soc(3250U));
    TEST_ASSERT_EQUAL_UINT8(55U, power_mgr_battery_calc_soc(3275U));
    TEST_ASSERT_EQUAL_UINT8(69U, power_mgr_battery_calc_soc(3299U));

    /* Segment 5: 3300 mV - 3329 mV (70% to 89%) */
    TEST_ASSERT_EQUAL_UINT8(70U, power_mgr_battery_calc_soc(3300U));
    TEST_ASSERT_EQUAL_UINT8(80U, power_mgr_battery_calc_soc(3315U));
    TEST_ASSERT_EQUAL_UINT8(89U, power_mgr_battery_calc_soc(3329U));

    /* Segment 6: 3330 mV - 3399 mV (90% to 99%) */
    TEST_ASSERT_EQUAL_UINT8(90U, power_mgr_battery_calc_soc(3330U));
    TEST_ASSERT_EQUAL_UINT8(95U, power_mgr_battery_calc_soc(3365U));
    TEST_ASSERT_EQUAL_UINT8(99U, power_mgr_battery_calc_soc(3399U));

    /* Segment 7: >= 3400 mV (100% Clamped) */
    TEST_ASSERT_EQUAL_UINT8(100U, power_mgr_battery_calc_soc(3400U));
    TEST_ASSERT_EQUAL_UINT8(100U, power_mgr_battery_calc_soc(3650U));
    TEST_ASSERT_EQUAL_UINT8(100U, power_mgr_battery_calc_soc(4200U));

    /* Verify strict monotonic non-decreasing property across voltage range */
    uint8_t prev_soc = 0;
    for (uint16_t mv = 2000U; mv <= 3700U; mv += 25U) {
        uint8_t curr_soc = power_mgr_battery_calc_soc(mv);
        TEST_ASSERT_TRUE(curr_soc >= prev_soc);
        TEST_ASSERT_TRUE(curr_soc <= 100U);
        prev_soc = curr_soc;
    }
}

/**
 * @brief TC-S3-T4.4-06: Battery Health Categorization & Throttling Flags.
 */
static void test_power_mgr_battery_health_categorization(void) {
    bsp_adc_test_set_factory_cal(1660U);
    bsp_adc_test_set_vrefint_raw(1660U);

    /* 1. Optimal Battery (Vbat = 3300 mV >= 3250 mV) */
    bsp_adc_test_set_vbat_raw(2252U); /* 3300 mV */
    status_t status = power_mgr_battery_update(5000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    const power_battery_status_t *p_status = power_mgr_battery_get_status();
    TEST_ASSERT_NOT_NULL(p_status);
    TEST_ASSERT_EQUAL_INT(POWER_BATTERY_HEALTH_OPTIMAL, p_status->health);
    TEST_ASSERT_EQUAL_INT(POWER_BATTERY_HEALTH_OPTIMAL, power_mgr_battery_get_health());
    TEST_ASSERT_FALSE(p_status->throttling_active);
    TEST_ASSERT_FALSE(power_mgr_battery_is_throttling_required());
    TEST_ASSERT_UINT16_WITHIN(15U, 3300U, p_status->vbat_mv);

    /* 2. Low Battery (Vbat = 3100 mV: 3000 mV <= Vbat < 3250 mV) */
    bsp_adc_test_set_vbat_raw(2116U); /* ~3100 mV */
    status = power_mgr_battery_update(5000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    p_status = power_mgr_battery_get_status();
    TEST_ASSERT_EQUAL_INT(POWER_BATTERY_HEALTH_LOW, p_status->health);
    TEST_ASSERT_EQUAL_INT(POWER_BATTERY_HEALTH_LOW, power_mgr_battery_get_health());
    TEST_ASSERT_TRUE(p_status->throttling_active);
    TEST_ASSERT_TRUE(power_mgr_battery_is_throttling_required());

    /* 3. Critical Battery (Vbat = 2900 mV < 3000 mV) */
    bsp_adc_test_set_vbat_raw(1979U); /* ~2900 mV */
    status = power_mgr_battery_update(5000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    p_status = power_mgr_battery_get_status();
    TEST_ASSERT_EQUAL_INT(POWER_BATTERY_HEALTH_CRITICAL, p_status->health);
    TEST_ASSERT_EQUAL_INT(POWER_BATTERY_HEALTH_CRITICAL, power_mgr_battery_get_health());
    TEST_ASSERT_TRUE(p_status->throttling_active);
    TEST_ASSERT_TRUE(power_mgr_battery_is_throttling_required());
}

/**
 * @brief TC-S3-T4.4-07: Power-Adjusted Sleep Throttling Recommendations.
 */
static void test_power_mgr_battery_sleep_throttling(void) {
    bsp_adc_test_set_factory_cal(1660U);
    bsp_adc_test_set_vrefint_raw(1660U);

    /* 1. Optimal Battery (Vbat = 3300 mV): Unchanged sleep durations */
    bsp_adc_test_set_vbat_raw(2252U);
    (void)power_mgr_battery_update(5000U);

    TEST_ASSERT_EQUAL_UINT32(600U, power_mgr_battery_get_recommended_sleep_sec(600U));
    TEST_ASSERT_EQUAL_UINT32(300U, power_mgr_battery_get_recommended_sleep_sec(300U));
    TEST_ASSERT_EQUAL_UINT32(120U, power_mgr_battery_get_recommended_sleep_sec(120U));

    /* 2. Low Battery (Vbat = 3100 mV): Clamped to minimum 900s (15 min) */
    bsp_adc_test_set_vbat_raw(2116U);
    (void)power_mgr_battery_update(5000U);

    TEST_ASSERT_EQUAL_UINT32(900U, power_mgr_battery_get_recommended_sleep_sec(120U));
    TEST_ASSERT_EQUAL_UINT32(900U, power_mgr_battery_get_recommended_sleep_sec(300U));
    TEST_ASSERT_EQUAL_UINT32(900U, power_mgr_battery_get_recommended_sleep_sec(600U));
    TEST_ASSERT_EQUAL_UINT32(1200U, power_mgr_battery_get_recommended_sleep_sec(1200U));

    /* 3. Critical Battery (Vbat = 2900 mV): Forced to 3600s (60 min) */
    bsp_adc_test_set_vbat_raw(1979U);
    (void)power_mgr_battery_update(5000U);

    TEST_ASSERT_EQUAL_UINT32(3600U, power_mgr_battery_get_recommended_sleep_sec(120U));
    TEST_ASSERT_EQUAL_UINT32(3600U, power_mgr_battery_get_recommended_sleep_sec(600U));
    TEST_ASSERT_EQUAL_UINT32(3600U, power_mgr_battery_get_recommended_sleep_sec(1800U));
    TEST_ASSERT_EQUAL_UINT32(3600U, power_mgr_battery_get_recommended_sleep_sec(7200U));
}

/**
 * @brief TC-S3-T4.4-08: LoRaWAN Telemetry Byte 11 Bit-Packing.
 */
static void test_power_mgr_battery_payload_encoding(void) {
    bsp_adc_test_set_factory_cal(1660U);
    bsp_adc_test_set_vrefint_raw(1660U);

    /* 1. Vbat = 3300 mV -> Raw 6-bit = (3300 - 2500) / 20 = 40 (0x28) */
    bsp_adc_test_set_vbat_raw(2252U); /* 3300 mV */
    (void)power_mgr_battery_update(5000U);

    /* No flags: Byte 11 = 0x28 */
    uint8_t byte11 = power_mgr_battery_encode_payload_byte(false, false);
    TEST_ASSERT_EQUAL_HEX8(0x28U, byte11);

    /* Sensor error flag (Bit 6 = 1): 0x28 | 0x40 = 0x68 (matches TC-S3-T4.4-08 spec) */
    byte11 = power_mgr_battery_encode_payload_byte(true, false);
    TEST_ASSERT_EQUAL_HEX8(0x68U, byte11);

    /* Unexpected reset flag (Bit 7 = 1): 0x28 | 0x80 = 0xA8 */
    byte11 = power_mgr_battery_encode_payload_byte(false, true);
    TEST_ASSERT_EQUAL_HEX8(0xA8U, byte11);

    /* Both flags set: 0x28 | 0xC0 = 0xE8 */
    byte11 = power_mgr_battery_encode_payload_byte(true, true);
    TEST_ASSERT_EQUAL_HEX8(0xE8U, byte11);

    /* 2. Lower bound: Vbat = 2500 mV -> Raw 6-bit = 0 */
    bsp_adc_test_set_vbat_raw(1706U); /* 2500 mV */
    (void)power_mgr_battery_update(5000U);
    byte11 = power_mgr_battery_encode_payload_byte(false, false);
    TEST_ASSERT_EQUAL_HEX8(0x00U, byte11);

    /* 3. Underflow bound: Vbat = 2400 mV (< 2500 mV) -> Clamped to 0 */
    bsp_adc_test_set_vbat_raw(1638U); /* 2400 mV */
    (void)power_mgr_battery_update(5000U);
    byte11 = power_mgr_battery_encode_payload_byte(false, false);
    TEST_ASSERT_EQUAL_HEX8(0x00U, byte11);

    /* 4. Upper bound: Vbat = 3760 mV -> Raw 6-bit = 63 (0x3F) */
    bsp_adc_test_set_vbat_raw(2566U); /* 3760 mV */
    (void)power_mgr_battery_update(5000U);
    byte11 = power_mgr_battery_encode_payload_byte(false, false);
    TEST_ASSERT_EQUAL_HEX8(0x3FU, byte11);

    /* 5. Overflow bound: Vbat = 4000 mV (> 3760 mV) -> Clamped to 63 */
    bsp_adc_test_set_vbat_raw(2730U); /* 4000 mV */
    (void)power_mgr_battery_update(5000U);
    byte11 = power_mgr_battery_encode_payload_byte(true, true);
    TEST_ASSERT_EQUAL_HEX8(0xFFU, byte11);
}

/**
 * @brief TC-S3-T4.4-09: Solar Harvesting Telemetry Classification.
 */
static void test_power_mgr_solar_harvesting_classification(void) {
    bsp_adc_test_set_factory_cal(1660U);
    bsp_adc_test_set_vrefint_raw(1660U);

    /* 1. Night condition (Lux < 50): Regardless of voltage */
    bsp_adc_test_set_vbat_raw(2252U); /* 3300 mV */
    (void)power_mgr_battery_update(20U); /* 20 lux */
    const power_battery_status_t *p_status = power_mgr_battery_get_status();
    TEST_ASSERT_EQUAL_INT(SOLAR_STATUS_NIGHT, p_status->solar_status);

    /* 2. Float Charged condition (Vbat >= 3450 mV and Lux >= 50) */
    bsp_adc_test_set_vbat_raw(2360U); /* ~3460 mV */
    (void)power_mgr_battery_update(5000U);
    p_status = power_mgr_battery_get_status();
    TEST_ASSERT_EQUAL_INT(SOLAR_STATUS_FLOAT_CHARGED, p_status->solar_status);

    /* 3. Active Harvesting (Lux >= 1000 and delta Vbat > 0) */
    bsp_adc_test_set_vbat_raw(2252U); /* 3300 mV */
    (void)power_mgr_battery_update(20000U);

    bsp_adc_test_set_vbat_raw(2270U); /* 3326 mV (> 3300 mV) */
    (void)power_mgr_battery_update(25000U);
    p_status = power_mgr_battery_get_status();
    TEST_ASSERT_EQUAL_INT(SOLAR_STATUS_ACTIVE_HARVEST, p_status->solar_status);
    TEST_ASSERT_TRUE(p_status->delta_vbat_mv_per_hr > 0);

    /* 4. Daytime Discharging (Lux >= 50, but delta Vbat <= 0 or Lux < 1000) */
    bsp_adc_test_set_vbat_raw(2240U); /* 3282 mV (< 3326 mV) */
    (void)power_mgr_battery_update(1500U);
    p_status = power_mgr_battery_get_status();
    TEST_ASSERT_EQUAL_INT(SOLAR_STATUS_DISCHARGING, p_status->solar_status);
}

/**
 * @brief TC-S3-T4.4-10: Defensive Error Handling, Injected Faults & Fail-Safe Rails.
 */
static void test_bsp_adc_defensive_error_handling(void) {
    /* 1. NULL pointer checks */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bsp_adc_read_vrefint_raw(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bsp_adc_read_vbat_raw(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bsp_adc_read_vbat_mv(NULL));

    /* 2. Conversion count tracking */
    uint32_t count_before = bsp_adc_test_get_conversion_count();
    uint16_t vbat_mv = 0;
    status_t status = bsp_adc_read_vbat_mv(&vbat_mv);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    /* 8x VREFINT + 8x VBAT = 16 conversions */
    TEST_ASSERT_EQUAL_UINT32(count_before + 16U, bsp_adc_test_get_conversion_count());

    /* 3. Injected hardware timeout error during raw VBAT read */
    bsp_adc_test_inject_error(STATUS_ERR_TIMEOUT);
    uint16_t raw_vbat = 0;
    status = bsp_adc_read_vbat_raw(&raw_vbat);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);

    /* Verify PB1 is safely de-asserted (HIGH) despite error */
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE));

    /* 4. Injected error during power_mgr_battery_update */
    bsp_adc_test_inject_error(STATUS_ERR_TIMEOUT);
    status = power_mgr_battery_update(5000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bsp_adc_init_and_constants);
    RUN_TEST(test_bsp_adc_factory_cal_read);
    RUN_TEST(test_bsp_adc_divider_gating_timing);
    RUN_TEST(test_bsp_adc_vbat_mv_accuracy);
    RUN_TEST(test_power_mgr_battery_soc_mapping);
    RUN_TEST(test_power_mgr_battery_health_categorization);
    RUN_TEST(test_power_mgr_battery_sleep_throttling);
    RUN_TEST(test_power_mgr_battery_payload_encoding);
    RUN_TEST(test_power_mgr_solar_harvesting_classification);
    RUN_TEST(test_bsp_adc_defensive_error_handling);

    return UNITY_END();
}
