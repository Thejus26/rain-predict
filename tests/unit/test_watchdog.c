/**
 * @file    test_watchdog.c
 * @brief   Unit test verification suite for Independent Watchdog (IWDG) Driver & Supervision (S3-T4.3).
 * @details Validates 8.0s timeout initialization, anti-masking safe refresh counters, boot reset reason
 *          flag diagnostics (RCC_CSR_IWDGRSTF), parameter validation, and timing calculation invariants.
 */

#include "unity.h"
#include "watchdog.h"
#include "status.h"

void setUp(void) {
    watchdog_test_reset();
}

void tearDown(void) {
    watchdog_test_reset();
}

/**
 * @brief TC-S3-T4.3-01: Header Inclusion, Constants & Timing Calculation Invariants.
 */
static void test_watchdog_constants_and_timing_verification(void) {
    /* Verify clock & timing macros */
    TEST_ASSERT_EQUAL_UINT32(8000UL,  WATCHDOG_TIMEOUT_MS_DEFAULT);
    TEST_ASSERT_EQUAL_UINT32(100UL,   WATCHDOG_TIMEOUT_MS_MIN);
    TEST_ASSERT_EQUAL_UINT32(32768UL, WATCHDOG_TIMEOUT_MS_MAX);
    TEST_ASSERT_EQUAL_UINT32(32000UL, WATCHDOG_LSI_FREQ_HZ);
    TEST_ASSERT_EQUAL_UINT32(64UL,    WATCHDOG_PRESCALER_DIV);
    TEST_ASSERT_EQUAL_UINT32(4000UL,  WATCHDOG_RELOAD_VALUE);

    /* Verify 8.0s mathematical formula: (Reload * Prescaler) / LSI = 8.00s */
    uint32_t calc_timeout_ms = (WATCHDOG_RELOAD_VALUE * WATCHDOG_PRESCALER_DIV * 1000UL) / WATCHDOG_LSI_FREQ_HZ;
    TEST_ASSERT_EQUAL_UINT32(8000UL, calc_timeout_ms);
}

/**
 * @brief TC-S3-T4.3-02: Default Watchdog Initialization (8.0s).
 */
static void test_watchdog_init_default(void) {
    TEST_ASSERT_FALSE(watchdog_is_enabled());

    /* Initialize with 0 -> should default to 8000 ms */
    status_t status = watchdog_init(0U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(watchdog_is_enabled());
    TEST_ASSERT_EQUAL_UINT32(8000UL, watchdog_get_timeout_ms());
}

/**
 * @brief TC-S3-T4.3-03: Custom Valid Timeout Initialization.
 */
static void test_watchdog_init_custom_timeouts(void) {
    /* Test 1000 ms (1.0s) */
    status_t status = watchdog_init(1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(watchdog_is_enabled());
    TEST_ASSERT_EQUAL_UINT32(1000UL, watchdog_get_timeout_ms());

    /* Test 4000 ms (4.0s) */
    status = watchdog_init(4000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(4000UL, watchdog_get_timeout_ms());

    /* Test Minimum boundary: 100 ms */
    status = watchdog_init(WATCHDOG_TIMEOUT_MS_MIN);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(WATCHDOG_TIMEOUT_MS_MIN, watchdog_get_timeout_ms());

    /* Test Maximum boundary: 32768 ms */
    status = watchdog_init(WATCHDOG_TIMEOUT_MS_MAX);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(WATCHDOG_TIMEOUT_MS_MAX, watchdog_get_timeout_ms());
}

/**
 * @brief TC-S3-T4.3-04: Parameter Validation & Out-of-Range Handling.
 */
static void test_watchdog_init_invalid_params(void) {
    /* Below minimum (< 100 ms) */
    status_t status = watchdog_init(50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);

    /* Above maximum (> 32768 ms) */
    status = watchdog_init(40000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);
}

/**
 * @brief TC-S3-T4.3-05: Watchdog Counter Refresh Tracking.
 */
static void test_watchdog_refresh_counter(void) {
    /* Before initialization: refresh should have no effect */
    watchdog_refresh();
    TEST_ASSERT_EQUAL_UINT32(0U, watchdog_test_get_refresh_count());

    /* Initialize watchdog */
    status_t status = watchdog_init(WATCHDOG_TIMEOUT_MS_DEFAULT);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* First refresh checkpoint */
    watchdog_refresh();
    TEST_ASSERT_EQUAL_UINT32(1U, watchdog_test_get_refresh_count());

    /* Multiple application state checkpoints */
    for (uint32_t i = 0; i < 10; i++) {
        watchdog_refresh();
    }
    TEST_ASSERT_EQUAL_UINT32(11U, watchdog_test_get_refresh_count());
}

/**
 * @brief TC-S3-T4.3-06: Boot Reset Reason Diagnostics (RCC_CSR_IWDGRSTF).
 */
static void test_watchdog_boot_reset_reason_detection(void) {
    /* 1. Normal clean boot -> no watchdog reset flag */
    TEST_ASSERT_FALSE(watchdog_was_reset_by_watchdog());

    /* 2. Simulate watchdog timeout reset */
    watchdog_test_set_reset_reason(true);
    TEST_ASSERT_TRUE(watchdog_was_reset_by_watchdog());

    /* 3. Clear reset flags */
    watchdog_clear_reset_flags();
    TEST_ASSERT_FALSE(watchdog_was_reset_by_watchdog());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_watchdog_constants_and_timing_verification);
    RUN_TEST(test_watchdog_init_default);
    RUN_TEST(test_watchdog_init_custom_timeouts);
    RUN_TEST(test_watchdog_init_invalid_params);
    RUN_TEST(test_watchdog_refresh_counter);
    RUN_TEST(test_watchdog_boot_reset_reason_detection);
    return UNITY_END();
}
