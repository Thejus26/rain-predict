/**
 * @file dew_point.c
 * @brief Implementation of thermodynamic vapor pressure and psychrometric formulas.
 */

#include "dew_point.h"
#include <math.h>
#include <stddef.h>

/**
 * @brief Internal helper to clamp temperature to safe physical operating bounds.
 */
static inline float clamp_temperature(float temp_c)
{
    if (temp_c < PSYCHRO_TEMP_MIN_C) {
        return PSYCHRO_TEMP_MIN_C;
    }
    if (temp_c > PSYCHRO_TEMP_MAX_C) {
        return PSYCHRO_TEMP_MAX_C;
    }
    return temp_c;
}

/**
 * @brief Internal helper to clamp relative humidity to valid physical domain.
 */
static inline float clamp_humidity(float rh_pct)
{
    if (rh_pct < PSYCHRO_RH_MIN_PCT) {
        return PSYCHRO_RH_MIN_PCT;
    }
    if (rh_pct > PSYCHRO_RH_MAX_PCT) {
        return PSYCHRO_RH_MAX_PCT;
    }
    return rh_pct;
}

status_t dew_point_calc_saturation_vp(float temp_c, float *p_es_hpa)
{
    if (p_es_hpa == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    float t = clamp_temperature(temp_c);

    /* es(T) = 6.112 * exp((17.67 * T) / (T + 243.5)) */
    float exponent = (MAGNUS_COEFF_A * t) / (t + MAGNUS_COEFF_B);
    *p_es_hpa = MAGNUS_COEFF_C * expf(exponent);

    return STATUS_OK;
}

status_t dew_point_calc_actual_vp(float temp_c, float rh_pct, float *p_e_hpa)
{
    if (p_e_hpa == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c) || isnan(rh_pct)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    float es = 0.0f;
    status_t status = dew_point_calc_saturation_vp(temp_c, &es);
    if (status != STATUS_OK) {
        return status;
    }

    float rh = clamp_humidity(rh_pct);
    *p_e_hpa = es * (rh / 100.0f);

    return STATUS_OK;
}

status_t dew_point_calc_abs_humidity(float temp_c, float rh_pct, float *p_ah_gm3)
{
    if (p_ah_gm3 == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c) || isnan(rh_pct)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    float e = 0.0f;
    status_t status = dew_point_calc_actual_vp(temp_c, rh_pct, &e);
    if (status != STATUS_OK) {
        return status;
    }

    float t = clamp_temperature(temp_c);
    float t_kelvin = t + KELVIN_OFFSET;

    /* AH = (216.7 * e) / (T + 273.15) */
    *p_ah_gm3 = (ABS_HUMIDITY_COEFF * e) / t_kelvin;

    return STATUS_OK;
}

status_t dew_point_calc_vpd(float temp_c, float rh_pct, float *p_vpd_hpa)
{
    if (p_vpd_hpa == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c) || isnan(rh_pct)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    float es = 0.0f;
    status_t status = dew_point_calc_saturation_vp(temp_c, &es);
    if (status != STATUS_OK) {
        return status;
    }

    float rh = clamp_humidity(rh_pct);
    *p_vpd_hpa = es * (1.0f - (rh / 100.0f));

    return STATUS_OK;
}

#define DENOMINATOR_EPSILON         (1e-4f)
#define MIN_VAPOR_PRESSURE_HPA      (0.1f)

status_t dew_point_calc_tdew(float temp_c, float rh_pct, float *p_dew_c)
{
    if (p_dew_c == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c) || isnan(rh_pct)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    float t = clamp_temperature(temp_c);
    float rh = clamp_humidity(rh_pct);

    /* gamma = ln(RH / 100) + (17.67 * T) / (T + 243.5) */
    float gamma = logf(rh / 100.0f) + ((MAGNUS_COEFF_A * t) / (t + MAGNUS_COEFF_B));

    /* Denominator = 17.67 - gamma */
    float denom = MAGNUS_COEFF_A - gamma;
    if (fabsf(denom) < DENOMINATOR_EPSILON) {
        denom = (denom >= 0.0f) ? DENOMINATOR_EPSILON : -DENOMINATOR_EPSILON;
    }

    /* Tdew = (243.5 * gamma) / (17.67 - gamma) */
    float tdew = (MAGNUS_COEFF_B * gamma) / denom;

    /* Physical constraint: Tdew cannot exceed ambient temperature */
    if (tdew > t) {
        tdew = t;
    }

    *p_dew_c = tdew;
    return STATUS_OK;
}

status_t dew_point_calc_depression(float temp_c, float rh_pct, float *p_dpd_c)
{
    if (p_dpd_c == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c) || isnan(rh_pct)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    float tdew = 0.0f;
    status_t status = dew_point_calc_tdew(temp_c, rh_pct, &tdew);
    if (status != STATUS_OK) {
        return status;
    }

    float t = clamp_temperature(temp_c);
    float dpd = t - tdew;
    if (dpd < 0.0f) {
        dpd = 0.0f;
    }

    *p_dpd_c = dpd;
    return STATUS_OK;
}

status_t dew_point_calc_from_vapor_pressure(float e_hpa, float *p_dew_c)
{
    if (p_dew_c == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(e_hpa) || e_hpa <= 0.0f) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* Clamp vapor pressure to physical limits */
    float e = (e_hpa < MIN_VAPOR_PRESSURE_HPA) ? MIN_VAPOR_PRESSURE_HPA : e_hpa;

    /* ln(e / 6.112) */
    float ln_val = logf(e / MAGNUS_COEFF_C);
    float denom = MAGNUS_COEFF_A - ln_val;
    if (fabsf(denom) < DENOMINATOR_EPSILON) {
        denom = (denom >= 0.0f) ? DENOMINATOR_EPSILON : -DENOMINATOR_EPSILON;
    }

    *p_dew_c = (MAGNUS_COEFF_B * ln_val) / denom;
    return STATUS_OK;
}

status_t dew_point_calc_psychrometric_state(float temp_c, float rh_pct, psychrometric_state_t *p_state)
{
    if (p_state == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(temp_c) || isnan(rh_pct)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    p_state->temp_c = clamp_temperature(temp_c);
    p_state->rh_pct = clamp_humidity(rh_pct);

    status_t status = dew_point_calc_saturation_vp(p_state->temp_c, &p_state->saturation_vp_hpa);
    if (status != STATUS_OK) {
        return status;
    }

    p_state->actual_vp_hpa = p_state->saturation_vp_hpa * (p_state->rh_pct / 100.0f);
    p_state->vpd_hpa = p_state->saturation_vp_hpa - p_state->actual_vp_hpa;

    float t_kelvin = p_state->temp_c + KELVIN_OFFSET;
    p_state->abs_humidity_gm3 = (ABS_HUMIDITY_COEFF * p_state->actual_vp_hpa) / t_kelvin;

    status = dew_point_calc_tdew(p_state->temp_c, p_state->rh_pct, &p_state->dew_point_c);
    if (status != STATUS_OK) {
        return status;
    }

    p_state->dew_point_dep_c = p_state->temp_c - p_state->dew_point_c;
    if (p_state->dew_point_dep_c < 0.0f) {
        p_state->dew_point_dep_c = 0.0f;
    }

    return STATUS_OK;
}

status_t dew_point_calc_sea_level_pressure(float station_p_hpa, float temp_c, float altitude_m, float *p_p0_hpa)
{
    if (p_p0_hpa == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(station_p_hpa) || isnan(temp_c) || isnan(altitude_m)) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (station_p_hpa < HYPSO_PRESSURE_MIN_HPA || station_p_hpa > HYPSO_PRESSURE_MAX_HPA) {
        return STATUS_ERR_OUT_OF_RANGE;
    }

    /* If station is at or below sea level, no reduction needed */
    if (altitude_m <= 0.0f) {
        *p_p0_hpa = station_p_hpa;
        return STATUS_OK;
    }

    /* Clamp altitude to valid operational domain */
    float alt = altitude_m;
    if (alt > HYPSO_ALTITUDE_MAX_M) {
        alt = HYPSO_ALTITUDE_MAX_M;
    }

    float t = clamp_temperature(temp_c);
    float lapse_h = HYPSO_LAPSE_RATE * alt;
    float t_sea_kelvin = t + lapse_h + KELVIN_OFFSET;

    if (t_sea_kelvin < HYPSO_DENOM_MIN_KELVIN) {
        t_sea_kelvin = HYPSO_DENOM_MIN_KELVIN;
    }

    float base = 1.0f - (lapse_h / t_sea_kelvin);
    *p_p0_hpa = station_p_hpa * powf(base, HYPSO_EXPONENT);

    return STATUS_OK;
}

status_t dew_point_calc_station_pressure_from_p0(float p0_hpa, float temp_c, float altitude_m, float *p_station_p)
{
    if (p_station_p == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(p0_hpa) || isnan(temp_c) || isnan(altitude_m)) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p0_hpa < HYPSO_PRESSURE_MIN_HPA || p0_hpa > HYPSO_PRESSURE_MAX_HPA) {
        return STATUS_ERR_OUT_OF_RANGE;
    }

    if (altitude_m <= 0.0f) {
        *p_station_p = p0_hpa;
        return STATUS_OK;
    }

    float alt = altitude_m;
    if (alt > HYPSO_ALTITUDE_MAX_M) {
        alt = HYPSO_ALTITUDE_MAX_M;
    }

    float t = clamp_temperature(temp_c);
    float lapse_h = HYPSO_LAPSE_RATE * alt;
    float t_sea_kelvin = t + lapse_h + KELVIN_OFFSET;

    if (t_sea_kelvin < HYPSO_DENOM_MIN_KELVIN) {
        t_sea_kelvin = HYPSO_DENOM_MIN_KELVIN;
    }

    float base = 1.0f - (lapse_h / t_sea_kelvin);
    /* Inversion: P = P0 * powf(base, +5.257) */
    *p_station_p = p0_hpa * powf(base, -HYPSO_EXPONENT);

    return STATUS_OK;
}

status_t dew_point_calc_pressure_altitude(float station_p_hpa, float p0_hpa, float temp_c, float *p_altitude_m)
{
    if (p_altitude_m == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(station_p_hpa) || isnan(p0_hpa) || isnan(temp_c)) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (station_p_hpa <= 0.0f || p0_hpa <= 0.0f) {
        return STATUS_ERR_OUT_OF_RANGE;
    }

    float t = clamp_temperature(temp_c);
    float t_kelvin = t + KELVIN_OFFSET;
    float p_ratio = p0_hpa / station_p_hpa;

    /* h = (T_kelvin / 0.0065) * (powf(P0 / P, 1/5.257) - 1.0) */
    *p_altitude_m = (t_kelvin / HYPSO_LAPSE_RATE) * (powf(p_ratio, HYPSO_EXPONENT_INV) - 1.0f);

    return STATUS_OK;
}
