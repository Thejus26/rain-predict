/**
 * @file    test_moving_avg.c
 * @brief   Comprehensive unit tests for moving-average and outlier rejection filter.
 * @details Validates sliding window math across multiple sizes (3, 4, 6, 12), O(1) constant-time
 *          floating-point stability, spike suppression / clamping, priming states, negative values,
 *          and defensive NULL/boundary error handling.
 */

#include "unity.h"
#include "moving_avg_filter.h"
#include <string.h>
#include <math.h>

#define MAX_STORAGE_SIZE 32U

static float s_filter_storage[MAX_STORAGE_SIZE];
static moving_avg_filter_t s_filter;

void setUp(void) {
    (void)memset(s_filter_storage, 0, sizeof(s_filter_storage));
    (void)memset(&s_filter, 0, sizeof(s_filter));
}

void tearDown(void) {
}

/**
 * @brief Test initialization parameter validation, zero-capacity rejection, and NULL pointer guards.
 */
static void test_moving_avg_init_validation(void) {
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR,
                      moving_avg_init(NULL, s_filter_storage, 4, false, 0.0f));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR,
                      moving_avg_init(&s_filter, NULL, 4, false, 0.0f));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM,
                      moving_avg_init(&s_filter, s_filter_storage, 0, false, 0.0f));
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, false, 0.0f));

    TEST_ASSERT_EQUAL_UINT16(0, moving_avg_get_count(&s_filter));
    TEST_ASSERT_FALSE(moving_avg_is_primed(&s_filter));
}

/**
 * @brief Test sliding window calculations across standard meteorological window sizes (3, 4, 6, 12).
 */
static void test_moving_avg_sliding_window_sizes(void) {
    float out = 0.0f;

    /* Window Size 3 */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 3, false, 0.0f));
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 10.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, out);

    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 20.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, out);

    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 30.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, out);
    TEST_ASSERT_TRUE(moving_avg_is_primed(&s_filter));

    /* Push 4th element (40.0): buffer becomes [20, 30, 40], avg = 30.0 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 40.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, out);

    /* Push 5th element (50.0): buffer becomes [30, 40, 50], avg = 40.0 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 50.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, out);

    /* Window Size 4 (1-Hour Trend Window at 15-min intervals) */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, false, 0.0f));
    (void)moving_avg_update(&s_filter, 100.0f, &out);
    (void)moving_avg_update(&s_filter, 102.0f, &out);
    (void)moving_avg_update(&s_filter, 104.0f, &out);
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 106.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 103.0f, out);
    TEST_ASSERT_TRUE(moving_avg_is_primed(&s_filter));

    /* Push 5th value 108.0 -> [102, 104, 106, 108], avg = 105.0 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 108.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 105.0f, out);

    /* Window Size 6 */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 6, false, 0.0f));
    for (size_t i = 0; i < 5; i++) {
        (void)moving_avg_update(&s_filter, 5.0f, &out);
    }
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 5.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, out);
    TEST_ASSERT_TRUE(moving_avg_is_primed(&s_filter));

    /* Push 11.0 -> (5*5 + 11) / 6 = 36.0 / 6 = 6.0 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 11.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, out);

    /* Window Size 12 (3-Hour Trend Window at 15-min intervals) */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 12, false, 0.0f));
    for (size_t i = 0; i < 12; i++) {
        (void)moving_avg_update(&s_filter, 12.0f, &out);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, out);
    TEST_ASSERT_TRUE(moving_avg_is_primed(&s_filter));

    /* Push 24.0 -> (11 * 12.0 + 24.0) / 12 = 156.0 / 12 = 13.0 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 24.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 13.0f, out);
}

/**
 * @brief Test O(1) running accumulator floating-point precision stability over 10,000 iterations.
 */
static void test_moving_avg_constant_time_stability(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 12, false, 0.0f));

    float out = 0.0f;
    /* Push 10,000 continuous alternating samples: 24.0 and 26.0 (expected average = 25.0) */
    for (int i = 0; i < 10000; i++) {
        float sample = (i % 2 == 0) ? 24.0f : 26.0f;
        TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, sample, &out));
    }
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, out);

    float stored_avg = 0.0f;
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_get_average(&s_filter, &stored_avg));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, stored_avg);
}

/**
 * @brief Test statistical outlier detection and threshold clamping.
 */
static void test_moving_avg_outlier_clamping(void) {
    /* Initialize with threshold of 5.0 C */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, true, 5.0f));

    float out = 0.0f;
    /* Establish baseline of 20.0 C across 3 samples (minimum for outlier filter activation) */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 20.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 20.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 20.0f, &out));

    /* Push positive spike of 100.0 C -> should clamp to current_avg (20.0) + threshold (5.0) = 25.0 C */
    /* Buffer becomes [20, 20, 20, 25], sum = 85.0, avg = 85.0 / 4 = 21.25 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 100.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.25f, out);

    /* Push negative spike of -50.0 C -> current_avg = 21.25, delta = 71.25 > 5.0 */
    /* Clamps to 21.25 - 5.0 = 16.25 C */
    /* Buffer replaces oldest 20 with 16.25 -> [16.25, 20, 20, 25], sum = 81.25, avg = 81.25 / 4 = 20.3125 */
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, -50.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.3125f, out);

    /* Test that outlier filter disabled does NOT clamp */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, false, 5.0f));
    (void)moving_avg_update(&s_filter, 20.0f, &out);
    (void)moving_avg_update(&s_filter, 20.0f, &out);
    (void)moving_avg_update(&s_filter, 20.0f, &out);
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, 100.0f, &out));
    /* Buffer [20, 20, 20, 100], sum = 160.0, avg = 40.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, out);
}

/**
 * @brief Test priming state transitions during window warm-up.
 */
static void test_moving_avg_priming_indicator(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, false, 0.0f));

    float out = 0.0f;
    TEST_ASSERT_FALSE(moving_avg_is_primed(&s_filter));
    TEST_ASSERT_EQUAL_UINT16(0, moving_avg_get_count(&s_filter));

    (void)moving_avg_update(&s_filter, 1.0f, &out);
    TEST_ASSERT_FALSE(moving_avg_is_primed(&s_filter));
    TEST_ASSERT_EQUAL_UINT16(1, moving_avg_get_count(&s_filter));

    (void)moving_avg_update(&s_filter, 2.0f, &out);
    TEST_ASSERT_FALSE(moving_avg_is_primed(&s_filter));
    TEST_ASSERT_EQUAL_UINT16(2, moving_avg_get_count(&s_filter));

    (void)moving_avg_update(&s_filter, 3.0f, &out);
    TEST_ASSERT_FALSE(moving_avg_is_primed(&s_filter));
    TEST_ASSERT_EQUAL_UINT16(3, moving_avg_get_count(&s_filter));

    (void)moving_avg_update(&s_filter, 4.0f, &out);
    TEST_ASSERT_TRUE(moving_avg_is_primed(&s_filter));
    TEST_ASSERT_EQUAL_UINT16(4, moving_avg_get_count(&s_filter));

    (void)moving_avg_update(&s_filter, 5.0f, &out);
    TEST_ASSERT_TRUE(moving_avg_is_primed(&s_filter));
    TEST_ASSERT_EQUAL_UINT16(4, moving_avg_get_count(&s_filter));
}

/**
 * @brief Test sub-zero negative temperatures and near-zero precision handling.
 */
static void test_moving_avg_negative_and_zero_values(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, false, 0.0f));

    float out = 0.0f;
    (void)moving_avg_update(&s_filter, -10.0f, &out);
    (void)moving_avg_update(&s_filter, -20.0f, &out);
    (void)moving_avg_update(&s_filter, -30.0f, &out);
    TEST_ASSERT_EQUAL(STATUS_OK, moving_avg_update(&s_filter, -40.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -25.0f, out);

    /* Zero-crossing / near zero sum protection */
    (void)moving_avg_update(&s_filter, 10.0f, &out);
    (void)moving_avg_update(&s_filter, 20.0f, &out);
    (void)moving_avg_update(&s_filter, 30.0f, &out);
    (void)moving_avg_update(&s_filter, -60.0f, &out);
    /* Buffer [10, 20, 30, -60], sum = 0.0, avg = 0.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, out);
}

/**
 * @brief Test reset state and defensive NULL/uninitialized guards.
 */
static void test_moving_avg_reset_and_null_checks(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      moving_avg_init(&s_filter, s_filter_storage, 4, false, 0.0f));

    float out = 0.0f;
    (void)moving_avg_update(&s_filter, 15.0f, &out);
    (void)moving_avg_update(&s_filter, 25.0f, &out);
    TEST_ASSERT_EQUAL_UINT16(2, moving_avg_get_count(&s_filter));

    moving_avg_reset(&s_filter);
    TEST_ASSERT_EQUAL_UINT16(0, moving_avg_get_count(&s_filter));
    TEST_ASSERT_FALSE(moving_avg_is_primed(&s_filter));

    /* Empty filter get_average returns STATUS_ERR_BUSY */
    float avg = 0.0f;
    TEST_ASSERT_EQUAL(STATUS_ERR_BUSY, moving_avg_get_average(&s_filter, &avg));

    /* NULL Pointer checks */
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, moving_avg_update(NULL, 10.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, moving_avg_update(&s_filter, 10.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, moving_avg_get_average(NULL, &avg));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, moving_avg_get_average(&s_filter, NULL));
    TEST_ASSERT_FALSE(moving_avg_is_primed(NULL));
    TEST_ASSERT_EQUAL_UINT16(0, moving_avg_get_count(NULL));

    /* NULL reset call should be safe */
    moving_avg_reset(NULL);

    /* Uninitialized filter checks */
    moving_avg_filter_t uninit_filter = { 0 };
    TEST_ASSERT_EQUAL(STATUS_ERR_NOT_INITIALIZED, moving_avg_update(&uninit_filter, 10.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_NOT_INITIALIZED, moving_avg_get_average(&uninit_filter, &avg));
    TEST_ASSERT_FALSE(moving_avg_is_primed(&uninit_filter));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_moving_avg_init_validation);
    RUN_TEST(test_moving_avg_sliding_window_sizes);
    RUN_TEST(test_moving_avg_constant_time_stability);
    RUN_TEST(test_moving_avg_outlier_clamping);
    RUN_TEST(test_moving_avg_priming_indicator);
    RUN_TEST(test_moving_avg_negative_and_zero_values);
    RUN_TEST(test_moving_avg_reset_and_null_checks);
    return UNITY_END();
}
