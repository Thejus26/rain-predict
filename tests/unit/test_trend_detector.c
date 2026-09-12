/**
 * @file test_trend_detector.c
 * @brief Unit tests for multi-variable atmospheric gradient detection engine.
 * 
 * Target: Host Unit Testing (ThrowTheSwitch Unity)
 */

#include "unity.h"
#include "trend_detector.h"
#include <math.h>
#include <string.h>

void setUp(void)
{
    /* No mutable global state; pure struct calculations */
}

void tearDown(void)
{
    /* No state cleanup required */
}

static void test_trend_detector_gradients(void)
{
    /* Construct 19 chronological samples (index 0 = oldest, 18 = newest) */
    env_sample_t history[19];
    for (uint32_t i = 0; i < 19; i++) {
        history[i].p0_hpa = 1010.0f - ((float)i * (3.0f / 18.0f));     /* Drops by 3.0 hPa over 3h */
        history[i].rh_pct = 60.0f + ((float)i * (20.0f / 18.0f));      /* Rises by 20% over 3h */
        history[i].temp_c = 25.0f - ((float)i * (4.0f / 18.0f));       /* Drops by 4.0°C over 3h */
        history[i].lux = 60000.0f - ((float)i * (50000.0f / 18.0f));   /* Drops from 60k to 10k Lux */
        history[i].timestamp_s = 1000U + (i * 600U);
    }

    multi_gradient_t grad;
    TEST_ASSERT_EQUAL_INT(0, trend_detector_compute_gradients(history, 19, &grad));

    /* 3-hour delta (sample 18 vs sample 0) */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -3.00f, grad.delta_p_3h_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.00f, grad.delta_rh_3h_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -4.00f, grad.delta_t_3h_c);
    TEST_ASSERT_TRUE(grad.is_history_complete);

    /* 1-hour delta (sample 18 vs sample 12, 6 samples) */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -1.00f, grad.delta_p_1h_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 6.67f, grad.delta_rh_1h_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -1.33f, grad.delta_t_1h_c);

    /* 30-min solar drop (sample 18 vs sample 15, 3 samples) */
    /* Lux at 15 is 60k - 15*(50k/18) = 18333.33 Lux. Lux at 18 is 10000 Lux. Drop = 8333.33 Lux (~45.45%) */
    TEST_ASSERT_FLOAT_WITHIN(100.0f, -8333.3f, grad.delta_lux_30m);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 45.45f, grad.solar_drop_pct_30m);

    /* Partial history scaled test: 7 samples (6 intervals = 1 hour) */
    multi_gradient_t partial_grad;
    TEST_ASSERT_EQUAL_INT(0, trend_detector_compute_gradients(history, 7, &partial_grad));
    TEST_ASSERT_FALSE(partial_grad.is_history_complete);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -3.00f, partial_grad.delta_p_3h_hpa);

    /* Partial history scaled test: 2 samples (1 interval = 10 min) */
    multi_gradient_t short_grad;
    TEST_ASSERT_EQUAL_INT(0, trend_detector_compute_gradients(history, 2, &short_grad));
    TEST_ASSERT_FALSE(short_grad.is_history_complete);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -1.00f, short_grad.delta_p_1h_hpa);

    /* Empty sample test */
    TEST_ASSERT_TRUE(trend_detector_compute_gradients(history, 0, &grad) < 0);
}

static void test_trend_detector_pressure_classification_and_scoring(void)
{
    pressure_trend_state_t state;
    uint8_t score = 0;

    /* TC-PS-01: Severe Squall (-3.5 hPa/3h) -> RAPID_DROP, Score 100 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(-1.0f, -3.50f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_RAPID_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(-1.0f, -3.50f, &score));
    TEST_ASSERT_EQUAL_UINT8(100, score);

    /* TC-PS-02: Rapid Drop (-2.2 hPa/3h) -> RAPID_DROP, Score 80 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(-0.8f, -2.20f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_RAPID_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(-0.8f, -2.20f, &score));
    TEST_ASSERT_EQUAL_UINT8(80, score);

    /* TC-PS-03: Sudden 1-Hour Squall (-1.6 hPa/1h, -1.2 hPa/3h) -> RAPID_DROP, Score 90 (override) */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(-1.60f, -1.20f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_RAPID_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(-1.60f, -1.20f, &score));
    TEST_ASSERT_EQUAL_UINT8(90, score);

    /* TC-PS-04: Moderate Trough (-1.5 hPa/3h) -> MODERATE_DROP, Score 50 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(-0.4f, -1.50f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_MODERATE_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(-0.4f, -1.50f, &score));
    TEST_ASSERT_EQUAL_UINT8(50, score);

    /* TC-PS-05: Diurnal Drift (-0.6 hPa/3h) -> SLOW_DROP, Score 20 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(-0.1f, -0.60f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_SLOW_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(-0.1f, -0.60f, &score));
    TEST_ASSERT_EQUAL_UINT8(20, score);

    /* TC-PS-06: Steady (+0.5 hPa/3h) -> STEADY, Score 0 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(0.0f, 0.50f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_STEADY, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(0.0f, 0.50f, &score));
    TEST_ASSERT_EQUAL_UINT8(0, score);

    /* TC-PS-07: Rising (+1.8 hPa/3h) -> RISING, Score 0 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_pressure(0.5f, 1.80f, &state));
    TEST_ASSERT_EQUAL_INT(PRESSURE_RISING, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_pressure(0.5f, 1.80f, &score));
    TEST_ASSERT_EQUAL_UINT8(0, score);

    /* String lookups */
    TEST_ASSERT_NOT_NULL(trend_detector_get_pressure_state_name(PRESSURE_RAPID_DROP));
    TEST_ASSERT_NOT_NULL(trend_detector_get_pressure_state_name(PRESSURE_MODERATE_DROP));
    TEST_ASSERT_NOT_NULL(trend_detector_get_pressure_state_name(PRESSURE_SLOW_DROP));
    TEST_ASSERT_NOT_NULL(trend_detector_get_pressure_state_name(PRESSURE_STEADY));
    TEST_ASSERT_NOT_NULL(trend_detector_get_pressure_state_name(PRESSURE_RISING));
    TEST_ASSERT_EQUAL_STRING("Unknown Pressure State", trend_detector_get_pressure_state_name((pressure_trend_state_t)99));
}

static void test_trend_detector_solar_classification_and_scoring(void)
{
    solar_cloud_state_t state;
    uint8_t score = 0;

    /* TC-SO-01: Cumulonimbus Blackout (60k -> 1.5k, 97.5% drop) -> STORM_CLOUD, Score 100 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_solar(1500.0f, 60000.0f, 97.5f, &state));
    TEST_ASSERT_EQUAL_INT(SOLAR_STORM_CLOUD_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_solar(1500.0f, 60000.0f, 97.5f, &score));
    TEST_ASSERT_EQUAL_UINT8(100, score);

    /* TC-SO-02: Moderate Storm Cloud (50k -> 20k, 60% drop) -> STORM_CLOUD, Score 65 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_solar(20000.0f, 50000.0f, 60.0f, &state));
    TEST_ASSERT_EQUAL_INT(SOLAR_STORM_CLOUD_DROP, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_solar(20000.0f, 50000.0f, 60.0f, &score));
    TEST_ASSERT_EQUAL_UINT8(65, score);

    /* TC-SO-03: Mild Cloud Shadow (60k -> 38k, 36.7% drop) -> SCATTERED, Score 30 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_solar(38000.0f, 60000.0f, 36.7f, &state));
    TEST_ASSERT_EQUAL_INT(SOLAR_SCATTERED, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_solar(38000.0f, 60000.0f, 36.7f, &score));
    TEST_ASSERT_EQUAL_UINT8(30, score);

    /* TC-SO-04: Clear Sunshine (80k -> 78k, 2.5% drop) -> CLEAR, Score 0 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_solar(78000.0f, 80000.0f, 2.5f, &state));
    TEST_ASSERT_EQUAL_INT(SOLAR_CLEAR, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_solar(78000.0f, 80000.0f, 2.5f, &score));
    TEST_ASSERT_EQUAL_UINT8(0, score);

    /* TC-SO-05: Nighttime Gating (20 -> 10 Lux, 50% drop) -> NIGHT, Score 0 */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_solar(10.0f, 20.0f, 50.0f, &state));
    TEST_ASSERT_EQUAL_INT(SOLAR_NIGHT, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_solar(10.0f, 20.0f, 50.0f, &score));
    TEST_ASSERT_EQUAL_UINT8(0, score);

    /* TC-SO-06: Low Twilight Gating (3000 -> 1000 Lux) -> SCATTERED, Score 0 (< 5000 Lux prior) */
    TEST_ASSERT_EQUAL_INT(0, trend_detector_classify_solar(1000.0f, 3000.0f, 66.7f, &state));
    TEST_ASSERT_EQUAL_INT(SOLAR_SCATTERED, state);
    TEST_ASSERT_EQUAL_INT(0, trend_detector_score_solar(1000.0f, 3000.0f, 66.7f, &score));
    TEST_ASSERT_EQUAL_UINT8(0, score);

    /* String lookups */
    TEST_ASSERT_NOT_NULL(trend_detector_get_solar_state_name(SOLAR_NIGHT));
    TEST_ASSERT_NOT_NULL(trend_detector_get_solar_state_name(SOLAR_CLEAR));
    TEST_ASSERT_NOT_NULL(trend_detector_get_solar_state_name(SOLAR_SCATTERED));
    TEST_ASSERT_NOT_NULL(trend_detector_get_solar_state_name(SOLAR_STORM_CLOUD_DROP));
    TEST_ASSERT_EQUAL_STRING("Unknown Solar State", trend_detector_get_solar_state_name((solar_cloud_state_t)99));
}

static void test_trend_detector_humidity_and_temp_derivatives(void)
{
    env_sample_t samples[7];
    for (uint32_t i = 0; i < 7; i++) {
        samples[i].p0_hpa = 1005.0f;
        samples[i].rh_pct = 70.0f + ((float)i * (15.0f / 6.0f));    /* +15% in 1 hour */
        samples[i].temp_c = 28.0f - ((float)i * (5.0f / 6.0f));     /* -5°C in 1 hour */
        samples[i].lux = 40000.0f;
        samples[i].timestamp_s = i * 600U;
    }

    multi_gradient_t grad;
    TEST_ASSERT_EQUAL_INT(0, trend_detector_compute_gradients(samples, 7, &grad));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 15.00f, grad.delta_rh_1h_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -5.00f, grad.delta_t_1h_c);
}

static void test_trend_detector_defensive_guards(void)
{
    multi_gradient_t grad;
    pressure_trend_state_t p_state;
    solar_cloud_state_t s_state;
    uint8_t score = 0;

    env_sample_t sample = {
        .p0_hpa = 1013.25f,
        .rh_pct = 65.0f,
        .temp_c = 22.0f,
        .lux = 50000.0f,
        .timestamp_s = 1000U
    };

    /* Null pointer traps */
    TEST_ASSERT_TRUE(trend_detector_compute_gradients(NULL, 10, &grad) < 0);
    TEST_ASSERT_TRUE(trend_detector_compute_gradients(&sample, 1, NULL) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_pressure(0.0f, 0.0f, NULL) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_pressure(0.0f, 0.0f, NULL) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_solar(1000.0f, 1000.0f, 0.0f, NULL) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_solar(1000.0f, 1000.0f, 0.0f, NULL) < 0);

    /* NaN float inputs */
    TEST_ASSERT_TRUE(trend_detector_classify_pressure(NAN, 0.0f, &p_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_pressure(0.0f, NAN, &p_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_pressure(NAN, 0.0f, &score) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_pressure(0.0f, NAN, &score) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_solar(NAN, 1000.0f, 0.0f, &s_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_solar(1000.0f, NAN, 0.0f, &s_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_solar(1000.0f, 1000.0f, NAN, &s_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_solar(NAN, 1000.0f, 0.0f, &score) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_solar(1000.0f, NAN, 0.0f, &score) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_solar(1000.0f, 1000.0f, NAN, &score) < 0);

    /* Infinity float inputs */
    TEST_ASSERT_TRUE(trend_detector_classify_pressure(INFINITY, 0.0f, &p_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_pressure(0.0f, INFINITY, &score) < 0);
    TEST_ASSERT_TRUE(trend_detector_classify_solar(INFINITY, 1000.0f, 0.0f, &s_state) < 0);
    TEST_ASSERT_TRUE(trend_detector_score_solar(1000.0f, INFINITY, 0.0f, &score) < 0);

    /* Corrupted sample with NaN */
    env_sample_t bad_sample = {
        .p0_hpa = NAN,
        .rh_pct = 65.0f,
        .temp_c = 22.0f,
        .lux = 50000.0f,
        .timestamp_s = 1000U
    };
    TEST_ASSERT_TRUE(trend_detector_compute_gradients(&bad_sample, 1, &grad) < 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_trend_detector_gradients);
    RUN_TEST(test_trend_detector_pressure_classification_and_scoring);
    RUN_TEST(test_trend_detector_solar_classification_and_scoring);
    RUN_TEST(test_trend_detector_humidity_and_temp_derivatives);
    RUN_TEST(test_trend_detector_defensive_guards);
    return UNITY_END();
}
