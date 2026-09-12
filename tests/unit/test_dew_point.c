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
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 12.272f, es);

    /* TC-VP-03: Standard ambient (20.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(20.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 23.370f, es);

    /* TC-VP-04: Warm pre-monsoon peak (25.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(25.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 31.674f, es);

    /* TC-VP-05: Hot saturated squall (35.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(35.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 56.312f, es);

    /* TC-VP-06: Winter ground frost (-5.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(-5.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.220f, es);

    /* TC-VP-07: Arid boundary (30.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(30.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 42.456f, es);

    /* TC-VP-08: Sub-zero clamping (-60.0°C clamped to -40.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(-60.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.190f, es);

    /* TC-VP-09: Over-temperature clamping (+95.0°C clamped to +85.0°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_saturation_vp(95.0f, &es));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 591.35f, es);
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
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 9.817f, e);

    /* TC-VP-03: 20.0°C, 50.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(20.0f, 50.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 11.685f, e);

    /* TC-VP-04: 25.0°C, 85.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(25.0f, 85.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 26.923f, e);

    /* TC-VP-05: 35.0°C, 95.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(35.0f, 95.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 53.496f, e);

    /* TC-VP-06: -5.0°C, 75.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(-5.0f, 75.0f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.165f, e);

    /* TC-VP-07: 30.0°C, 0.1% RH (clamped) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_actual_vp(30.0f, 0.1f, &e));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.043f, e);
}

/**
 * @brief Test absolute humidity AH(T, e) against reference vectors.
 */
static void test_dew_point_abs_humidity_reference_matrix(void) {
    float ah = 0.0f;

    /* TC-VP-01: 0.0°C, 100.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(0.0f, 100.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.849f, ah);

    /* TC-VP-02: 10.0°C, 80.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(10.0f, 80.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 7.513f, ah);

    /* TC-VP-03: 20.0°C, 50.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(20.0f, 50.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 8.638f, ah);

    /* TC-VP-04: 25.0°C, 85.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(25.0f, 85.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 19.568f, ah);

    /* TC-VP-05: 35.0°C, 95.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(35.0f, 95.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 37.620f, ah);

    /* TC-VP-06: -5.0°C, 75.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_abs_humidity(-5.0f, 75.0f, &ah));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.558f, ah);

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

    /* TC-VP-02: 10.0°C, 80.0% RH -> 2.454 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(10.0f, 80.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 2.454f, vpd);

    /* TC-VP-03: 20.0°C, 50.0% RH -> 11.685 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(20.0f, 50.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 11.685f, vpd);

    /* TC-VP-04: 25.0°C, 85.0% RH -> 4.751 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(25.0f, 85.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 4.751f, vpd);

    /* TC-VP-05: 35.0°C, 95.0% RH -> 2.816 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(35.0f, 95.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.816f, vpd);

    /* TC-VP-06: -5.0°C, 75.0% RH -> 1.055 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(-5.0f, 75.0f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.055f, vpd);

    /* TC-VP-07: 30.0°C, 0.1% RH -> 42.413 hPa deficit */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_vpd(30.0f, 0.1f, &vpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 42.413f, vpd);
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

/**
 * @brief Test dew point temperature Tdew(T, RH) against NOAA reference vectors.
 */
static void test_dew_point_calc_tdew_reference_matrix(void) {
    float tdew = 0.0f;

    /* TC-DP-01: 100% Saturated Freezing (0.0°C, 100.0% RH) -> Tdew = 0.00°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(0.00f, 100.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.00f, tdew);

    /* TC-DP-02: Cool Plantation Dawn (12.0°C, 85.0% RH) -> Tdew = 9.56°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(12.00f, 85.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 9.56f, tdew);

    /* TC-DP-03: Mild Afternoon (20.0°C, 50.0% RH) -> Tdew = 9.27°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(20.00f, 50.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 9.27f, tdew);

    /* TC-DP-04: Warm Pre-Monsoon (25.0°C, 80.0% RH) -> Tdew = 21.30°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(25.00f, 80.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.30f, tdew);

    /* TC-DP-05: Saturated Pre-Storm (26.0°C, 95.0% RH) -> Tdew = 25.15°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(26.00f, 95.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 25.15f, tdew);

    /* TC-DP-06: Hot Tropical Squall (32.0°C, 90.0% RH) -> Tdew = 30.17°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(32.00f, 90.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 30.17f, tdew);

    /* TC-DP-07: High-Altitude Frost (-5.0°C, 70.0% RH) -> Tdew = -9.63°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(-5.00f, 70.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -9.63f, tdew);

    /* TC-DP-08: Extremely Dry Parcel (30.0°C, 5.0% RH) -> Tdew = -13.75°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(30.00f, 5.0f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -13.75f, tdew);
}

/**
 * @brief Test dew point depression DPD(T, RH) against reference vectors.
 */
static void test_dew_point_calc_depression_reference_matrix(void) {
    float dpd = 0.0f;

    /* TC-DP-01: 100% Saturated Freezing (0.0°C, 100.0% RH) -> DPD = 0.00°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(0.00f, 100.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.00f, dpd);

    /* TC-DP-02: Cool Plantation Dawn (12.0°C, 85.0% RH) -> DPD = 2.44°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(12.00f, 85.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.44f, dpd);

    /* TC-DP-03: Mild Afternoon (20.0°C, 50.0% RH) -> DPD = 10.73°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(20.00f, 50.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 10.73f, dpd);

    /* TC-DP-04: Warm Pre-Monsoon (25.0°C, 80.0% RH) -> DPD = 3.70°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(25.00f, 80.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.70f, dpd);

    /* TC-DP-05: Saturated Pre-Storm (26.0°C, 95.0% RH) -> DPD = 0.85°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(26.00f, 95.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.85f, dpd);

    /* TC-DP-06: Hot Tropical Squall (32.0°C, 90.0% RH) -> DPD = 1.83°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(32.00f, 90.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.83f, dpd);

    /* TC-DP-07: High-Altitude Frost (-5.0°C, 70.0% RH) -> DPD = 4.63°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(-5.00f, 70.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 4.63f, dpd);

    /* TC-DP-08: Extremely Dry Parcel (30.0°C, 5.0% RH) -> DPD = 43.75°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(30.00f, 5.0f, &dpd));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 43.75f, dpd);
}

/**
 * @brief Test dew point calculated directly from vapor pressure.
 */
static void test_dew_point_calc_from_vapor_pressure_reference(void) {
    float tdew = 0.0f;

    /* TC-DP-09: Direct from Vapor Press (e = 23.37 hPa -> Tdew = 20.00°C) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_from_vapor_pressure(23.37f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.00f, tdew);

    /* Freezing vapor pressure: 6.112 hPa -> 0.00°C */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_from_vapor_pressure(6.112f, &tdew));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.00f, tdew);

    /* Low vapor pressure clamp guard */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_from_vapor_pressure(0.01f, &tdew));
    TEST_ASSERT_TRUE(tdew < -30.0f);
}

/**
 * @brief Test full psychrometric state struct population.
 */
static void test_dew_point_psychrometric_state_full(void) {
    psychrometric_state_t state;

    /* 25.0°C, 80.0% RH */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_psychrometric_state(25.0f, 80.0f, &state));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.00f, state.temp_c);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.00f, state.rh_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 31.67f, state.saturation_vp_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 25.34f, state.actual_vp_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 6.33f, state.vpd_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.42f, state.abs_humidity_gm3);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.30f, state.dew_point_c);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.70f, state.dew_point_dep_c);
}

/**
 * @brief Test physical invariants: Tdew <= T, DPD >= 0.0 across edge cases.
 */
static void test_dew_point_physical_invariants(void) {
    float tdew = 0.0f;
    float dpd = 0.0f;

    /* 100% RH should have Tdew == T and DPD == 0.0 */
    float test_temps[] = {-40.0f, -10.0f, 0.0f, 15.0f, 25.0f, 40.0f, 85.0f};
    for (size_t i = 0; i < sizeof(test_temps) / sizeof(test_temps[0]); i++) {
        float t = test_temps[i];
        TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(t, 100.0f, &tdew));
        TEST_ASSERT_TRUE(tdew <= t + 1e-5f);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, t, tdew);

        TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(t, 100.0f, &dpd));
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dpd);
        TEST_ASSERT_TRUE(dpd >= 0.0f);
    }

    /* Out of bounds RH > 100% clamped */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_tdew(20.0f, 110.0f, &tdew));
    TEST_ASSERT_TRUE(tdew <= 20.0f);
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_depression(20.0f, 110.0f, &dpd));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dpd);
}

/**
 * @brief Test NULL pointer and NaN safety for new functions.
 */
static void test_dew_point_tdew_depression_null_and_nan_safety(void) {
    float out = 0.0f;
    psychrometric_state_t state;
    float nan_val = 0.0f / 0.0f;

    /* TC-DP-10: NULL pointer returns STATUS_ERR_NULL_PTR */
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_tdew(20.0f, 50.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_depression(20.0f, 50.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_from_vapor_pressure(20.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_psychrometric_state(20.0f, 50.0f, NULL));

    /* TC-DP-11: NaN inputs return STATUS_ERR_INVALID_PARAM */
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_tdew(nan_val, 50.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_tdew(20.0f, nan_val, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_depression(nan_val, 50.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_depression(20.0f, nan_val, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_from_vapor_pressure(nan_val, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_from_vapor_pressure(-5.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_psychrometric_state(nan_val, 50.0f, &state));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_psychrometric_state(20.0f, nan_val, &state));
}

/**
 * @brief Test hypsometric sea level reduction P0(P, T, h) against ICAO / WMO reference vectors.
 */
static void test_dew_point_sea_level_pressure_reference_matrix(void) {
    float p0 = 0.0f;

    /* TC-HYP-01: Sea-Level Baseline (P=1013.25 hPa, T=15.0°C, h=0.0m) -> P0 = 1013.25 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(1013.25f, 15.00f, 0.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, p0);

    /* TC-HYP-02: Munnar Valley Station (P=898.70 hPa, T=22.0°C, h=1000.0m) -> P0 = 1007.74 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(898.70f, 22.00f, 1000.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1007.74f, p0);

    /* TC-HYP-03: Coonoor Mid-Slope (P=845.20 hPa, T=18.5°C, h=1500.0m) -> P0 = 1004.70 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(845.20f, 18.50f, 1500.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1004.70f, p0);

    /* TC-HYP-04: Ooty Ridge Peak (P=785.40 hPa, T=14.0°C, h=2000.0m) -> P0 = 991.24 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(785.40f, 14.00f, 2000.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 991.24f, p0);

    /* TC-HYP-05: Kolukkumalai High Mast (P=767.10 hPa, T=12.0°C, h=2160.0m) -> P0 = 987.61 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(767.10f, 12.00f, 2160.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 987.61f, p0);

    /* TC-HYP-06: Monsoon Storm Drop (P=832.10 hPa, T=19.0°C, h=1500.0m) -> P0 = 988.84 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(832.10f, 19.00f, 1500.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 988.84f, p0);

    /* TC-HYP-07: Sub-Zero Winter Frost (P=850.00 hPa, T=-2.0°C, h=1500.0m) -> P0 = 1023.46 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(850.00f, -2.00f, 1500.0f, &p0));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1023.46f, p0);
}

/**
 * @brief Test inverse station pressure calculation and bidirectional loopback.
 */
static void test_dew_point_station_pressure_from_p0_reference(void) {
    float p_station = 0.0f;
    float p0 = 0.0f;

    /* TC-HYP-08: Inverse Pressure Loopback (P0 = 1004.70 hPa, T=18.5°C, h=1500.0m -> Station P = 845.20 hPa) */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_station_pressure_from_p0(1004.70f, 18.50f, 1500.0f, &p_station));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 845.20f, p_station);

    /* Sea level bypass: P0 = 1013.25 hPa, h=0.0m -> Station P = 1013.25 hPa */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_station_pressure_from_p0(1013.25f, 15.00f, 0.0f, &p_station));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, p_station);

    /* Exact roundtrip test across various elevations (using physically realistic station pressures) */
    float test_pressures[] = {1013.25f, 955.0f, 900.0f, 845.0f, 795.0f, 745.0f};
    float test_altitudes[] = {0.0f, 500.0f, 1000.0f, 1500.0f, 2000.0f, 2500.0f};
    float temp = 20.0f;

    for (size_t i = 0; i < sizeof(test_pressures) / sizeof(test_pressures[0]); i++) {
        float p_orig = test_pressures[i];
        float alt = test_altitudes[i];

        TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(p_orig, temp, alt, &p0));
        TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_station_pressure_from_p0(p0, temp, alt, &p_station));
        TEST_ASSERT_FLOAT_WITHIN(0.005f, p_orig, p_station);
    }
}

/**
 * @brief Test pressure altitude estimation from station pressure and P0 reference.
 */
static void test_dew_point_pressure_altitude_reference(void) {
    float alt_m = 0.0f;

    /* Sea level: P = 1013.25 hPa, P0 = 1013.25 hPa, T = 15.0°C -> h = 0.0m */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_pressure_altitude(1013.25f, 1013.25f, 15.0f, &alt_m));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, alt_m);

    /* Mid-elevation: P = 845.20 hPa, P0 = 1004.70 hPa, T = 18.5°C -> h ≈ 1500m */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_pressure_altitude(845.20f, 1004.70f, 18.5f, &alt_m));
    TEST_ASSERT_FLOAT_WITHIN(30.0f, 1500.0f, alt_m);
}

/**
 * @brief Test hypsometric altitude clamping, bypass, and domain bounds.
 */
static void test_dew_point_hypsometric_clamping_and_bypass(void) {
    float p0_neg = 0.0f;
    float p0_zero = 0.0f;
    float p0_clamped = 0.0f;
    float p0_max = 0.0f;

    /* Negative elevation (below sea level) direct bypass */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(950.0f, 20.0f, -50.0f, &p0_neg));
    TEST_ASSERT_EQUAL_FLOAT(950.0f, p0_neg);

    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(950.0f, 20.0f, 0.0f, &p0_zero));
    TEST_ASSERT_EQUAL_FLOAT(950.0f, p0_zero);

    /* Elevation > 5000.0m is clamped to 5000.0m */
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(600.0f, 10.0f, 6000.0f, &p0_clamped));
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_sea_level_pressure(600.0f, 10.0f, 5000.0f, &p0_max));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, p0_max, p0_clamped);

    /* Station pressure from P0 with negative elevation bypass */
    float p_neg = 0.0f;
    TEST_ASSERT_EQUAL(STATUS_OK, dew_point_calc_station_pressure_from_p0(1013.25f, 20.0f, -10.0f, &p_neg));
    TEST_ASSERT_EQUAL_FLOAT(1013.25f, p_neg);
}

/**
 * @brief Test defensive null pointer, NaN inputs, and out-of-range bounds.
 */
static void test_dew_point_hypsometric_null_and_nan_safety(void) {
    float out = 0.0f;
    float nan_val = 0.0f / 0.0f;

    /* TC-HYP-09: Null pointer trap */
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_sea_level_pressure(845.0f, 20.0f, 1500.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_station_pressure_from_p0(1013.25f, 20.0f, 1500.0f, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, dew_point_calc_pressure_altitude(845.0f, 1013.25f, 20.0f, NULL));

    /* TC-HYP-10: Out of bounds pressure (< 300 hPa or > 1100 hPa) */
    TEST_ASSERT_EQUAL(STATUS_ERR_OUT_OF_RANGE, dew_point_calc_sea_level_pressure(150.0f, 20.0f, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_OUT_OF_RANGE, dew_point_calc_sea_level_pressure(1200.0f, 20.0f, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_OUT_OF_RANGE, dew_point_calc_station_pressure_from_p0(200.0f, 20.0f, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_OUT_OF_RANGE, dew_point_calc_station_pressure_from_p0(1200.0f, 20.0f, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_OUT_OF_RANGE, dew_point_calc_pressure_altitude(-10.0f, 1013.25f, 20.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_OUT_OF_RANGE, dew_point_calc_pressure_altitude(845.0f, 0.0f, 20.0f, &out));

    /* NaN validation */
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_sea_level_pressure(nan_val, 20.0f, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_sea_level_pressure(845.0f, nan_val, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_sea_level_pressure(845.0f, 20.0f, nan_val, &out));

    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_station_pressure_from_p0(nan_val, 20.0f, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_station_pressure_from_p0(1013.25f, nan_val, 1500.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_station_pressure_from_p0(1013.25f, 20.0f, nan_val, &out));

    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_pressure_altitude(nan_val, 1013.25f, 20.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_pressure_altitude(845.0f, nan_val, 20.0f, &out));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, dew_point_calc_pressure_altitude(845.0f, 1013.25f, nan_val, &out));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_dew_point_saturation_vp_reference_matrix);
    RUN_TEST(test_dew_point_actual_vp_reference_matrix);
    RUN_TEST(test_dew_point_abs_humidity_reference_matrix);
    RUN_TEST(test_dew_point_vpd_reference_matrix);
    RUN_TEST(test_dew_point_null_pointer_and_nan_safety);
    RUN_TEST(test_dew_point_humidity_clamping);
    RUN_TEST(test_dew_point_calc_tdew_reference_matrix);
    RUN_TEST(test_dew_point_calc_depression_reference_matrix);
    RUN_TEST(test_dew_point_calc_from_vapor_pressure_reference);
    RUN_TEST(test_dew_point_psychrometric_state_full);
    RUN_TEST(test_dew_point_physical_invariants);
    RUN_TEST(test_dew_point_tdew_depression_null_and_nan_safety);
    RUN_TEST(test_dew_point_sea_level_pressure_reference_matrix);
    RUN_TEST(test_dew_point_station_pressure_from_p0_reference);
    RUN_TEST(test_dew_point_pressure_altitude_reference);
    RUN_TEST(test_dew_point_hypsometric_clamping_and_bypass);
    RUN_TEST(test_dew_point_hypsometric_null_and_nan_safety);

    return UNITY_END();
}
