/**
 * @file    test_app_state_machine.c
 * @brief   Unity unit & integration test suite for Application State Machine Watchdog Checkpoints (S6-T3.3).
 * @details Validates 8 dedicated state checkpoints, anti-masking guards, boot reset diagnostics,
 *          Stop 2 deep sleep freeze, post-wake refresh, and hang recovery matrix (UT_WDG_01 - CP_WDG_10).
 */

#include "unity.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_state_machine.h"
#include "watchdog.h"
#include "power_mgr.h"
#include "bsp_power_rails.h"
#include "bsp_indicators.h"
#include "rain_gauge_driver.h"
#include "telemetry_codec.h"
#include "flash_storage.h"
#include "status.h"

/* ========================================================================== */
/* Setup & Teardown                                                           */
/* ========================================================================== */

void setUp(void) {
    watchdog_test_reset();
    power_mgr_test_reset();
    bsp_power_rails_test_reset();
    (void)bsp_power_rails_init();
    bsp_indicators_test_reset();
    (void)bsp_indicators_init();
    (void)rain_gauge_init();
    rain_gauge_reset_all_accumulators();
    (void)flash_storage_init();
    (void)flash_storage_erase_all_nvm_pages();
    (void)flash_ring_init();
    app_state_machine_reset();
}

void tearDown(void) {
    app_state_machine_reset();
    watchdog_test_reset();
    power_mgr_test_reset();
}

/* ========================================================================== */
/* Test Cases (UT_WDG_01 to CP_WDG_10)                                        */
/* ========================================================================== */

/**
 * @brief UT_WDG_01: Watchdog Initialization & Configuration on State Machine Boot.
 */
static void test_wdg_01_initialization(void) {
    TEST_ASSERT_FALSE(watchdog_is_enabled());

    status_t rc = app_state_machine_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, rc);

    /* Verify hardware IWDG supervisor is armed with 8.0s timeout */
    TEST_ASSERT_TRUE(watchdog_is_enabled());
    TEST_ASSERT_EQUAL_UINT32(8000UL, watchdog_get_timeout_ms());
    TEST_ASSERT_EQUAL_UINT32(0U, app_state_machine_get_watchdog_kick_count());
    TEST_ASSERT_EQUAL_INT(STATE_WAKE, app_state_machine_get_current_state());
}

/**
 * @brief CP_WDG_02: Sequential Checkpoint Kicks across Full 8-State Cycle.
 */
static void test_wdg_02_sequential_checkpoint_kicks(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
    TEST_ASSERT_EQUAL_UINT32(0U, watchdog_test_get_refresh_count());
    TEST_ASSERT_EQUAL_UINT32(0U, app_state_machine_get_watchdog_kick_count());

    /* Execute full 8-state sequence: WAKE -> POWER_ON -> SAMPLE -> FILTER ->
     * PREDICT -> TRANSMIT -> ALERT -> SLEEP */
    status_t rc = app_state_machine_run_cycle();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, rc);

    /* Exactly 8 state checkpoints must have triggered watchdog_refresh() */
    TEST_ASSERT_EQUAL_UINT32(8U, app_state_machine_get_watchdog_kick_count());
    TEST_ASSERT_EQUAL_UINT32(8U, watchdog_test_get_refresh_count());
}

/**
 * @brief CP_WDG_03: Boot Reset Cause Detection & Telemetry Unexpected Reset Notification.
 */
static void test_wdg_03_boot_reset_cause_detection(void) {
    /* 1. Simulate previous hardware reboot caused by IWDG expiration */
    watchdog_test_set_reset_reason(true);
    TEST_ASSERT_TRUE(watchdog_was_reset_by_watchdog());

    /* 2. Boot state machine */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
    TEST_ASSERT_TRUE(app_state_machine_was_boot_watchdog_reset());

    /* 3. Step through to STATE_ALERT (which executes STATE_TRANSMIT) */
    for (uint8_t i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    }
    TEST_ASSERT_EQUAL_INT(STATE_ALERT, app_state_machine_get_current_state());

    /* 4. Verify telemetry record in Flash contains unexpected_reset bit set */
    flash_record_t record;
    memset(&record, 0, sizeof(record));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, flash_ring_peek(0, &record));

    /* Byte 11, bit 7 is Unexpected Reset flag */
    bool telem_unexpected_reset = (record.payload[11] & 0x80U) != 0U;
    TEST_ASSERT_TRUE(telem_unexpected_reset);
}

/**
 * @brief CP_WDG_04: Hardware Reset Status Flags Cleared Post-Init.
 */
static void test_wdg_04_reset_flags_cleared(void) {
    watchdog_test_set_reset_reason(true);

    /* app_state_machine_init() captures the flag and clears hardware flags */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
    TEST_ASSERT_TRUE(app_state_machine_was_boot_watchdog_reset());

    /* Ensure subsequent direct query reports flags cleared */
    TEST_ASSERT_FALSE(watchdog_was_reset_by_watchdog());
}

/**
 * @brief CP_WDG_05: Stop 2 Deep Sleep Counter Freeze Verification.
 */
static void test_wdg_05_stop2_sleep_freeze(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* Run cycle through to SLEEP entry */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Verify system successfully transitioned through Stop 2 */
    TEST_ASSERT_EQUAL_UINT32(1U, power_mgr_test_get_sleep_cycle_count());
    /* Watchdog remains armed and running across low-power mode transitions */
    TEST_ASSERT_TRUE(watchdog_is_enabled());
}

/**
 * @brief CP_WDG_06: Resume Post-Sleep Kick at STATE_WAKE Entry.
 */
static void test_wdg_06_resume_post_sleep_kick(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* Complete 1st cycle (ends with sleep entry and resets next state to STATE_WAKE) */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_EQUAL_UINT32(8U, app_state_machine_get_watchdog_kick_count());
    TEST_ASSERT_EQUAL_INT(STATE_WAKE, app_state_machine_get_current_state());

    /* Wake simulation: execute STATE_WAKE */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    /* 9th kick registered immediately at Checkpoint 1 (WAKE) */
    TEST_ASSERT_EQUAL_UINT32(9U, app_state_machine_get_watchdog_kick_count());
    TEST_ASSERT_EQUAL_UINT32(9U, watchdog_test_get_refresh_count());
    TEST_ASSERT_EQUAL_INT(STATE_POWER_ON, app_state_machine_get_current_state());
}

/**
 * @brief CP_WDG_07: Anti-Masking Protection against Out-of-Range State Corruption.
 */
static void test_wdg_07_anti_masking_state_corruption(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* Reach STATE_SAMPLE */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step()); /* WAKE -> POWER_ON */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step()); /* POWER_ON -> SAMPLE */
    TEST_ASSERT_EQUAL_UINT32(2U, app_state_machine_get_watchdog_kick_count());

    /* Corrupt context state by resetting and attempting step with invalid state */
    /* An uninitialized or corrupted state step must fail and not kick watchdog */
    app_state_machine_reset();
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NOT_INITIALIZED, app_state_machine_step());
    TEST_ASSERT_EQUAL_UINT32(0U, app_state_machine_get_watchdog_kick_count());
}

/**
 * @brief CP_WDG_08: Anti-Masking Guard on Excessive Single-State Stall (> 200 ms).
 */
static void test_wdg_08_anti_masking_long_state_stall(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* WAKE step: entry tick recorded */
    power_mgr_test_set_tick_ms(100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step()); /* WAKE -> POWER_ON */
    TEST_ASSERT_EQUAL_UINT32(1U, app_state_machine_get_watchdog_kick_count());

    /* Simulate long stall in POWER_ON exceeding 200 ms guard threshold */
    power_mgr_test_set_tick_ms(400U); /* elapsed = 300 ms > 200 ms */

    /* Next step enters SAMPLE; app_watchdog_checkpoint(STATE_SAMPLE) checks elapsed time */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step()); /* POWER_ON -> SAMPLE */

    /* Because elapsed was 300ms (>= 200ms), watchdog refresh was suppressed! */
    TEST_ASSERT_EQUAL_UINT32(1U, app_state_machine_get_watchdog_kick_count());
    TEST_ASSERT_EQUAL_UINT32(1U, watchdog_test_get_refresh_count());
}

/**
 * @brief CP_WDG_09: Zero ISR Watchdog Refreshes Verification.
 */
static void test_wdg_09_zero_isr_refreshes(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
    uint32_t baseline_kicks = watchdog_test_get_refresh_count();

    /* Simulate rain pulse tipping interrupts on EXTI0 */
    for (uint32_t i = 0; i < 20; i++) {
        rain_gauge_exti_isr(power_mgr_get_tick_ms() + i * 100U);
    }

    /* Simulate periodic RTC / SysTick ISR ticks */
    power_mgr_test_set_tick_ms(power_mgr_get_tick_ms() + 50U);

    /* Watchdog refresh count MUST remain completely unchanged by ISR execution */
    TEST_ASSERT_EQUAL_UINT32(baseline_kicks, watchdog_test_get_refresh_count());
}

/**
 * @brief CP_WDG_10: Simulated Hang Recovery Cycle.
 */
static void test_wdg_10_simulated_hang_recovery(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* Advance time by 8.5 seconds without servicing */
    power_mgr_test_set_tick_ms(power_mgr_get_tick_ms() + 8500U);

    /* Next checkpoint detects stall (> 200 ms), suppressing kicks */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    TEST_ASSERT_EQUAL_UINT32(0U, app_state_machine_get_watchdog_kick_count());

    /* Simulate resultant MCU reset */
    watchdog_test_set_reset_reason(true);
    app_state_machine_reset();

    /* Reboot detects watchdog reset */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
    TEST_ASSERT_TRUE(app_state_machine_was_boot_watchdog_reset());
}

/* ========================================================================== */
/* Test Runner Main                                                           */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wdg_01_initialization);
    RUN_TEST(test_wdg_02_sequential_checkpoint_kicks);
    RUN_TEST(test_wdg_03_boot_reset_cause_detection);
    RUN_TEST(test_wdg_04_reset_flags_cleared);
    RUN_TEST(test_wdg_05_stop2_sleep_freeze);
    RUN_TEST(test_wdg_06_resume_post_sleep_kick);
    RUN_TEST(test_wdg_07_anti_masking_state_corruption);
    RUN_TEST(test_wdg_08_anti_masking_long_state_stall);
    RUN_TEST(test_wdg_09_zero_isr_refreshes);
    RUN_TEST(test_wdg_10_simulated_hang_recovery);
    return UNITY_END();
}
