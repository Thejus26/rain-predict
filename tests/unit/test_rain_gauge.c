/**
 * @file    test_rain_gauge.c
 * @brief   ThrowTheSwitch Unity unit test suite for Tipping-Bucket Rain Gauge Driver (S4-T3.1 - S4-T3.3).
 * @details Validates GPIO EXTI0 interrupt configuration, 50ms dual-stage debounce filter,
 *          atomic read-and-clear critical sections, rolling 1-hour FIFO accumulation,
 *          daily midnight reset, LoRaWAN Byte 8 telemetry serialization,
 *          instantaneous rate derivative math, time-decay aging, peak tracking,
 *          and 7-tier IMD/WMO meteorological classification.
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

/* ========================================================================== */
/* S4-T3.1 Tests: EXTI & Debounce Filter                                      */
/* ========================================================================== */

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

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 720000.0f, RAIN_RATE_NUMERATOR_MS);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 720.0f, RAIN_RATE_NUMERATOR_SEC);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 300.0f, RAIN_RATE_MAX_MM_HR);
    TEST_ASSERT_EQUAL_UINT32(900000U, RAIN_RATE_INACTIVITY_TIMEOUT_MS);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, RAIN_RATE_ACCELERATION_THRESH_MM_HR);
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
    /* Pulse close to 32-bit integer overflow (0xFFFFFFF0) */
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

/* ========================================================================== */
/* S4-T3.2 Tests: Multi-Horizon Accumulators & Telemetry Byte 8               */
/* ========================================================================== */

/**
 * @brief TC-S4-T3.2-02: Atomic Read and Clear Interval Pulse Counter.
 */
static void test_rain_gauge_atomic_read_and_clear_interval(void) {
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
    TEST_ASSERT_EQUAL_UINT32(45U, data.daily_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.00f, data.daily_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(45U, data.total_lifetime_tips);

    /* Midnight reset */
    rain_gauge_reset_daily();

    (void)rain_gauge_get_accumulation(&data);
    TEST_ASSERT_EQUAL_UINT32(0U, data.daily_tips);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.00f, data.daily_rain_mm);
    TEST_ASSERT_EQUAL_UINT32(45U, data.total_lifetime_tips);
}

/**
 * @brief TC-S4-T3.2-08 & TC-S4-T3.2-09: LoRaWAN Telemetry Byte 8 Encoding & Clamping.
 */
static void test_rain_gauge_telemetry_byte8_encoding(void) {
    TEST_ASSERT_EQUAL_UINT8(0x00U, rain_gauge_encode_telemetry_byte(0U));
    TEST_ASSERT_EQUAL_UINT8(0x01U, rain_gauge_encode_telemetry_byte(1U));
    TEST_ASSERT_EQUAL_UINT8(0x19U, rain_gauge_encode_telemetry_byte(25U));
    TEST_ASSERT_EQUAL_UINT8(0x32U, rain_gauge_encode_telemetry_byte(50U));
    TEST_ASSERT_EQUAL_UINT8(0x7DU, rain_gauge_encode_telemetry_byte(125U));
    TEST_ASSERT_EQUAL_UINT8(0xFFU, rain_gauge_encode_telemetry_byte(255U));
    /* 300 tips clamped to 255 */
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

/* ========================================================================== */
/* S4-T3.3 Tests: Rain Rate Derivative, Peak Tracking & Intensity Classifier  */
/* ========================================================================== */

/**
 * @brief TC-RATE-01: First Tip Initialization State.
 */
static void test_rain_rate_first_tip_initialization(void) {
    /* Single bucket tip at t = 10,000ms */
    rain_gauge_exti_isr(10000U);

    /* Instantaneous rate must be 0.0 mm/hr until 2nd tip establishes delta */
    float inst_rate = rain_gauge_get_instantaneous_rate(10000U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, inst_rate);
}

/**
 * @brief TC-RATE-02: Moderate Rain Inter-Tip Derivative Math (20.0 mm/hr).
 */
static void test_rain_rate_moderate_rain_inter_tip(void) {
    /* Tip 1 at 10,000ms, Tip 2 at 46,000ms (delta = 36,000ms) */
    rain_gauge_exti_isr(10000U);
    rain_gauge_exti_isr(46000U);

    /* Rate: 720,000 / 36,000 = 20.00 mm/hr */
    float inst_rate = rain_gauge_get_instantaneous_rate(46000U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.00f, inst_rate);

    rain_intensity_t intensity = rain_gauge_classify_intensity(inst_rate);
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_HEAVY, intensity);
}

/**
 * @brief TC-RATE-03: Extreme Cloudburst Math (100.0 mm/hr).
 */
static void test_rain_rate_cloudburst_extreme(void) {
    /* Tip 1 at 50,000ms, Tip 2 at 57,200ms (delta = 7,200ms) */
    rain_gauge_exti_isr(50000U);
    rain_gauge_exti_isr(57200U);

    /* Rate: 720,000 / 7,200 = 100.00 mm/hr */
    float inst_rate = rain_gauge_get_instantaneous_rate(57200U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.00f, inst_rate);

    rain_intensity_t intensity = rain_gauge_classify_intensity(inst_rate);
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_CLOUDBURST, intensity);
}

/**
 * @brief TC-RATE-04: Debounce Limit Clamping (14,400 -> 300 mm/hr).
 */
static void test_rain_rate_debounce_limit_clamping(void) {
    /* Two tips at 50ms interval (minimum valid debounce window) */
    rain_gauge_exti_isr(1000U);
    rain_gauge_exti_isr(1050U);

    /* Theoretical 720,000 / 50 = 14,400 mm/hr -> Clamped to 300.00 mm/hr */
    float inst_rate = rain_gauge_get_instantaneous_rate(1050U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 300.00f, inst_rate);
}

/**
 * @brief TC-RATE-05: Time-Decayed Aging Math.
 */
static void test_rain_rate_time_decay_aging(void) {
    /* Tip 1 at 10,000ms, Tip 2 at 17,200ms (delta = 7,200ms -> 100.0 mm/hr) */
    rain_gauge_exti_isr(10000U);
    rain_gauge_exti_isr(17200U);

    /* Rate immediately at tip is 100 mm/hr */
    float inst_rate = rain_gauge_get_instantaneous_rate(17200U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.00f, inst_rate);

    /* Query 36,000ms after 2nd tip (t = 53,200ms): elapsed = 36,000ms */
    /* Decayed rate: 720,000 / 36,000 = 20.00 mm/hr */
    inst_rate = rain_gauge_get_instantaneous_rate(53200U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.00f, inst_rate);
}

/**
 * @brief TC-RATE-06: Inactivity Timeout Zeroing (15 minutes).
 */
static void test_rain_rate_inactivity_timeout(void) {
    rain_gauge_exti_isr(10000U);
    rain_gauge_exti_isr(17200U); /* 100 mm/hr */

    /* Query at t = 17,200 + 900,000ms = 917,200ms (15 minutes elapsed) */
    float inst_rate = rain_gauge_get_instantaneous_rate(917200U);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.00f, inst_rate);

    rain_intensity_t intensity = rain_gauge_classify_intensity(inst_rate);
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_NONE, intensity);
}

/**
 * @brief TC-RATE-07: 10-Minute & 2-Minute Windowed Interval Rates.
 */
static void test_rain_rate_windowed_interval_rates(void) {
    /* 10-minute window (600s): 25 tips -> 25 * 0.20 * 3600 / 600 = 30.00 mm/hr */
    float rate_10m = rain_gauge_compute_interval_rate(25U, 600U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.00f, rate_10m);

    /* 2-minute accelerated window (120s): 5 tips -> 5 * 0.20 * 3600 / 120 = 30.00 mm/hr */
    float rate_2m = rain_gauge_compute_interval_rate(5U, 120U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.00f, rate_2m);

    /* Zero duration defensive safety guard */
    float zero_rate = rain_gauge_compute_interval_rate(10U, 0U);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.00f, zero_rate);
}

/**
 * @brief TC-RATE-08: 7-Tier Meteorological Classification & String Conversion.
 */
static void test_rain_rate_intensity_classification_and_strings(void) {
    /* 0.0 mm/hr -> None */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_NONE, rain_gauge_classify_intensity(0.0f));
    TEST_ASSERT_EQUAL_STRING("None", rain_gauge_intensity_to_str(RAIN_INTENSITY_NONE));

    /* 1.2 mm/hr -> Drizzle */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_DRIZZLE, rain_gauge_classify_intensity(1.2f));
    TEST_ASSERT_EQUAL_STRING("Drizzle", rain_gauge_intensity_to_str(RAIN_INTENSITY_DRIZZLE));

    /* 5.0 mm/hr -> Light */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_LIGHT, rain_gauge_classify_intensity(5.0f));
    TEST_ASSERT_EQUAL_STRING("Light", rain_gauge_intensity_to_str(RAIN_INTENSITY_LIGHT));

    /* 12.0 mm/hr -> Moderate */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_MODERATE, rain_gauge_classify_intensity(12.0f));
    TEST_ASSERT_EQUAL_STRING("Moderate", rain_gauge_intensity_to_str(RAIN_INTENSITY_MODERATE));

    /* 25.0 mm/hr -> Heavy */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_HEAVY, rain_gauge_classify_intensity(25.0f));
    TEST_ASSERT_EQUAL_STRING("Heavy", rain_gauge_intensity_to_str(RAIN_INTENSITY_HEAVY));

    /* 60.0 mm/hr -> Torrential */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_TORRENTIAL, rain_gauge_classify_intensity(60.0f));
    TEST_ASSERT_EQUAL_STRING("Torrential", rain_gauge_intensity_to_str(RAIN_INTENSITY_TORRENTIAL));

    /* 150.0 mm/hr -> Cloudburst */
    TEST_ASSERT_EQUAL_INT(RAIN_INTENSITY_CLOUDBURST, rain_gauge_classify_intensity(150.0f));
    TEST_ASSERT_EQUAL_STRING("Cloudburst", rain_gauge_intensity_to_str(RAIN_INTENSITY_CLOUDBURST));
}

/**
 * @brief TC-RATE-09: Storm Tracking Acceleration Threshold (5.0 mm/hr).
 */
static void test_rain_rate_sampling_acceleration_trigger(void) {
    TEST_ASSERT_FALSE(rain_gauge_should_accelerate_sampling(0.0f));
    TEST_ASSERT_FALSE(rain_gauge_should_accelerate_sampling(4.9f));
    TEST_ASSERT_TRUE(rain_gauge_should_accelerate_sampling(5.0f));
    TEST_ASSERT_TRUE(rain_gauge_should_accelerate_sampling(5.1f));
    TEST_ASSERT_TRUE(rain_gauge_should_accelerate_sampling(50.0f));
}

/**
 * @brief TC-RATE-10: Peak Rate Tracking & Reset.
 */
static void test_rain_rate_peak_tracking_and_reset(void) {
    /* Step 1: 10 mm/hr -> delta = 72,000ms */
    rain_gauge_exti_isr(10000U);
    rain_gauge_exti_isr(82000U);
    (void)rain_gauge_get_instantaneous_rate(82000U);

    /* Step 2: 45 mm/hr -> delta = 16,000ms */
    rain_gauge_exti_isr(98000U);
    (void)rain_gauge_get_instantaneous_rate(98000U);

    /* Step 3: 20 mm/hr -> delta = 36,000ms */
    rain_gauge_exti_isr(134000U);
    (void)rain_gauge_get_instantaneous_rate(134000U);

    rain_rate_metrics_t metrics;
    status_t status = rain_gauge_get_rate_metrics(134000U, 600U, &metrics);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.00f, metrics.instantaneous_rate_mm_hr);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.00f, metrics.peak_instantaneous_mm_hr);

    /* Reset peaks */
    rain_gauge_reset_peak_rates();

    status = rain_gauge_get_rate_metrics(134000U, 600U, &metrics);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.00f, metrics.peak_instantaneous_mm_hr);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.00f, metrics.peak_interval_mm_hr);

    /* Test NULL pointer guard */
    status = rain_gauge_get_rate_metrics(134000U, 600U, NULL);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);
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

    /* S4-T3.3 Tests */
    RUN_TEST(test_rain_rate_first_tip_initialization);
    RUN_TEST(test_rain_rate_moderate_rain_inter_tip);
    RUN_TEST(test_rain_rate_cloudburst_extreme);
    RUN_TEST(test_rain_rate_debounce_limit_clamping);
    RUN_TEST(test_rain_rate_time_decay_aging);
    RUN_TEST(test_rain_rate_inactivity_timeout);
    RUN_TEST(test_rain_rate_windowed_interval_rates);
    RUN_TEST(test_rain_rate_intensity_classification_and_strings);
    RUN_TEST(test_rain_rate_sampling_acceleration_trigger);
    RUN_TEST(test_rain_rate_peak_tracking_and_reset);

    return UNITY_END();
}
