/**
 * @file    test_dew_point.c
 * @brief   Comprehensive unit tests for psychrometric and vapor pressure algorithms.
 * @details Validates Magnus-Tetens saturation vapor pressure (es), actual vapor pressure (e),
 *          absolute humidity (AH), vapor pressure deficit (VPD), input clamping, NULL pointers,
 *          and NaN resilience against standard NOAA / Smithsonian reference vectors.
 */

#include "unity.h"
#include "dew_point.h"
#include <math.h>

void setUp(void) {
}

void tearDown(void) {
}

/**
 * @brief Test saturation vapor pressure es(T) against NOAA reference vectors.
 */
static void test_dew_point_saturation_vp_reference_matrix(void) {
    float es = 0.0f;

    /* TC-VP-01: Triple point freezing (0.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(0.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.112f, es);

    /* TC-VP-02: Cool plantation morning (10.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(10.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 12.279f, es);

    /* TC-VP-03: Standard ambient (20.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(20.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 23.370f, es);

    /* TC-VP-04: Warm pre-monsoon peak (25.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(25.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 31.671f, es);

    /* TC-VP-05: Hot saturated squall (35.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(35.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 56.236f, es);

    /* TC-VP-06: Winter ground frost (-5.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(-5.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.215f, es);

    /* TC-VP-07: Arid boundary (30.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(30.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 42.430f, es);

    /* TC-VP-08: Sub-zero clamping (-60.0°C clamped to -40.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(-60.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.189f, es);

    /* TC-VP-09: Over-temperature clamping (+95.0°C clamped to +85.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(95.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 578.03f, es);
}

/**
 * @brief Test actual partial vapor pressure e(T, RH) against reference vectors.
 */
static void test_dew_point_actual_vp_reference_matrix(void) {
    float e = 0.0f;

    /* TC-VP-01: 0.0°C, 100.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(0.0f, 100.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.112f, e);

    /* TC-VP-02: 10.0°C, 80.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(10.0f, 80.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 9.823f, e);

    /* TC-VP-03: 20.0°C, 50.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(20.0f, 50.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 11.685f, e);

    /* TC-VP-04: 25.0°C, 85.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(25.0f, 85.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 26.920f, e);

    /* TC-VP-05: 35.0°C, 95.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(35.0f, 95.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 53.424f, e);

    /* TC-VP-06: -5.0°C, 75.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(-5.0f, 75.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.161f, e);

    /* TC-VP-07: 30.0°C, 0.1% RH (clamped) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(30.0f, 0.1f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.042f, e);
}

/**
 * @brief Test absolute humidity AH(T, e) against reference vectors.
 */
static void test_dew_point_abs_humidity_reference_matrix(void) {
    float ah = 0.0f;

    /* TC-VP-01: 0.0°C, 100.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(0.0f, 100.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.848f, ah);

    /* TC-VP-02: 10.0°C, 80.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(10.0f, 80.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 7.518f, ah);

    /* TC-VP-03: 20.0°C, 50.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(20.0f, 50.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 8.638f, ah);

    /* TC-VP-04: 25.0°C, 85.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(25.0f, 85.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 19.567f, ah);

    /* TC-VP-05: 35.0°C, 95.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(35.0f, 95.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 37.556f, ah);

    /* TC-VP-06: -5.0°C, 75.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(-5.0f, 75.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.554f, ah);

    /* TC-VP-07: 30.0°C, 0.1% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(30.0f, 0.1f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.030f, ah);
}

/**
 * @brief Test vapor pressure deficit VPD(T, RH) against reference vectors.
 */
static void test_dew_point_vpd_reference_matrix(void) {
    float vpd = 0.0f;

    /* TC-VP-01: 0.0°C, 100.0% RH -> 0.0 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(0.0f, 100.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.000f, vpd);

    /* TC-VP-02: 10.0°C, 80.0% RH -> 2.456 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(10.0f, 80.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 2.456f, vpd);

    /* TC-VP-03: 20.0°C, 50.0% RH -> 11.685 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(20.0f, 50.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 11.685f, vpd);

    /* TC-VP-04: 25.0°C, 85.0% RH -> 4.751 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(25.0f, 85.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 4.751f, vpd);

    /* TC-VP-05: 35.0°C, 95.0% RH -> 2.812 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(35.0f, 95.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.812f, vpd);

    /* TC-VP-06: -5.0°C, 75.0% RH -> 1.054 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(-5.0f, 75.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.054f, vpd);

    /* TC-VP-07: 30.0°C, 0.1% RH -> 42.388 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(30.0f, 0.1f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 42.388f, vpd);
}

/**
 * @brief Test defensive null pointer and NaN input handling.
 */
static void test_dew_point_null_pointer_and_nan_safety(void) {
    float out = 0.0f;
    float nan_val = 0.0f / 0.0f;

    /* TC-VP-10: NULL pointer returns STATUS_ERR_NULL_PTR */
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_saturation_vp(20.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_actual_vp(20.0f, 50.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_abs_humidity(20.0f, 50.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_vpd(20.0f, 50.0f, NULL));

    /* TC-VP-11: NaN inputs return STATUS_ERR_INVALID_PARAM */
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_saturation_vp(nan_val, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_actual_vp(nan_val, 50.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_actual_vp(20.0f, nan_val, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_abs_humidity(nan_val, 50.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_abs_humidity(20.0f, nan_val, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_vpd(nan_val, 50.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_vpd(20.0f, nan_val, &out));
}

/**
 * @brief Test boundary humidity clamping for out-of-range RH values.
 */
static void test_dew_point_humidity_clamping(void) {
    float e_normal = 0.0f;
    float e_clamped_high = 0.0f;
    float e_clamped_low = 0.0f;

    /* RH > 100.0% clamped to 100.0% */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(20.0f, 100.0f, &e_normal));
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(20.0f, 150.0f, &e_clamped_high));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, e_normal, e_clamped_high);

    /* RH < 0.1% clamped to 0.1% */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(20.0f, 0.1f, &e_normal));
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(20.0f, -10.0f, &e_clamped_low));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, e_normal, e_clamped_low);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_dew_point_saturation_vp_reference_matrix);
    RUN_TEST(test_dew_point_actual_vp_reference_matrix);
    RUN_TEST(test_dew_point_abs_humidity_reference_matrix);
    RUN_TEST(test_dew_point_vpd_reference_matrix);
    RUN_TEST(test_dew_point_null_pointer_and_nan_safety);
    RUN_TEST(test_dew_point_humidity_clamping);

    return UNITY_END();
}
