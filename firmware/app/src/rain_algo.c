/**
 * @file    rain_algo.c
 * @brief   Implementation of composite precipitation nowcasting scoring engine.
 * @details Computes normalized meteorological sub-scores and dynamic weighted CPI.
 * 
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#include "rain_algo.h"
#include <math.h>
#include <stddef.h>

status_t rain_algo_score_humidity(float rh_pct, float delta_rh_1h, uint8_t *p_score)
{
    if (p_score == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(rh_pct) || isnan(delta_rh_1h)) {
        return STATUS_ERR_INVALID_ARG;
    }

    uint8_t score = 0U;

    if ((rh_pct >= 95.0f) || (delta_rh_1h >= 15.0f)) {
        score = 100U;
    } else if ((rh_pct >= 90.0f) || (delta_rh_1h >= 10.0f)) {
        score = 80U;
    } else if ((rh_pct >= 80.0f) || (delta_rh_1h >= 5.0f)) {
        score = 50U;
    } else if (rh_pct >= 65.0f) {
        score = 15U;
    } else {
        score = 0U;
    }

    *p_score = score;
    return STATUS_OK;
}

status_t rain_algo_score_dew_point(float dpd_c, uint8_t *p_score)
{
    if (p_score == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(dpd_c)) {
        return STATUS_ERR_INVALID_ARG;
    }

    uint8_t score = 0U;

    if (dpd_c <= 0.50f) {
        score = 100U;
    } else if (dpd_c <= 1.50f) {
        score = 80U;
    } else if (dpd_c <= 3.00f) {
        score = 50U;
    } else if (dpd_c <= 5.00f) {
        score = 15U;
    } else {
        score = 0U;
    }

    *p_score = score;
    return STATUS_OK;
}

status_t rain_algo_score_zambretti(uint8_t z_index, uint8_t *p_score)
{
    if (p_score == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if ((z_index < ZAMBRETTI_INDEX_MIN) || (z_index > ZAMBRETTI_INDEX_MAX)) {
        return STATUS_ERR_INVALID_ARG;
    }

    int32_t raw_score = (int32_t)(z_index - 1U) * 4;
    if (raw_score > 100) {
        raw_score = 100;
    } else if (raw_score < 0) {
        raw_score = 0;
    }

    *p_score = (uint8_t)raw_score;
    return STATUS_OK;
}

status_t rain_algo_compute_composite_score(uint8_t s_p,
                                           uint8_t s_rh,
                                           uint8_t s_dpd,
                                           uint8_t s_sol,
                                           uint8_t s_zam,
                                           bool is_daylight,
                                           float *p_cpi_pct)
{
    if (p_cpi_pct == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    float cpi = 0.0f;

    if (is_daylight) {
        /* Daytime 5-variable weighting: 30% Sp + 25% Srh + 20% Sdpd + 15% Ssol + 10% Szam */
        cpi = (WEIGHT_PRESSURE_DAY * (float)s_p) +
              (WEIGHT_HUMIDITY_DAY * (float)s_rh) +
              (WEIGHT_DEW_POINT_DAY * (float)s_dpd) +
              (WEIGHT_SOLAR_DAY * (float)s_sol) +
              (WEIGHT_ZAMBRETTI_DAY * (float)s_zam);
    } else {
        /* Nighttime 4-variable re-normalized weighting (divided by 0.85) */
        float raw_sum = (WEIGHT_PRESSURE_DAY * (float)s_p) +
                        (WEIGHT_HUMIDITY_DAY * (float)s_rh) +
                        (WEIGHT_DEW_POINT_DAY * (float)s_dpd) +
                        (WEIGHT_ZAMBRETTI_DAY * (float)s_zam);
        cpi = raw_sum / WEIGHT_NIGHT_DIVISOR;
    }

    if (cpi < 0.0f) {
        cpi = 0.0f;
    } else if (cpi > 100.0f) {
        cpi = 100.0f;
    }

    *p_cpi_pct = cpi;
    return STATUS_OK;
}
