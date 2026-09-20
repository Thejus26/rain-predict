/**
 * @file    test_app_fault_handler.c
 * @brief   Unity unit test suite for Graceful Degradation & Fault Tolerance Engine.
 * @details Validates fault bitmask tracking, 3-cycle self-healing auto-clearing,
 *          autonomous I2C bus recovery, BME280 stale hold & neutral fallback baselines,
 *          and OPT3001 solar fallback heuristics.
 */

#include "unity.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_fault_handler.h"
#include "i2c_bus.h"
#include "bsp_power_rails.h"
#include "status.h"

/* ========================================================================== */
/* Setup & Teardown                                                           */
/* ========================================================================== */

void setUp(void) {
    bsp_power_rails_test_reset();
    (void)bsp_power_rails_init();
    i2c_bus_test_reset();
    (void)i2c_bus_init(I2C_BUS_SPEED_FAST_HZ);
    app_fault_handler_reset();
}

void tearDown(void) {
    app_fault_handler_reset();
    (void)i2c_bus_deinit();
    i2c_bus_test_reset();
}

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

/**
 * @brief Test initialization default status values and defensive NULL pointer guards.
 */
static void test_fault_handler_init_defaults(void) {
    fault_handler_status_t status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));

    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_NONE, status.active_fault_mask);
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_NONE, status.latched_fault_mask);
    TEST_ASSERT_EQUAL_UINT32(0U, status.bme280_fail_count);
    TEST_ASSERT_EQUAL_UINT32(0U, status.opt3001_fail_count);
    TEST_ASSERT_EQUAL_UINT32(0U, status.i2c_lockup_count);
    TEST_ASSERT_EQUAL_UINT32(0U, status.lora_timeout_count);
    TEST_ASSERT_EQUAL_UINT32(0U, status.flash_error_count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.bme280_consecutive_clean);
    TEST_ASSERT_EQUAL_UINT8(0U, status.opt3001_consecutive_clean);
    TEST_ASSERT_EQUAL_UINT8(0U, status.pressure_stale_count);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 950.0f, status.last_valid_pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, status.last_valid_temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f, status.last_valid_humidity_pct);

    TEST_ASSERT_FALSE(app_fault_handler_is_system_fault_active());

    /* Defensive NULL pointer check */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, app_fault_handler_get_status(NULL));
}

/**
 * @brief Test BME280 fault latching, error metric accumulation, and 3-cycle self-healing.
 */
static void test_fault_handler_bme280_fault_and_self_healing(void) {
    fault_handler_status_t status;

    /* Report BME280 read failure */
    app_fault_handler_report(FAULT_MASK_BME280_COMM, false);
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_BME280_COMM, status.active_fault_mask & FAULT_MASK_BME280_COMM);
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_BME280_COMM, status.latched_fault_mask & FAULT_MASK_BME280_COMM);
    TEST_ASSERT_EQUAL_UINT32(1U, status.bme280_fail_count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.bme280_consecutive_clean);

    /* 1st clean read: fault remains active */
    app_fault_handler_report(FAULT_MASK_BME280_COMM, true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(1U, status.bme280_consecutive_clean);
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());

    /* 2nd clean read: fault still remains active */
    app_fault_handler_report(FAULT_MASK_BME280_COMM, true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(2U, status.bme280_consecutive_clean);
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());

    /* 3rd clean read: auto-clear triggers! */
    app_fault_handler_report(FAULT_MASK_BME280_COMM, true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(3U, status.bme280_consecutive_clean);
    TEST_ASSERT_FALSE(app_fault_handler_is_system_fault_active());
    TEST_ASSERT_EQUAL_HEX16(0U, status.active_fault_mask & FAULT_MASK_BME280_COMM);
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_BME280_COMM, status.latched_fault_mask & FAULT_MASK_BME280_COMM);

    /* Verify interrupted clean cycle: clean count resets on failure */
    app_fault_handler_report(FAULT_MASK_BME280_COMM, false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(0U, status.bme280_consecutive_clean);
    TEST_ASSERT_EQUAL_UINT32(2U, status.bme280_fail_count);
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());
}

/**
 * @brief Test OPT3001 fault latching and 3-cycle self-healing.
 */
static void test_fault_handler_opt3001_fault_and_self_healing(void) {
    fault_handler_status_t status;

    /* Report OPT3001 failure */
    app_fault_handler_report(FAULT_MASK_OPT3001_COMM, false);
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_OPT3001_COMM, status.active_fault_mask & FAULT_MASK_OPT3001_COMM);
    TEST_ASSERT_EQUAL_UINT32(1U, status.opt3001_fail_count);

    /* 2 clean reads: still active */
    app_fault_handler_report(FAULT_MASK_OPT3001_COMM, true);
    app_fault_handler_report(FAULT_MASK_OPT3001_COMM, true);
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());

    /* 3rd clean read: auto-cleared */
    app_fault_handler_report(FAULT_MASK_OPT3001_COMM, true);
    TEST_ASSERT_FALSE(app_fault_handler_is_system_fault_active());
}

/**
 * @brief Test peripheral and battery fault mask reporting without false system fault trigger.
 */
static void test_fault_handler_peripheral_faults(void) {
    fault_handler_status_t status;

    /* Flash write fault */
    app_fault_handler_report(FAULT_MASK_FLASH_WRITE, false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT32(1U, status.flash_error_count);
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_FLASH_WRITE, status.active_fault_mask & FAULT_MASK_FLASH_WRITE);
    /* Flash fault alone does not assert system critical sensor fault */
    TEST_ASSERT_FALSE(app_fault_handler_is_system_fault_active());

    /* LoRa TX timeout fault */
    app_fault_handler_report(FAULT_MASK_LORA_TX_TIMEOUT, false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT32(1U, status.lora_timeout_count);
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_LORA_TX_TIMEOUT, status.active_fault_mask & FAULT_MASK_LORA_TX_TIMEOUT);

    /* Rain chatter and Battery flags */
    app_fault_handler_report(FAULT_MASK_RAIN_GAUGE_CHATTER, false);
    app_fault_handler_report(FAULT_MASK_BATTERY_LOW, false);
    app_fault_handler_report(FAULT_MASK_BATTERY_CRITICAL, false);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_TRUE((status.active_fault_mask & FAULT_MASK_RAIN_GAUGE_CHATTER) != 0U);
    TEST_ASSERT_TRUE((status.active_fault_mask & FAULT_MASK_BATTERY_LOW) != 0U);
    TEST_ASSERT_TRUE((status.active_fault_mask & FAULT_MASK_BATTERY_CRITICAL) != 0U);

    /* Clear non-healing faults directly via success report */
    app_fault_handler_report(FAULT_MASK_FLASH_WRITE, true);
    app_fault_handler_report(FAULT_MASK_LORA_TX_TIMEOUT, true);
    app_fault_handler_report(FAULT_MASK_RAIN_GAUGE_CHATTER, true);
    app_fault_handler_report(FAULT_MASK_BATTERY_LOW, true);
    app_fault_handler_report(FAULT_MASK_BATTERY_CRITICAL, true);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_NONE, status.active_fault_mask);
}

/**
 * @brief Test BME280 fallback: holds last valid values for 3 cycles, then defaults to neutral baseline.
 */
static void test_fault_handler_bme280_fallback_stale_and_neutral(void) {
    float temp = 0.0f;
    float rh   = 0.0f;
    float p    = 0.0f;

    /* Set last known good reading */
    app_fault_handler_set_last_valid_bme280(24.5f, 65.0f, 962.3f);

    /* Cycle 1 of fallback: stale count becomes 1, returns true */
    bool available = app_fault_handler_get_bme280_fallback(&temp, &rh, &p);
    TEST_ASSERT_TRUE(available);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, rh);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 962.3f, p);

    /* Cycle 2 of fallback: stale count becomes 2, returns true */
    available = app_fault_handler_get_bme280_fallback(&temp, &rh, &p);
    TEST_ASSERT_TRUE(available);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 962.3f, p);

    /* Cycle 3 of fallback: stale count becomes 3, returns true */
    available = app_fault_handler_get_bme280_fallback(&temp, &rh, &p);
    TEST_ASSERT_TRUE(available);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 962.3f, p);

    /* Cycle 4 of fallback: stale window (3 cycles) expired! Returns false, neutral defaults applied */
    available = app_fault_handler_get_bme280_fallback(&temp, &rh, &p);
    TEST_ASSERT_FALSE(available);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, FAULT_TEMP_NEUTRAL_DEFAULT_C, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, FAULT_HUM_NEUTRAL_DEFAULT_PCT, rh);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, FAULT_PRESSURE_NEUTRAL_DEFAULT_HPA, p);

    /* Defensive NULL pointer checks */
    TEST_ASSERT_FALSE(app_fault_handler_get_bme280_fallback(NULL, &rh, &p));
    TEST_ASSERT_FALSE(app_fault_handler_get_bme280_fallback(&temp, NULL, &p));
    TEST_ASSERT_FALSE(app_fault_handler_get_bme280_fallback(&temp, &rh, NULL));

    /* Resetting last valid reading restores cache */
    app_fault_handler_set_last_valid_bme280(21.0f, 75.0f, 955.0f);
    available = app_fault_handler_get_bme280_fallback(&temp, &rh, &p);
    TEST_ASSERT_TRUE(available);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.0f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 955.0f, p);
}

/**
 * @brief Test OPT3001 solar fallback: daylight hours produce 25,000 Lux, nighttime produces 0 Lux.
 */
static void test_fault_handler_opt3001_fallback_heuristics(void) {
    float lux = 999.0f;

    /* Midday (hour 12) -> Daytime estimate (25,000 Lux) */
    app_fault_handler_get_opt3001_fallback(12U, &lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_LUX_DAYTIME_ESTIMATE, lux);

    /* Dawn boundary (hour 6) -> Daytime estimate */
    app_fault_handler_get_opt3001_fallback(6U, &lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_LUX_DAYTIME_ESTIMATE, lux);

    /* Dusk boundary (hour 18) -> Daytime estimate */
    app_fault_handler_get_opt3001_fallback(18U, &lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_LUX_DAYTIME_ESTIMATE, lux);

    /* Night (hour 19) -> 0 Lux */
    app_fault_handler_get_opt3001_fallback(19U, &lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_LUX_NIGHTTIME_ESTIMATE, lux);

    /* Midnight (hour 0) -> 0 Lux */
    app_fault_handler_get_opt3001_fallback(0U, &lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_LUX_NIGHTTIME_ESTIMATE, lux);

    /* Pre-dawn (hour 5) -> 0 Lux */
    app_fault_handler_get_opt3001_fallback(5U, &lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_LUX_NIGHTTIME_ESTIMATE, lux);

    /* Defensive NULL pointer guard */
    app_fault_handler_get_opt3001_fallback(12U, NULL);
}

/**
 * @brief Test autonomous I2C bus lockup recovery.
 */
static void test_fault_handler_i2c_recovery(void) {
    fault_handler_status_t status;

    /* Simulate SDA stuck low by slave */
    i2c_bus_test_set_sda_stuck(true);
    TEST_ASSERT_TRUE(i2c_bus_test_get_sda_stuck());

    /* Execute autonomous recovery */
    status_t rc = app_fault_handler_recover_i2c_bus();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, rc);
    TEST_ASSERT_FALSE(i2c_bus_test_get_sda_stuck());

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&status));
    TEST_ASSERT_EQUAL_UINT32(1U, status.i2c_lockup_count);
    TEST_ASSERT_EQUAL_HEX16(FAULT_MASK_I2C_BUS_LOCKUP, status.latched_fault_mask & FAULT_MASK_I2C_BUS_LOCKUP);
    TEST_ASSERT_EQUAL_HEX16(0U, status.active_fault_mask & FAULT_MASK_I2C_BUS_LOCKUP);
    TEST_ASSERT_FALSE(app_fault_handler_is_system_fault_active());
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fault_handler_init_defaults);
    RUN_TEST(test_fault_handler_bme280_fault_and_self_healing);
    RUN_TEST(test_fault_handler_opt3001_fault_and_self_healing);
    RUN_TEST(test_fault_handler_peripheral_faults);
    RUN_TEST(test_fault_handler_bme280_fallback_stale_and_neutral);
    RUN_TEST(test_fault_handler_opt3001_fallback_heuristics);
    RUN_TEST(test_fault_handler_i2c_recovery);
    return UNITY_END();
}
