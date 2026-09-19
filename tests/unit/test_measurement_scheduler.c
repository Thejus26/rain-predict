/**
 * @file    test_measurement_scheduler.c
 * @brief   Unit tests for the adaptive multi-rate measurement scheduler.
 * @details Covers TC-SCHED-01 through TC-SCHED-10 using ThrowTheSwitch Unity.
 */

#include "unity.h"
#include "measurement_scheduler.h"
#include <string.h>

void setUp(void) {
    measurement_scheduler_init(NULL);
}

void tearDown(void) {
    /* No cleanup required */
}

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

/**
 * @brief TC-SCHED-01: Nominal Initial Mode Evaluation
 * Quiescent conditions result in 15-minute nominal sampling.
 */
static void test_tc_sched_01_nominal_initial_mode(void) {
    scheduler_decision_t decision;
    status_t st = measurement_scheduler_evaluate(15U, 0.0f, false, 0U, 0U, &decision);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_NOMINAL, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_NOMINAL_SEC, decision.target_interval_sec);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_NOMINAL_SEC, decision.computed_sleep_sec);
    TEST_ASSERT_FALSE(decision.mode_changed);
    TEST_ASSERT_EQUAL_UINT32(0U, decision.hold_down_remaining_sec);
}

/**
 * @brief TC-SCHED-02: Storm Trigger by CPI Score
 * CPI >= 40% accelerates sampling to 5-minute Storm Watch mode.
 */
static void test_tc_sched_02_storm_trigger_by_cpi(void) {
    scheduler_decision_t decision;
    status_t st = measurement_scheduler_evaluate(45U, 0.0f, false, 0U, 0U, &decision);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_STORM_SEC, decision.target_interval_sec);
    TEST_ASSERT_TRUE(decision.mode_changed);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_HOLD_DOWN_DURATION_SEC, decision.hold_down_remaining_sec);
}

/**
 * @brief TC-SCHED-03: Storm Trigger by Barometric Plunge
 * Rapid pressure drop (<= -1.0 hPa/hr) triggers Storm Watch mode.
 */
static void test_tc_sched_03_storm_trigger_by_pressure_plunge(void) {
    scheduler_decision_t decision;
    status_t st = measurement_scheduler_evaluate(20U, -1.5f, false, 0U, 0U, &decision);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_STORM_SEC, decision.target_interval_sec);
    TEST_ASSERT_TRUE(decision.mode_changed);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_HOLD_DOWN_DURATION_SEC, decision.hold_down_remaining_sec);
}

/**
 * @brief Storm Trigger by Daylight Solar Cloud Drop Alarm
 * Optical attenuation alarm triggers Storm Watch mode even with low CPI.
 */
static void test_storm_trigger_by_solar_alarm(void) {
    scheduler_decision_t decision;
    status_t st = measurement_scheduler_evaluate(10U, 0.0f, true, 0U, 0U, &decision);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_STORM_SEC, decision.target_interval_sec);
    TEST_ASSERT_TRUE(decision.mode_changed);
}

/**
 * @brief TC-SCHED-04: Active Rain Acceleration
 * Tipping bucket pulses accelerate sampling to 2-minute Active Rain mode.
 */
static void test_tc_sched_04_active_rain_acceleration(void) {
    scheduler_decision_t decision;
    status_t st = measurement_scheduler_evaluate(50U, 0.0f, false, 3U, 0U, &decision);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_ACTIVE_RAIN, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_RAIN_SEC, decision.target_interval_sec);
    TEST_ASSERT_TRUE(decision.mode_changed);
}

/**
 * @brief TC-SCHED-05: Anti-Chatter Hold-Down Countdown
 * System remains in Storm Watch for 30 minutes of calm before reverting to Nominal.
 */
static void test_tc_sched_05_anti_chatter_hold_down(void) {
    scheduler_decision_t decision;

    /* 1. Trigger Storm Watch mode */
    status_t st = measurement_scheduler_evaluate(45U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(1800U, decision.hold_down_remaining_sec);

    /* 2. Step calm weather (CPI = 10%) through 5 evaluations in Storm Watch */
    for (uint32_t i = 0; i < 5U; i++) {
        st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
        TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
        TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
        TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_STORM_SEC, decision.target_interval_sec);
        TEST_ASSERT_FALSE(decision.mode_changed);
        TEST_ASSERT_EQUAL_UINT32(1500U - (i * 300U), decision.hold_down_remaining_sec);
    }

    /* 3. The 6th calm evaluation (30 min of calm elapsed) transitions to Nominal */
    st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_NOMINAL, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_NOMINAL_SEC, decision.target_interval_sec);
    TEST_ASSERT_TRUE(decision.mode_changed);
    TEST_ASSERT_EQUAL_UINT32(0U, decision.hold_down_remaining_sec);
}

/**
 * @brief Hold-Down Timer Reset on Intermittent Storm Spikes
 * Fluctuations during hold-down window reset the 30-minute timer.
 */
static void test_anti_chatter_retrigger_resets_timer(void) {
    scheduler_decision_t decision;

    /* Trigger Storm Watch */
    measurement_scheduler_evaluate(45U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);

    /* Calm sample 1: 1500s remaining */
    measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_UINT32(1500U, decision.hold_down_remaining_sec);

    /* Storm precursor reasserts (CPI = 42%): timer reloads to 1800s */
    measurement_scheduler_evaluate(42U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(1800U, decision.hold_down_remaining_sec);

    /* Next calm sample: 1500s remaining again */
    measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_UINT32(1500U, decision.hold_down_remaining_sec);
}

/**
 * @brief Rain Calm Hold-Down Transition to Storm Watch
 * After rain ceases, station holds 2-min rate for 10 min before returning to Storm Watch.
 */
static void test_rain_calm_hold_down_to_storm_watch(void) {
    scheduler_decision_t decision;

    /* Trigger active rain */
    measurement_scheduler_evaluate(50U, 0.0f, false, 1U, 0U, &decision);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_ACTIVE_RAIN, decision.active_mode);

    /* Rain stops: 4 cycles of zero rain remain in ACTIVE_RAIN (4 * 120s = 480s) */
    for (uint32_t i = 0; i < 4U; i++) {
        measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
        TEST_ASSERT_EQUAL(SCHEDULER_MODE_ACTIVE_RAIN, decision.active_mode);
        TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_RAIN_SEC, decision.target_interval_sec);
    }

    /* 5th cycle of zero rain (10 minutes total): transitions to STORM_WATCH */
    measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_STORM_SEC, decision.target_interval_sec);
    TEST_ASSERT_TRUE(decision.mode_changed);
}

/**
 * @brief TC-SCHED-06: Active Execution Time Compensation
 * Subtraction of ceil(T_active / 1000) from target interval.
 */
static void test_tc_sched_06_active_time_compensation(void) {
    scheduler_decision_t decision;

    /* 1. Nominal interval 900s, active time 1200ms -> ceil(1.2s) = 2s -> 898s sleep */
    status_t st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 1200U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL_UINT32(900U, decision.target_interval_sec);
    TEST_ASSERT_EQUAL_UINT32(898U, decision.computed_sleep_sec);

    /* 2. Zero active time -> full 900s sleep */
    st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL_UINT32(900U, decision.computed_sleep_sec);

    /* 3. Exact 1000ms active time -> 1s subtracted -> 899s sleep */
    st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 1000U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL_UINT32(899U, decision.computed_sleep_sec);

    /* 4. Over-budget execution time (> 900s) -> minimum 1s sleep safeguard */
    st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 950000U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL_UINT32(1U, decision.computed_sleep_sec);
}

/**
 * @brief TC-SCHED-07: Downlink Override Activation
 * LoRaWAN downlink custom interval override (FPort 10 Cmd 0x01).
 */
static void test_tc_sched_07_downlink_override_activation(void) {
    status_t st = measurement_scheduler_set_override_interval(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);

    scheduler_decision_t decision;
    st = measurement_scheduler_evaluate(15U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_OVERRIDE, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(600U, decision.target_interval_sec);
    TEST_ASSERT_EQUAL_UINT32(600U, decision.computed_sleep_sec);
}

/**
 * @brief TC-SCHED-08: Downlink Override Clear & Reversion
 * Disabling downlink override reverts to autonomous mode.
 */
static void test_tc_sched_08_downlink_override_clear_and_revert(void) {
    measurement_scheduler_set_override_interval(600U);

    status_t st = measurement_scheduler_clear_override();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);

    scheduler_decision_t decision;
    st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_NOMINAL, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(SCHEDULER_INTERVAL_NOMINAL_SEC, decision.target_interval_sec);
}

/**
 * @brief TC-SCHED-09: Cumulative Wakeup Counter Tracking
 * Verify nominal, storm watch, and active rain cycle counters.
 */
static void test_tc_sched_09_cumulative_counters(void) {
    scheduler_decision_t decision;

    /* 3 nominal wakeups */
    for (int i = 0; i < 3; i++) {
        measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, &decision);
    }

    /* 2 storm watch wakeups */
    for (int i = 0; i < 2; i++) {
        measurement_scheduler_evaluate(50U, 0.0f, false, 0U, 0U, &decision);
    }

    /* 1 active rain wakeup */
    measurement_scheduler_evaluate(50U, 0.0f, false, 2U, 0U, &decision);

    scheduler_status_t status;
    status_t st = measurement_scheduler_get_status(&status);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL_UINT32(3U, status.total_wakeups_nominal);
    TEST_ASSERT_EQUAL_UINT32(2U, status.total_wakeups_storm);
    TEST_ASSERT_EQUAL_UINT32(1U, status.total_wakeups_rain);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_ACTIVE_RAIN, status.current_mode);
}

/**
 * @brief TC-SCHED-10: Defensive Parameter & NULL Guards
 * Defensive bounds checks and NULL pointers return appropriate errors safely.
 */
static void test_tc_sched_10_defensive_guards(void) {
    /* NULL decision pointer */
    status_t st = measurement_scheduler_evaluate(10U, 0.0f, false, 0U, 0U, NULL);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, st);

    /* NULL status pointer */
    st = measurement_scheduler_get_status(NULL);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, st);

    /* Below minimum override interval (< 60s) */
    st = measurement_scheduler_set_override_interval(30U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_OUT_OF_BOUNDS, st);

    /* Above maximum override interval (> 3600s) */
    st = measurement_scheduler_set_override_interval(3601U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_OUT_OF_BOUNDS, st);

    /* Verify state remains uncorrupted */
    scheduler_status_t status;
    st = measurement_scheduler_get_status(&status);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_FALSE(status.is_override_active);
}

/**
 * @brief Custom Configuration Initialization
 * Custom thresholds and interval definitions.
 */
static void test_custom_configuration(void) {
    scheduler_config_t cfg = {
        .nominal_interval_sec      = 1200U,
        .storm_interval_sec        = 240U,
        .rain_interval_sec         = 60U,
        .hold_down_sec             = 600U,
        .cpi_trigger_pct           = 50U,
        .cpi_calm_pct              = 25U,
        .pressure_drop_trigger_hpa = -2.0f
    };

    status_t st = measurement_scheduler_init(&cfg);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);

    scheduler_decision_t decision;
    /* CPI 45% should NOT trigger with custom 50% threshold */
    st = measurement_scheduler_evaluate(45U, -1.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_NOMINAL, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(1200U, decision.target_interval_sec);

    /* CPI 55% should trigger custom storm interval */
    st = measurement_scheduler_evaluate(55U, 0.0f, false, 0U, 0U, &decision);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_STORM_WATCH, decision.active_mode);
    TEST_ASSERT_EQUAL_UINT32(240U, decision.target_interval_sec);
    TEST_ASSERT_EQUAL_UINT32(600U, decision.hold_down_remaining_sec);
}

/**
 * @brief Scheduler Reset API Verification
 * Reset clears active overrides and returns to nominal mode.
 */
static void test_scheduler_reset(void) {
    measurement_scheduler_set_override_interval(600U);

    status_t st = measurement_scheduler_reset();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);

    scheduler_status_t status;
    st = measurement_scheduler_get_status(&status);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULER_MODE_NOMINAL, status.current_mode);
    TEST_ASSERT_FALSE(status.is_override_active);
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tc_sched_01_nominal_initial_mode);
    RUN_TEST(test_tc_sched_02_storm_trigger_by_cpi);
    RUN_TEST(test_tc_sched_03_storm_trigger_by_pressure_plunge);
    RUN_TEST(test_storm_trigger_by_solar_alarm);
    RUN_TEST(test_tc_sched_04_active_rain_acceleration);
    RUN_TEST(test_tc_sched_05_anti_chatter_hold_down);
    RUN_TEST(test_anti_chatter_retrigger_resets_timer);
    RUN_TEST(test_rain_calm_hold_down_to_storm_watch);
    RUN_TEST(test_tc_sched_06_active_time_compensation);
    RUN_TEST(test_tc_sched_07_downlink_override_activation);
    RUN_TEST(test_tc_sched_08_downlink_override_clear_and_revert);
    RUN_TEST(test_tc_sched_09_cumulative_counters);
    RUN_TEST(test_tc_sched_10_defensive_guards);
    RUN_TEST(test_custom_configuration);
    RUN_TEST(test_scheduler_reset);

    return UNITY_END();
}
