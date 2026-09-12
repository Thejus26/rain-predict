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
