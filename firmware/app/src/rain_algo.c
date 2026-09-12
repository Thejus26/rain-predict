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

static const char * const s_alert_state_names[4] = {
    "Rain Unlikely (Green Alert)",
    "Rain Possible (Yellow Alert)",
    "Rain Likely (Orange Alert)",
    "Rain Imminent (Red Alert)"
};

status_t rain_algo_classify_state(float cpi_pct,
                                  float delta_p_1h,
                                  float rh_pct,
                                  float dpd_c,
                                  float drop_solar_pct,
                                  float lux_curr,
                                  rain_alert_state_t *p_state)
{
    if (p_state == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (isnan(cpi_pct) || isnan(delta_p_1h) || isnan(rh_pct) ||
        isnan(dpd_c) || isnan(drop_solar_pct) || isnan(lux_curr)) {
        return STATUS_ERR_INVALID_ARG;
    }

    /* 1. Evaluate Critical Safety Overrides first */
    if ((delta_p_1h <= CRITICAL_DROP_1H_HPA) && (rh_pct >= CRITICAL_DROP_RH_MIN_PCT)) {
        *p_state = RAIN_ALERT_IMMINENT;
        return STATUS_OK;
    }

    if ((drop_solar_pct >= CRITICAL_SOLAR_DROP_PCT) &&
        (lux_curr < CRITICAL_SOLAR_LUX_MAX) &&
        (rh_pct >= 85.0f)) {
        *p_state = RAIN_ALERT_IMMINENT;
        return STATUS_OK;
    }

    if ((dpd_c <= CRITICAL_SAT_DPD_MAX_C) &&
        (rh_pct >= CRITICAL_SAT_RH_MIN_PCT) &&
        (delta_p_1h <= -0.50f)) {
        *p_state = RAIN_ALERT_IMMINENT;
        return STATUS_OK;
    }

    /* 2. Standard CPI Tier Classification */
    if (cpi_pct < CPI_THRESH_POSSIBLE_PCT) {
        *p_state = RAIN_ALERT_UNLIKELY;
    } else if (cpi_pct < CPI_THRESH_LIKELY_PCT) {
        *p_state = RAIN_ALERT_POSSIBLE;
    } else if (cpi_pct < CPI_THRESH_IMMINENT_PCT) {
        *p_state = RAIN_ALERT_LIKELY;
    } else {
        *p_state = RAIN_ALERT_IMMINENT;
    }

    return STATUS_OK;
}

status_t rain_algo_evaluate(const env_sample_t *p_samples,
                            uint32_t sample_count,
                            float altitude_m,
                            uint8_t month_1_to_12,
                            wind_dir_t wind_dir,
                            float wind_speed_mps,
                            rain_forecast_t *p_forecast)
{
    if ((p_samples == NULL) || (p_forecast == NULL)) {
        return STATUS_ERR_NULL_PTR;
    }
    if (sample_count == 0U) {
        return STATUS_ERR_INVALID_ARG;
    }

    const env_sample_t *p_curr = &p_samples[sample_count - 1U];

    /* 1. Sea-level pressure reduction */
    float p0_curr = 0.0f;
    status_t status = dew_point_calc_sea_level_pressure(p_curr->p0_hpa,
                                                        p_curr->temp_c,
                                                        altitude_m,
                                                        &p0_curr);
    if (status != STATUS_OK) {
        p0_curr = p_curr->p0_hpa;
    }

    /* 2. Psychrometric calculations */
    psychrometric_state_t psychro;
    status = dew_point_calc_psychrometric_state(p_curr->temp_c,
                                                p_curr->rh_pct,
                                                &psychro);
    if (status != STATUS_OK) {
        return status;
    }

    /* 3. Multi-variable gradients */
    multi_gradient_t grads;
    if (trend_detector_compute_gradients(p_samples, sample_count, &grads) != 0) {
        return STATUS_ERR_INVALID_ARG;
    }

    /* 4. Sub-scores: Pressure */
    (void)trend_detector_classify_pressure(grads.delta_p_1h_hpa,
                                           grads.delta_p_3h_hpa,
                                           &p_forecast->pressure_state);
    (void)trend_detector_score_pressure(grads.delta_p_1h_hpa,
                                        grads.delta_p_3h_hpa,
                                        &p_forecast->score_pressure);

    /* 5. Sub-scores: Solar */
    (void)trend_detector_classify_solar(p_curr->lux,
                                        p_curr->lux - grads.delta_lux_30m,
                                        grads.solar_drop_pct_30m,
                                        &p_forecast->solar_state);
    (void)trend_detector_score_solar(p_curr->lux,
                                     p_curr->lux - grads.delta_lux_30m,
                                     grads.solar_drop_pct_30m,
                                     &p_forecast->score_solar);

    /* 6. Sub-scores: Humidity & Dew Point */
    (void)rain_algo_score_humidity(p_curr->rh_pct,
                                   grads.delta_rh_1h_pct,
                                   &p_forecast->score_humidity);
    (void)rain_algo_score_dew_point(psychro.dew_point_dep_c,
                                    &p_forecast->score_dew_point);

    /* 7. Sub-scores: Zambretti */
    rain_forecast_state_t z_state;
    (void)zambretti_calculate_weighted(p0_curr,
                                       grads.delta_p_3h_hpa,
                                       month_1_to_12,
                                       wind_dir,
                                       wind_speed_mps,
                                       &p_forecast->z_index,
                                       &z_state);
    (void)rain_algo_score_zambretti(p_forecast->z_index,
                                    &p_forecast->score_zambretti);

    /* 8. Composite Precipitation Index (CPI) */
    bool is_daylight = (p_curr->lux >= SOLAR_DAYLIGHT_MIN_LUX);
    (void)rain_algo_compute_composite_score(p_forecast->score_pressure,
                                            p_forecast->score_humidity,
                                            p_forecast->score_dew_point,
                                            p_forecast->score_solar,
                                            p_forecast->score_zambretti,
                                            is_daylight,
                                            &p_forecast->cpi_score_pct);

    /* 9. Operational Alert State Classification */
    rain_alert_state_t alert_state;
    (void)rain_algo_classify_state(p_forecast->cpi_score_pct,
                                   grads.delta_p_1h_hpa,
                                   p_curr->rh_pct,
                                   psychro.dew_point_dep_c,
                                   grads.solar_drop_pct_30m,
                                   p_curr->lux,
                                   &alert_state);

    p_forecast->forecast_state = (rain_forecast_state_t)alert_state;
    return STATUS_OK;
}

const char *rain_algo_get_alert_state_name(rain_alert_state_t state)
{
    if ((uint32_t)state > (uint32_t)RAIN_ALERT_IMMINENT) {
        return "Unknown Alert State";
    }
    return s_alert_state_names[(uint32_t)state];
}
