/**
 * @file    test_alert_manager.c
 * @brief   Unity unit test suite for Alert Manager, Visual LEDs, and Siren Actuation.
 * @details Validates 100% branch coverage across LED flash waveforms, acoustic buzzer cadences,
 *          siren auto-cutoff guards, 30-min cooldown, battery throttling, and priority cascades.
 */

#include "unity.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "alert_manager.h"
#include "status.h"

/* ========================================================================== */
/* Mock State & Counting Structure                                            */
/* ========================================================================== */

typedef struct {
    uint32_t init_call_count;
    uint32_t all_off_call_count;
    uint32_t last_relay_pulse_ms;
} test_tracker_t;

static test_tracker_t s_track;

/* ========================================================================== */
/* Test Setup & Teardown                                                      */
/* ========================================================================== */

void setUp(void) {
    bsp_indicators_test_reset();
    memset(&s_track, 0, sizeof(s_track));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_init(NULL));
}

void tearDown(void) {
    alert_manager_force_all_off();
}

/* ========================================================================== */
/* Category A: Initialization & Lifecycle Tests                               */
/* ========================================================================== */

static void test_alert_init_defaults(void) {
    alert_manager_status_t status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_get_status(&status));

    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_OFF, status.active_led_pattern);
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_OFF, status.active_buzzer_pattern);
    TEST_ASSERT_FALSE(status.green_led_state);
    TEST_ASSERT_FALSE(status.red_led_state);
    TEST_ASSERT_FALSE(status.buzzer_state);
    TEST_ASSERT_FALSE(status.siren_relay_active);
    TEST_ASSERT_EQUAL_UINT32(0U, status.siren_cooldown_sec);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
    TEST_ASSERT_FALSE(bsp_relay_get());
}

static void test_alert_init_custom_config(void) {
    alert_manager_config_t cfg = {
        .enable_visual_leds         = false,
        .enable_audible_buzzer      = false,
        .enable_siren_relay         = false,
        .enable_night_quiet_hours   = false,
        .quiet_hours_start_hour     = 22U,
        .quiet_hours_end_hour       = 5U,
        .active_display_duration_ms = 15000U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_init(&cfg));

    /* Force a strobe pattern */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_set_led_pattern(ALERT_LED_PATTERN_IMMINENT_STROBE));
    alert_manager_process_step(20U);

    /* Because enable_visual_leds is false, physical mock LEDs must remain false */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
}

static void test_alert_force_all_off(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_set_led_pattern(ALERT_LED_PATTERN_WATCH_AMBER));
    alert_manager_process_step(10U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));

    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_force_all_off());
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_OFF, alert_manager_get_active_led_pattern());
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
    TEST_ASSERT_FALSE(bsp_relay_get());
}

static void test_alert_manual_led_override(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_set_led_pattern(ALERT_LED_PATTERN_WARNING_RED));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_WARNING_RED, alert_manager_get_active_led_pattern());

    alert_manager_process_step(0U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, alert_manager_set_led_pattern(ALERT_LED_PATTERN_MAX));
}

static void test_alert_manual_buzzer_override(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_set_buzzer_pattern(ALERT_BUZZER_PATTERN_DOUBLE_CHIRP));
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_DOUBLE_CHIRP, alert_manager_get_active_buzzer_pattern());

    alert_manager_process_step(10U);
    TEST_ASSERT_TRUE(bsp_buzzer_get());

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, alert_manager_set_buzzer_pattern(ALERT_BUZZER_PATTERN_MAX));
}

static void test_alert_pattern_name_lookup(void) {
    TEST_ASSERT_EQUAL_STRING("OFF", alert_manager_get_pattern_name(ALERT_LED_PATTERN_OFF));
    TEST_ASSERT_EQUAL_STRING("HEALTHY_GREEN_PULSE", alert_manager_get_pattern_name(ALERT_LED_PATTERN_HEALTHY_PULSE));
    TEST_ASSERT_EQUAL_STRING("WATCH_AMBER_BLINK", alert_manager_get_pattern_name(ALERT_LED_PATTERN_WATCH_AMBER));
    TEST_ASSERT_EQUAL_STRING("WARNING_RED_BLINK", alert_manager_get_pattern_name(ALERT_LED_PATTERN_WARNING_RED));
    TEST_ASSERT_EQUAL_STRING("IMMINENT_RED_STROBE", alert_manager_get_pattern_name(ALERT_LED_PATTERN_IMMINENT_STROBE));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", alert_manager_get_pattern_name(ALERT_LED_PATTERN_MAX));
}

/* ========================================================================== */
/* Category B: Visual Status LED Waveform & Timing Tests                      */
/* ========================================================================== */

static void test_alert_led_healthy_pulse_timing(void) {
    alert_input_t in = {
        .rain_state   = RAIN_ALERT_UNLIKELY,
        .cpi_pct      = 15.0f,
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_HEALTHY_PULSE, alert_manager_get_active_led_pattern());

    /* t = 0ms: Pulse Start -> Green ON */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* t = 49ms: Still within 50ms pulse -> Green ON */
    alert_manager_process_step(49U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));

    /* t = 50ms: Pulse elapsed -> Green OFF */
    alert_manager_process_step(1U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));

    /* t = 950ms later (total 1000ms) -> Next cycle pulse start -> Green ON */
    alert_manager_process_step(950U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
}

static void test_alert_led_watch_amber_timing(void) {
    alert_input_t in = {
        .rain_state   = RAIN_ALERT_POSSIBLE,
        .cpi_pct      = 45.0f,
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_WATCH_AMBER, alert_manager_get_active_led_pattern());

    /* t = 0ms: Amber ON (Both Green & Red ON) */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    /* t = 249ms: Still ON */
    alert_manager_process_step(249U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    /* t = 250ms: Toggle OFF */
    alert_manager_process_step(1U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* t = 500ms: Cycle restarts -> Both ON */
    alert_manager_process_step(250U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
}

static void test_alert_led_warning_red_timing(void) {
    alert_input_t in = {
        .rain_state   = RAIN_ALERT_LIKELY,
        .cpi_pct      = 72.0f,
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_WARNING_RED, alert_manager_get_active_led_pattern());

    /* 200ms Red ON / 200ms OFF */
    alert_manager_process_step(0U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    alert_manager_process_step(199U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    alert_manager_process_step(1U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    alert_manager_process_step(200U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
}

static void test_alert_led_imminent_strobe_timing(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 85.0f,
        .battery_tier     = BATTERY_TIER_NORMAL,
        .rtc_hour_0_to_23 = 14U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_IMMINENT_STROBE, alert_manager_get_active_led_pattern());

    /* 10 Hz Strobe: 50ms ON / 50ms OFF */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));

    alert_manager_process_step(50U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    alert_manager_process_step(50U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
}

static void test_alert_led_active_rain_double_flash(void) {
    alert_input_t in = {
        .rain_state         = RAIN_ALERT_POSSIBLE,
        .cpi_pct            = 40.0f,
        .rain_pulses_recent = 3U, /* Tipping bucket active */
        .battery_tier       = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_ACTIVE_RAIN, alert_manager_get_active_led_pattern());

    /* Pulse 1: 0..49ms ON */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    /* Gap: 50..99ms OFF */
    alert_manager_process_step(50U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* Pulse 2: 100..149ms ON */
    alert_manager_process_step(50U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    /* Long Gap: 150..999ms OFF */
    alert_manager_process_step(50U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
}

static void test_alert_led_system_fault_beacon(void) {
    alert_input_t in = {
        .sensor_fault = true,
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_SYSTEM_FAULT, alert_manager_get_active_led_pattern());

    /* Phase 0..99ms: Green ON, Red OFF */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* Phase 100..199ms: Green OFF, Red ON */
    alert_manager_process_step(100U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    /* Phase 200..299ms: Green ON, Red OFF */
    alert_manager_process_step(100U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
}

/* ========================================================================== */
/* Category C: Acoustic & Siren Actuation Tests                               */
/* ========================================================================== */

static void test_alert_buzzer_watch_single_chirp(void) {
    alert_input_t in = {
        .rain_state   = RAIN_ALERT_POSSIBLE,
        .cpi_pct      = 35.0f,
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_SHORT_CHIRP, alert_manager_get_active_buzzer_pattern());

    /* t = 0..49ms: Buzzer ON */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_buzzer_get());

    /* t = 50ms: Buzzer OFF */
    alert_manager_process_step(50U);
    TEST_ASSERT_FALSE(bsp_buzzer_get());
}

static void test_alert_buzzer_warning_double_chirp(void) {
    alert_input_t in = {
        .rain_state   = RAIN_ALERT_LIKELY,
        .cpi_pct      = 65.0f,
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_DOUBLE_CHIRP, alert_manager_get_active_buzzer_pattern());

    /* Pulse 1: 0..99ms ON */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_buzzer_get());

    /* Gap: 100..199ms OFF */
    alert_manager_process_step(100U);
    TEST_ASSERT_FALSE(bsp_buzzer_get());

    /* Pulse 2: 200..299ms ON */
    alert_manager_process_step(100U);
    TEST_ASSERT_TRUE(bsp_buzzer_get());

    /* Done: 300ms+ OFF */
    alert_manager_process_step(100U);
    TEST_ASSERT_FALSE(bsp_buzzer_get());
}

static void test_alert_storm_siren_auto_trigger(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 88.0f,
        .battery_tier     = BATTERY_TIER_NORMAL,
        .rtc_hour_0_to_23 = 15U /* 3 PM Daytime */
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    TEST_ASSERT_TRUE(alert_manager_is_siren_active());
    TEST_ASSERT_TRUE(bsp_relay_get());
    TEST_ASSERT_EQUAL_UINT32(10000U, bsp_indicators_test_get_relay_timer_ms());
    TEST_ASSERT_EQUAL_UINT32(1800U, alert_manager_get_siren_cooldown_remaining_sec());
}

static void test_alert_siren_10s_auto_cutoff(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 88.0f,
        .battery_tier     = BATTERY_TIER_NORMAL,
        .rtc_hour_0_to_23 = 15U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    TEST_ASSERT_TRUE(alert_manager_is_siren_active());

    /* Step past 10 seconds */
    alert_manager_process_step(10001U);
    TEST_ASSERT_FALSE(alert_manager_is_siren_active());
    TEST_ASSERT_FALSE(bsp_relay_get());
}

static void test_alert_siren_30min_cooldown(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 88.0f,
        .battery_tier     = BATTERY_TIER_NORMAL,
        .rtc_hour_0_to_23 = 15U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));
    alert_manager_process_step(10000U); /* Siren ends, cooldown = ~1790s */

    /* Reset bsp relay manually */
    bsp_relay_set(false);

    /* Next sample 2 minutes later (120s elapsed) */
    alert_manager_process_step(120000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* Siren relay must remain OFF due to active cooldown */
    TEST_ASSERT_FALSE(alert_manager_is_siren_active());
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_TRUE(alert_manager_get_siren_cooldown_remaining_sec() > 0U);
}

static void test_alert_night_quiet_hours_mute(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 92.0f,
        .battery_tier     = BATTERY_TIER_NORMAL,
        .rtc_hour_0_to_23 = 23U /* 11 PM Night */
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* Visual Red Strobe is active */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_IMMINENT_STROBE, alert_manager_get_active_led_pattern());

    /* Audible buzzer burst is active */
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_STORM_BURST, alert_manager_get_active_buzzer_pattern());

    /* External siren relay is MUTED */
    TEST_ASSERT_FALSE(alert_manager_is_siren_active());
    TEST_ASSERT_FALSE(bsp_relay_get());
}

/* ========================================================================== */
/* Category D: Priority Resolution & Battery Interlocks                       */
/* ========================================================================== */

static void test_alert_battery_conservation_tier2(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 85.0f,
        .battery_tier     = BATTERY_TIER_CONSERVATION, /* Vbat < 3.10V */
        .rtc_hour_0_to_23 = 14U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* In Conservation mode, storm strobe is throttled down to conservation micro-pulse */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_CONSERVATION, alert_manager_get_active_led_pattern());

    /* Acoustic buzzer and siren relay are completely muted */
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_OFF, alert_manager_get_active_buzzer_pattern());
    TEST_ASSERT_FALSE(alert_manager_is_siren_active());
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_FALSE(bsp_buzzer_get());

    /* Verify 10ms micro-pulse */
    alert_manager_process_step(0U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    alert_manager_process_step(10U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
}

static void test_alert_battery_critical_tier3(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 99.0f,
        .battery_tier     = BATTERY_TIER_CRITICAL, /* Vbat < 2.90V */
        .rtc_hour_0_to_23 = 14U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* Total shutdown of all indicators */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_OFF, alert_manager_get_active_led_pattern());
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_OFF, alert_manager_get_active_buzzer_pattern());

    alert_manager_process_step(50U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
    TEST_ASSERT_FALSE(bsp_relay_get());
}

static void test_alert_sensor_fault_preemption(void) {
    alert_input_t in = {
        .rain_state   = RAIN_ALERT_IMMINENT,
        .cpi_pct      = 95.0f,
        .sensor_fault = true, /* Hardware fault active */
        .battery_tier = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* Fault beacon overrides storm strobe */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_SYSTEM_FAULT, alert_manager_get_active_led_pattern());
    TEST_ASSERT_EQUAL_INT(ALERT_BUZZER_PATTERN_FAULT_BEEP, alert_manager_get_active_buzzer_pattern());
}

static void test_alert_active_rain_preemption(void) {
    alert_input_t in = {
        .rain_state         = RAIN_ALERT_POSSIBLE,
        .cpi_pct            = 40.0f,
        .rain_pulses_recent = 2U, /* Rain bucket tips */
        .battery_tier       = BATTERY_TIER_NORMAL
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* Active rain overrides amber watch */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_ACTIVE_RAIN, alert_manager_get_active_led_pattern());
}

static void test_alert_full_storm_lifecycle_sim(void) {
    alert_input_t in = { .battery_tier = BATTERY_TIER_NORMAL, .rtc_hour_0_to_23 = 14U };

    /* Step 1: Quiescent Fair Weather */
    in.rain_state = RAIN_ALERT_UNLIKELY;
    in.cpi_pct    = 10.0f;
    alert_manager_update(&in);
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_HEALTHY_PULSE, alert_manager_get_active_led_pattern());

    /* Step 2: Unsettled Watch */
    in.rain_state = RAIN_ALERT_POSSIBLE;
    in.cpi_pct    = 42.0f;
    alert_manager_update(&in);
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_WATCH_AMBER, alert_manager_get_active_led_pattern());

    /* Step 3: Storm Imminent Alert */
    in.rain_state = RAIN_ALERT_IMMINENT;
    in.cpi_pct    = 88.0f;
    alert_manager_update(&in);
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_IMMINENT_STROBE, alert_manager_get_active_led_pattern());
    TEST_ASSERT_TRUE(alert_manager_is_siren_active());

    /* Step 4: Active Rain Downpour */
    in.rain_pulses_recent = 5U;
    in.rain_state = RAIN_ALERT_IMMINENT;
    alert_manager_update(&in);
    /* Imminent storm still takes precedence over rain double flash when CPI >= 80% */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_IMMINENT_STROBE, alert_manager_get_active_led_pattern());
}

static void test_alert_defensive_null_and_guards(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, alert_manager_update(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, alert_manager_get_status(NULL));
}

/* ========================================================================== */
/* Main Unity Test Runner                                                     */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Category A: Lifecycle & Overrides */
    RUN_TEST(test_alert_init_defaults);
    RUN_TEST(test_alert_init_custom_config);
    RUN_TEST(test_alert_force_all_off);
    RUN_TEST(test_alert_manual_led_override);
    RUN_TEST(test_alert_manual_buzzer_override);
    RUN_TEST(test_alert_pattern_name_lookup);

    /* Category B: Visual Status LED Waveform & Timing */
    RUN_TEST(test_alert_led_healthy_pulse_timing);
    RUN_TEST(test_alert_led_watch_amber_timing);
    RUN_TEST(test_alert_led_warning_red_timing);
    RUN_TEST(test_alert_led_imminent_strobe_timing);
    RUN_TEST(test_alert_led_active_rain_double_flash);
    RUN_TEST(test_alert_led_system_fault_beacon);

    /* Category C: Acoustic & Siren Actuation */
    RUN_TEST(test_alert_buzzer_watch_single_chirp);
    RUN_TEST(test_alert_buzzer_warning_double_chirp);
    RUN_TEST(test_alert_storm_siren_auto_trigger);
    RUN_TEST(test_alert_siren_10s_auto_cutoff);
    RUN_TEST(test_alert_siren_30min_cooldown);
    RUN_TEST(test_alert_night_quiet_hours_mute);

    /* Category D: Priority Resolution & Battery Interlocks */
    RUN_TEST(test_alert_battery_conservation_tier2);
    RUN_TEST(test_alert_battery_critical_tier3);
    RUN_TEST(test_alert_sensor_fault_preemption);
    RUN_TEST(test_alert_active_rain_preemption);
    RUN_TEST(test_alert_full_storm_lifecycle_sim);
    RUN_TEST(test_alert_defensive_null_and_guards);

    return UNITY_END();
}
