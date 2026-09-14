/**
 * @file    test_rain_gauge.c
 * @brief   ThrowTheSwitch Unity unit test suite for Tipping-Bucket Rain Gauge Driver (S4-T3.1 - S4-T3.2).
 * @details Validates GPIO EXTI0 interrupt configuration, 50ms dual-stage debounce filter,
 *          atomic read-and-clear critical sections, rolling 1-hour FIFO accumulation,
 *          daily midnight reset, and LoRaWAN Byte 8 telemetry serialization.
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
 * @brief TC-S4-T3.1-01 & TC-S4-T3.2-01: Header Definitions & Macro Constants.
 */
static void test_rain_gauge_macro_definitions(void) {
    TEST_ASSERT_EQUAL_UINT32(50U, RAIN_GAUGE_DEBOUNCE_MS);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.20f, RAIN_GAUGE_CALIB_MM_PER_TIP);
    TEST_ASSERT_EQUAL_UINT32(2U, RAIN_GAUGE_NVIC_PREEMPT_PRIO);
    TEST_ASSERT_EQUAL_UINT32(0U, RAIN_GAUGE_NVIC_SUB_PRIO);
    TEST_ASSERT_EQUAL_UINT32(6U, RAIN_GAUGE_HOURLY_FIFO_SIZE);
    TEST_ASSERT_EQUAL_UINT32(255U, RAIN_GAUGE_TELEMETRY_MAX_TIPS);
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

    rain_gauge_data_t data;
    status = rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(0U, data.interval_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.interval_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(0U, data.hourly_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.hourly_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(0U, data.daily_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.daily_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(0U, data.total_lifetime_tips);
    TEST_ASSERT_EQUAL_UINT8(0U, data.telemetry_byte8);
}

/**
 * @brief TC-S4-T3.1-03: First Valid Pulse Trigger at arbitrary tick.
 */
static void test_rain_gauge_first_valid_pulse(void) {
    rain_gauge_exti_isr(1000U);

    TEST_ASSERT_TRUE(rain_gauge_is_rain_active());
    TEST_ASSERT_EQUAL_UINT32(1000U, rain_gauge_get_last_pulse_timestamp());
    TEST_ASSERT_EQUAL_UINT32(0U, rain_gauge_get_rejected_bounce_count());

    rain_gauge_data_t data;
    (void)rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_UINT16(1U, data.interval_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.20f, data.interval_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(1U, data.hourly_tips);
    TEST_ASSERT_EQUAL_UINT32(1U, data.daily_tips);
    TEST_ASSERT_EQUAL_UINT32(1U, data.total_lifetime_tips);
    TEST_ASSERT_EQUAL_UINT8(1U, data.telemetry_byte8);
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

    rain_gauge_data_t data;
    (void)rain_gauge_get_accumulation(&data);
    /* Tip count must not increase on chatter */
    TEST_ASSERT_EQUAL_UINT16(1U, data.interval_tips);
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

    rain_gauge_data_t data;
    (void)rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_UINT16(2U, data.interval_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.40f, data.interval_rain_mm);
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
 * @brief TC-S4-T3.2-02: Atomic Read and Clear Interval Pulse Counter.
 */
static void test_rain_gauge_atomic_read_and_clear_interval(void) {
    /* Simulate 5 bucket tips at 60ms intervals */
    for (uint32_t i = 0; i < 5; ++i) {
        rain_gauge_exti_isr(1000U + (i * 60U));
    }

    float interval_mm = 0.0f;
    uint16_t tips = rain_gauge_read_and_clear_interval(&interval_mm);
    TEST_ASSERT_EQUAL_UINT16(5U, tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.00f, interval_mm);

    /* Immediate subsequent read must return 0 */
    interval_mm = 99.0f;
    tips = rain_gauge_read_and_clear_interval(&interval_mm);
    TEST_ASSERT_EQUAL_UINT16(0U, tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.00f, interval_mm);

    /* Test NULL parameter tolerance */
    for (uint32_t i = 0; i < 3; ++i) {
        rain_gauge_exti_isr(2000U + (i * 60U));
    }
    tips = rain_gauge_read_and_clear_interval(NULL);
    TEST_ASSERT_EQUAL_UINT16(3U, tips);
}

/**
 * @brief TC-S4-T3.2-03: Metric Calibration Factor (0.20 mm per tip).
 */
static void test_rain_gauge_calibration_math(void) {
    /* Simulate 25 tips (5.00 mm) */
    for (uint32_t i = 0; i < 25; ++i) {
        rain_gauge_exti_isr(10000U + (i * 60U));
    }

    rain_gauge_data_t data;
    status_t status = rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(25U, data.interval_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.00f, data.interval_rain_mm);
}

/**
 * @brief TC-S4-T3.2-04: Rolling 1-Hour FIFO Ring Buffer Accumulation.
 */
static void test_rain_gauge_rolling_hourly_fifo(void) {
    const uint16_t intervals[6] = {10U, 5U, 15U, 0U, 20U, 10U};

    for (uint8_t i = 0; i < 6; ++i) {
        rain_gauge_update_hourly_history(intervals[i]);
    }

    rain_gauge_data_t data;
    status_t status = rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Sum: 10 + 5 + 15 + 0 + 20 + 10 = 60 tips (12.00 mm) */
    TEST_ASSERT_EQUAL_UINT32(60U, data.hourly_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.00f, data.hourly_rain_mm);
}

/**
 * @brief TC-S4-T3.2-05: Hourly FIFO Overwrite (7th interval replaces oldest).
 */
static void test_rain_gauge_hourly_fifo_overwrite(void) {
    const uint16_t intervals[6] = {10U, 5U, 15U, 0U, 20U, 10U};

    for (uint8_t i = 0; i < 6; ++i) {
        rain_gauge_update_hourly_history(intervals[i]);
    }

    /* Push 7th interval (8 tips), overwriting the 1st interval (10 tips) */
    rain_gauge_update_hourly_history(8U);

    rain_gauge_data_t data;
    status_t status = rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* New sum: 60 - 10 + 8 = 58 tips (11.60 mm) */
    TEST_ASSERT_EQUAL_UINT32(58U, data.hourly_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.60f, data.hourly_rain_mm);
}

/**
 * @brief TC-S4-T3.2-06 & TC-S4-T3.2-07: Daily Accumulator Persistence & Midnight Reset.
 */
static void test_rain_gauge_daily_persistence_and_reset(void) {
    /* Interval 1: 10 tips */
    for (uint32_t i = 0; i < 10; ++i) {
        rain_gauge_exti_isr(1000U + (i * 60U));
    }
    (void)rain_gauge_read_and_clear_interval(NULL);

    /* Interval 2: 20 tips */
    for (uint32_t i = 0; i < 20; ++i) {
        rain_gauge_exti_isr(2000U + (i * 60U));
    }
    (void)rain_gauge_read_and_clear_interval(NULL);

    /* Interval 3: 15 tips */
    for (uint32_t i = 0; i < 15; ++i) {
        rain_gauge_exti_isr(3000U + (i * 60U));
    }

    rain_gauge_data_t data;
    (void)rain_gauge_get_accumulation(&data);
    /* Daily total: 10 + 20 + 15 = 45 tips (9.00 mm) */
    TEST_ASSERT_EQUAL_UINT32(45U, data.daily_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.00f, data.daily_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(45U, data.total_lifetime_tips);

    /* Midnight reset */
    rain_gauge_reset_daily();

    (void)rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_UINT32(0U, data.daily_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.00f, data.daily_rain_mm);
    /* Total lifetime count must NOT be cleared on daily reset */
    TEST_ASSERT_EQUAL_UINT32(45U, data.total_lifetime_tips);
}

/**
 * @brief TC-S4-T3.2-08 & TC-S4-T3.2-09: LoRaWAN Telemetry Byte 8 Encoding & Clamping.
 */
static void test_rain_gauge_telemetry_byte8_encoding(void) {
    /* 0 tips = 0 */
    TEST_ASSERT_EQUAL_UINT8(0x00U, rain_gauge_encode_telemetry_byte(0U));

    /* 1 tip = 1 (0.2 mm) */
    TEST_ASSERT_EQUAL_UINT8(0x01U, rain_gauge_encode_telemetry_byte(1U));

    /* 25 tips = 25 (5.0 mm) */
    TEST_ASSERT_EQUAL_UINT8(0x19U, rain_gauge_encode_telemetry_byte(25U));

    /* 50 tips = 50 (10.0 mm) */
    TEST_ASSERT_EQUAL_UINT8(0x32U, rain_gauge_encode_telemetry_byte(50U));

    /* 125 tips = 125 (25.0 mm) */
    TEST_ASSERT_EQUAL_UINT8(0x7DU, rain_gauge_encode_telemetry_byte(125U));

    /* 255 tips = 255 (51.0 mm) */
    TEST_ASSERT_EQUAL_UINT8(0xFFU, rain_gauge_encode_telemetry_byte(255U));

    /* 300 tips (cloudburst overflow) clamped at 255 (0xFF) */
    TEST_ASSERT_EQUAL_UINT8(0xFFU, rain_gauge_encode_telemetry_byte(300U));
}

/**
 * @brief TC-S4-T3.2-10: Complete Multi-Horizon Accumulation Telemetry & NULL Guards.
 */
static void test_rain_gauge_get_accumulation_null_guard(void) {
    status_t status = rain_gauge_get_accumulation(NULL);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);
}

/**
 * @brief Reset All Accumulators Verification.
 */
static void test_rain_gauge_reset_all_accumulators(void) {
    for (uint32_t i = 0; i < 10; ++i) {
        rain_gauge_exti_isr(1000U + (i * 60U));
    }
    rain_gauge_update_hourly_history(10U);

    rain_gauge_data_t data;
    (void)rain_gauge_get_accumulation(&data);
    TEST_ASSERT_TRUE(data.interval_tips > 0);
    TEST_ASSERT_TRUE(data.total_lifetime_tips > 0);

    rain_gauge_reset_all_accumulators();

    (void)rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_UINT16(0U, data.interval_tips);
    TEST_ASSERT_EQUAL_UINT32(0U, data.hourly_tips);
    TEST_ASSERT_EQUAL_UINT32(0U, data.daily_tips);
    TEST_ASSERT_EQUAL_UINT32(0U, data.total_lifetime_tips);
}

int main(void) {
    UNITY_BEGIN();

    /* S4-T3.1 Tests */
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

    /* S4-T3.2 Tests */
    RUN_TEST(test_rain_gauge_atomic_read_and_clear_interval);
    RUN_TEST(test_rain_gauge_calibration_math);
    RUN_TEST(test_rain_gauge_rolling_hourly_fifo);
    RUN_TEST(test_rain_gauge_hourly_fifo_overwrite);
    RUN_TEST(test_rain_gauge_daily_persistence_and_reset);
    RUN_TEST(test_rain_gauge_telemetry_byte8_encoding);
    RUN_TEST(test_rain_gauge_get_accumulation_null_guard);
    RUN_TEST(test_rain_gauge_reset_all_accumulators);

    return UNITY_END();
}
