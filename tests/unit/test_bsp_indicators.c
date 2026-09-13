/**
 * @file    test_bsp_indicators.c
 * @brief   Unit test verification suite for Board Indicators and Alarm Actuators.
 * @details Validates direct LED/buzzer/relay control, timed relay safety auto-cutoff,
 *          non-blocking pattern animation progressions, and pre-sleep master shutdown.
 */

#include "unity.h"
#include "bsp_indicators.h"

void setUp(void) {
    bsp_indicators_test_reset();
    (void)bsp_indicators_init();
}

void tearDown(void) {
    (void)bsp_indicators_all_off();
}

/**
 * @brief TC-S3-T3.2-01 & TC-S3-T3.2-02: Default Power-On Output States & Initialization.
 */
static void test_bsp_indicators_default_states(void) {
    status_t status = bsp_indicators_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify all indicators and actuators default to OFF */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_EQUAL_INT(BSP_INDICATOR_PATTERN_OFF, bsp_indicator_get_pattern());
    TEST_ASSERT_EQUAL_UINT32(0U, bsp_indicators_test_get_relay_timer_ms());
    TEST_ASSERT_EQUAL_UINT32(0U, bsp_indicators_test_get_pattern_timer_ms());
}

/**
 * @brief TC-S3-T3.2-03: Direct Status LED Control & Toggle.
 */
static void test_bsp_led_direct_control_and_toggle(void) {
    /* Green LED direct set & get */
    bsp_led_set(BSP_LED_GREEN, true);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    bsp_led_set(BSP_LED_GREEN, false);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));

    /* Green LED toggle */
    bsp_led_toggle(BSP_LED_GREEN);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    bsp_led_toggle(BSP_LED_GREEN);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));

    /* Red LED direct set & toggle */
    bsp_led_set(BSP_LED_RED, true);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
    bsp_led_toggle(BSP_LED_RED);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* Boundary safety for invalid LED enum */
    bsp_led_set(BSP_LED_MAX, true);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_MAX));
    bsp_led_toggle(BSP_LED_MAX);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_MAX));
}

/**
 * @brief TC-S3-T3.2-04: Direct Piezo Buzzer & Alert Relay Gate Control.
 */
static void test_bsp_buzzer_and_relay_direct_control(void) {
    /* Piezo Buzzer Gate */
    bsp_buzzer_set(true);
    TEST_ASSERT_TRUE(bsp_buzzer_get());
    bsp_buzzer_set(false);
    TEST_ASSERT_FALSE(bsp_buzzer_get());

    /* Alert Relay Gate */
    bsp_relay_set(true);
    TEST_ASSERT_TRUE(bsp_relay_get());
    bsp_relay_set(false);
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_EQUAL_UINT32(0U, bsp_indicators_test_get_relay_timer_ms());
}

/**
 * @brief TC-S3-T3.2-05 & TC-S3-T3.2-06: Timed Relay Activation & Auto-Cutoff Safety Guard.
 */
static void test_bsp_relay_timed_trigger_and_auto_cutoff(void) {
    /* 1. Trigger timed relay pulse of 500 ms */
    status_t status = bsp_relay_trigger_timed(500U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(bsp_relay_get());
    TEST_ASSERT_EQUAL_UINT32(500U, bsp_indicators_test_get_relay_timer_ms());

    /* 2. Step time by 200 ms -> 300 ms remaining -> relay still active */
    bsp_indicators_process(200U);
    TEST_ASSERT_TRUE(bsp_relay_get());
    TEST_ASSERT_EQUAL_UINT32(300U, bsp_indicators_test_get_relay_timer_ms());

    /* 3. Step time by 400 ms -> timer expires -> relay auto-de-energizes */
    bsp_indicators_process(400U);
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_EQUAL_UINT32(0U, bsp_indicators_test_get_relay_timer_ms());

    /* 4. Maximum duration clamping (requested 30,000 ms clamped to 10,000 ms) */
    status = bsp_relay_trigger_timed(30000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(bsp_relay_get());
    TEST_ASSERT_EQUAL_UINT32(BSP_RELAY_MAX_PULSE_DURATION_MS, bsp_indicators_test_get_relay_timer_ms());

    /* 5. Zero duration shuts off relay immediately */
    status = bsp_relay_trigger_timed(0U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(bsp_relay_get());
}

/**
 * @brief TC-S3-T3.2-07: Non-Blocking Indicator Pattern Animation Progressions.
 */
static void test_bsp_indicator_pattern_progressions(void) {
    /* 1. Heartbeat Pattern (Green 50ms pulse every 5000ms) */
    status_t status = bsp_indicator_set_pattern(BSP_INDICATOR_PATTERN_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(BSP_INDICATOR_PATTERN_HEARTBEAT, bsp_indicator_get_pattern());

    bsp_indicators_process(25U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());

    bsp_indicators_process(50U); /* Total 75ms -> pulse finished */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));

    /* 2. Watch Pattern (Amber 500ms ON / 500ms OFF + 50ms chirp) */
    status = bsp_indicator_set_pattern(BSP_INDICATOR_PATTERN_WATCH);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    bsp_indicators_process(100U); /* Within first 500ms */
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    bsp_indicators_process(500U); /* Total 600ms -> OFF phase */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* 3. Warning Pattern (Red 200ms ON / 200ms OFF) */
    status = bsp_indicator_set_pattern(BSP_INDICATOR_PATTERN_WARNING);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    bsp_indicators_process(100U);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));

    bsp_indicators_process(150U); /* Total 250ms -> OFF phase */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));

    /* 4. Storm Alert Pattern (Red 100ms strobe + buzzer) */
    status = bsp_indicator_set_pattern(BSP_INDICATOR_PATTERN_STORM_ALERT);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    bsp_indicators_process(50U);
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_TRUE(bsp_buzzer_get());

    bsp_indicators_process(100U); /* Total 150ms -> OFF phase */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());

    /* 5. Setting pattern to OFF silences everything */
    status = bsp_indicator_set_pattern(BSP_INDICATOR_PATTERN_OFF);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
}

/**
 * @brief TC-S3-T3.2-08: Pre-Sleep Master Actuator Shutdown & Boundary Guards.
 */
static void test_bsp_indicators_all_off_shutdown(void) {
    /* Turn on all actuators and set pattern */
    bsp_led_set(BSP_LED_GREEN, true);
    bsp_led_set(BSP_LED_RED, true);
    bsp_buzzer_set(true);
    bsp_relay_set(true);
    (void)bsp_indicator_set_pattern(BSP_INDICATOR_PATTERN_STORM_ALERT);

    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_TRUE(bsp_buzzer_get());
    TEST_ASSERT_TRUE(bsp_relay_get());

    /* Execute master shutdown */
    status_t status = bsp_indicators_all_off();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_EQUAL_INT(BSP_INDICATOR_PATTERN_OFF, bsp_indicator_get_pattern());

    /* Boundary parameter guard */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, bsp_indicator_set_pattern((bsp_indicator_pattern_t)99U));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bsp_indicators_default_states);
    RUN_TEST(test_bsp_led_direct_control_and_toggle);
    RUN_TEST(test_bsp_buzzer_and_relay_direct_control);
    RUN_TEST(test_bsp_relay_timed_trigger_and_auto_cutoff);
    RUN_TEST(test_bsp_indicator_pattern_progressions);
    RUN_TEST(test_bsp_indicators_all_off_shutdown);
    return UNITY_END();
}
