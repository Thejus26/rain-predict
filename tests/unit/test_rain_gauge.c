/**
 * @file    test_rain_gauge.c
 * @brief   ThrowTheSwitch Unity unit test suite for Tipping-Bucket Rain Gauge Driver (S4-T3.1).
 * @details Validates GPIO EXTI0 interrupt configuration, 50ms dual-stage debounce filter,
 *          mechanical chatter rejection, SysTick 32-bit rollover safety, active rain detection,
 *          and low-power deinitialization.
 */

#include "unity.h"
#include "rain_gauge_driver.h"
#include "status.h"

void setUp(void) {
    (void)rain_gauge_init();
}

void tearDown(void) {
    (void)rain_gauge_deinit();
}

/**
 * @brief TC-S4-T3.1-01: Header Definitions & Macro Constants.
 */
static void test_rain_gauge_macro_definitions(void) {
    TEST_ASSERT_EQUAL_UINT32(50U, RAIN_GAUGE_DEBOUNCE_MS);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.20f, RAIN_GAUGE_CALIB_MM_PER_TIP);
    TEST_ASSERT_EQUAL_UINT32(2U, RAIN_GAUGE_NVIC_PREEMPT_PRIO);
    TEST_ASSERT_EQUAL_UINT32(0U, RAIN_GAUGE_NVIC_SUB_PRIO);
}

/**
 * @brief TC-S4-T3.1-02: Driver Initialization Default State.
 */
static void test_rain_gauge_init_default_state(void) {
    status_t status = rain_gauge_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    TEST_ASSERT_FALSE(rain_gauge_is_rain_active());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());
}

/**
 * @brief TC-S4-T3.1-03: First Valid Pulse Trigger at arbitrary tick.
 */
static void test_rain_gauge_first_valid_pulse(void) {
    rain_gauge_exti_isr(1000U);

    TEST_ASSERT_TRUE(rain_gauge_is_rain_active());
    TEST_ASSERT_EQUAL_UINT32(1000U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());
}

/**
 * @brief TC-S4-T3.1-04: Bounce Rejection (10ms < 50ms).
 */
static void test_rain_gauge_bounce_rejection_10ms(void) {
    rain_gauge_exti_isr(1000U);
    TEST_ASSERT_EQUAL_UINT32(1000U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());

    /* Pulse 10ms later (mechanical chatter) */
    rain_gauge_exti_isr(1010U);
    TEST_ASSERT_EQUAL_UINT32(1000U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());
}

/**
 * @brief TC-S4-T3.1-05: Cumulative Chatter Bounce Rejection (35ms < 50ms).
 */
static void test_rain_gauge_cumulative_bounce_rejection(void) {
    rain_gauge_exti_isr(1000U);

    /* First bounce at +10ms */
    rain_gauge_exti_isr(1010U);
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());

    /* Second bounce at +35ms */
    rain_gauge_exti_isr(1035U);
    TEST_ASSERT_EQUAL_UINT32(2U, rain_gauge_get_rejected_bounce_count());

    /* Timestamp still locked to initial pulse */
    TEST_ASSERT_EQUAL_UINT32(1000U, rain_gauge_get_last_pulse_timestamp());
}

/**
 * @brief TC-S4-T3.1-06: Second Valid Pulse (>= 50ms).
 */
static void test_rain_gauge_second_valid_pulse(void) {
    rain_gauge_exti_isr(1000U);
    rain_gauge_exti_isr(1010U); /* Chatter #1 */
    rain_gauge_exti_isr(1035U); /* Chatter #2 */
    TEST_ASSERT_EQUAL_UINT32(2U, rain_gauge_get_rejected_bounce_count());

    /* Valid second tip at +55ms (>= 50ms lockout) */
    rain_gauge_exti_isr(1055U);
    TEST_ASSERT_EQUAL_UINT32(1055U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(2U, rain_gauge_get_rejected_bounce_count());
    TEST_ASSERT_TRUE(rain_gauge_is_rain_active());
}

/**
 * @brief Test exact 50ms boundary condition (valid pulse).
 */
static void test_rain_gauge_exact_50ms_boundary(void) {
    rain_gauge_exti_isr(2000U);
    TEST_ASSERT_EQUAL_UINT32(2000U, rain_gauge_get_last_pulse_timestamp());

    /* Exact 50ms elapsed */
    rain_gauge_exti_isr(2050U);
    TEST_ASSERT_EQUAL_UINT32(2050U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());
}

/**
 * @brief Test exact 49ms boundary condition (rejected bounce).
 */
static void test_rain_gauge_exact_49ms_boundary(void) {
    rain_gauge_exti_isr(3000U);
    TEST_ASSERT_EQUAL_UINT32(3000U, rain_gauge_get_last_pulse_timestamp());

    /* Exact 49ms elapsed (< 50ms) */
    rain_gauge_exti_isr(3049U);
    TEST_ASSERT_EQUAL_UINT32(3000U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());
}

/**
 * @brief TC-S4-T3.1-07: Systick Rollover Handling Across 32-Bit Max Boundary.
 */
static void test_rain_gauge_systick_rollover(void) {
    /* Pulse close to 32-bit integer overflow (0xFFFFFFF0 = 4,294,967,280) */
    rain_gauge_exti_isr(0xFFFFFFF0U);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFF0U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());

    /* Contact chatter 12ms later (0xFFFFFFFC) */
    rain_gauge_exti_isr(0xFFFFFFFCU);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFF0U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());

    /* Valid tip across rollover at 0x00000040 (80ms elapsed delta) */
    rain_gauge_exti_isr(0x00000040U);
    TEST_ASSERT_EQUAL_UINT32(0x00000040U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());
}

/**
 * @brief TC-S4-T3.1-08 & TC-S4-T3.1-09: Active Rain Flag and Diagnostic Reset.
 */
static void test_rain_gauge_diagnostic_reset(void) {
    TEST_ASSERT_FALSE(rain_gauge_is_rain_active());

    rain_gauge_exti_isr(5000U);
    rain_gauge_exti_isr(5015U); /* Chatter */
    TEST_ASSERT_TRUE(rain_gauge_is_rain_active());
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());

    /* Reset diagnostics */
    rain_gauge_reset_diagnostics();
    TEST_ASSERT_FALSE(rain_gauge_is_rain_active());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());
    /* Last pulse timestamp remains intact to preserve lockout interval */
    TEST_ASSERT_EQUAL_UINT32(5000U, rain_gauge_get_last_pulse_timestamp());

    /* Secondary chatter at 5025ms is still rejected based on last timestamp */
    rain_gauge_exti_isr(5025U);
    TEST_ASSERT_EQUAL_UINT32(1U, rain_gauge_get_rejected_bounce_count());
    TEST_ASSERT_EQUAL_UINT32(5000U, rain_gauge_get_last_pulse_timestamp());
}

/**
 * @brief TC-S4-T3.1-10: Driver De-Initialization & Error Guards.
 */
static void test_rain_gauge_deinit_and_irq_control(void) {
    /* Test enable_irq when initialized */
    status_t status = rain_gauge_enable_irq(true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    status = rain_gauge_enable_irq(false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* De-initialize */
    status = rain_gauge_deinit();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Calling enable_irq when uninitialized must return STATUS_ERR_NOT_INITIALIZED */
    status = rain_gauge_enable_irq(true);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NOT_INITIALIZED, status);

    /* ISR called when uninitialized must be safely ignored */
    rain_gauge_exti_isr(8000U);
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_FALSE(rain_gauge_is_rain_active());
}

/**
 * @brief Burst Chatter Simulation (10 mechanical bounces within 25ms).
 */
static void test_rain_gauge_burst_chatter_train(void) {
    rain_gauge_exti_isr(10000U); /* Initial valid contact */

    for (uint32_t i = 1; i <= 10; ++i) {
        rain_gauge_exti_isr(10000U + (i * 2)); /* Spurious spikes every 2ms */
    }

    TEST_ASSERT_EQUAL_UINT32(10U, rain_gauge_get_rejected_bounce_count());
    TEST_ASSERT_EQUAL_UINT32(10000U, rain_gauge_get_last_pulse_timestamp());

    /* Valid tip after 60ms */
    rain_gauge_exti_isr(10060U);
    TEST_ASSERT_EQUAL_UINT32(10060U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(10U, rain_gauge_get_rejected_bounce_count());
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_rain_gauge_macro_definitions);
    RUN_TEST(test_rain_gauge_init_default_state);
    RUN_TEST(test_rain_gauge_first_valid_pulse);
    RUN_TEST(test_rain_gauge_bounce_rejection_10ms);
    RUN_TEST(test_rain_gauge_cumulative_bounce_rejection);
    RUN_TEST(test_rain_gauge_second_valid_pulse);
    RUN_TEST(test_rain_gauge_exact_50ms_boundary);
    RUN_TEST(test_rain_gauge_exact_49ms_boundary);
    RUN_TEST(test_rain_gauge_systick_rollover);
    RUN_TEST(test_rain_gauge_diagnostic_reset);
    RUN_TEST(test_rain_gauge_deinit_and_irq_control);
    RUN_TEST(test_rain_gauge_burst_chatter_train);

    return UNITY_END();
}
