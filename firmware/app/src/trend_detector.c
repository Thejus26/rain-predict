/**
 * @file    trend_detector.c
 * @brief   Implementation of multi-variable gradient differential calculations.
 * @details Computes atmospheric rates of change (dP/dt, dRH/dt, dT/dt, dLux/dt)
 *          and daylight cloud attenuation drop ratio.
 * 
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#include "trend_detector.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

int32_t trend_detector_compute_gradients(const env_sample_t *p_samples,
                                         uint32_t sample_count,
                                         multi_gradient_t *p_gradients)
{
    if (p_samples == NULL || p_gradients == NULL) {
        return ERR_NULL_PTR_CODE;
    }
    if (sample_count == 0U) {
        return ERR_INSUFFICIENT_DATA_CODE;
    }

    /* Initialize output structure with safe defaults */
    memset(p_gradients, 0, sizeof(multi_gradient_t));

    uint32_t curr_idx = sample_count - 1U;
    const env_sample_t *p_curr = &p_samples[curr_idx];

    /* Defensive validation of newest sample */
    if (isnan(p_curr->p0_hpa) || isnan(p_curr->rh_pct) ||
        isnan(p_curr->temp_c) || isnan(p_curr->lux) ||
        isinf(p_curr->p0_hpa) || isinf(p_curr->rh_pct) ||
        isinf(p_curr->temp_c) || isinf(p_curr->lux)) {
        return ERR_INVALID_ARG_CODE;
    }

    /* Evaluate 30-minute differential (3 samples / intervals ago) */
    if (sample_count > TREND_SAMPLES_30MIN) {
        uint32_t idx_30m = curr_idx - TREND_SAMPLES_30MIN;
        const env_sample_t *p_30m = &p_samples[idx_30m];

        if (!isnan(p_30m->lux) && !isinf(p_30m->lux)) {
            p_gradients->delta_lux_30m = p_curr->lux - p_30m->lux;

            /* Compute relative solar drop percentage if daytime insolation was significant */
            if (p_30m->lux >= SOLAR_DAYLIGHT_MIN_LUX) {
                float drop = p_30m->lux - p_curr->lux;
                if (drop > 0.0f) {
                    p_gradients->solar_drop_pct_30m = (drop / p_30m->lux) * 100.0f;
                } else {
                    p_gradients->solar_drop_pct_30m = 0.0f;
                }
            } else {
                p_gradients->solar_drop_pct_30m = 0.0f;
            }
        }
    }

    /* Evaluate 1-hour differential (6 samples / intervals ago) */
    if (sample_count > TREND_SAMPLES_1HOUR) {
        uint32_t idx_1h = curr_idx - TREND_SAMPLES_1HOUR;
        const env_sample_t *p_1h = &p_samples[idx_1h];

        if (!isnan(p_1h->p0_hpa) && !isnan(p_1h->rh_pct) &&
            !isnan(p_1h->temp_c) && !isnan(p_1h->lux) &&
            !isinf(p_1h->p0_hpa) && !isinf(p_1h->rh_pct) &&
            !isinf(p_1h->temp_c) && !isinf(p_1h->lux)) {
            p_gradients->delta_p_1h_hpa = p_curr->p0_hpa - p_1h->p0_hpa;
            p_gradients->delta_rh_1h_pct = p_curr->rh_pct - p_1h->rh_pct;
            p_gradients->delta_t_1h_c = p_curr->temp_c - p_1h->temp_c;
            p_gradients->delta_lux_1h = p_curr->lux - p_1h->lux;
        }
    } else if (sample_count > 1U) {
        /* Scaled 1-hour estimate for initial bootstrap */
        const env_sample_t *p_oldest = &p_samples[0];
        if (!isnan(p_oldest->p0_hpa) && !isnan(p_oldest->rh_pct) &&
            !isnan(p_oldest->temp_c) &&
            !isinf(p_oldest->p0_hpa) && !isinf(p_oldest->rh_pct) &&
            !isinf(p_oldest->temp_c)) {
            uint32_t intervals = sample_count - 1U;
            float scale = (float)TREND_SAMPLES_1HOUR / (float)intervals;

            p_gradients->delta_p_1h_hpa = (p_curr->p0_hpa - p_oldest->p0_hpa) * scale;
            p_gradients->delta_rh_1h_pct = (p_curr->rh_pct - p_oldest->rh_pct) * scale;
            p_gradients->delta_t_1h_c = (p_curr->temp_c - p_oldest->temp_c) * scale;
            if (!isnan(p_oldest->lux) && !isinf(p_oldest->lux)) {
                p_gradients->delta_lux_1h = (p_curr->lux - p_oldest->lux) * scale;
            }
        }
    }

    /* Evaluate 3-hour differential (18 samples / intervals ago) */
    if (sample_count > TREND_SAMPLES_3HOUR) {
        uint32_t idx_3h = curr_idx - TREND_SAMPLES_3HOUR;
        const env_sample_t *p_3h = &p_samples[idx_3h];

        if (!isnan(p_3h->p0_hpa) && !isnan(p_3h->rh_pct) &&
            !isnan(p_3h->temp_c) &&
            !isinf(p_3h->p0_hpa) && !isinf(p_3h->rh_pct) &&
            !isinf(p_3h->temp_c)) {
            p_gradients->delta_p_3h_hpa = p_curr->p0_hpa - p_3h->p0_hpa;
            p_gradients->delta_rh_3h_pct = p_curr->rh_pct - p_3h->rh_pct;
            p_gradients->delta_t_3h_c = p_curr->temp_c - p_3h->temp_c;
            p_gradients->is_history_complete = true;
        }
    } else if (sample_count >= TREND_SAMPLES_1HOUR) {
        /* Scaled 3-hour extrapolation if at least 1 hour available */
        const env_sample_t *p_oldest = &p_samples[0];
        if (!isnan(p_oldest->p0_hpa) && !isnan(p_oldest->rh_pct) &&
            !isnan(p_oldest->temp_c) &&
            !isinf(p_oldest->p0_hpa) && !isinf(p_oldest->rh_pct) &&
            !isinf(p_oldest->temp_c)) {
            uint32_t intervals = sample_count - 1U;
            if (intervals > 0U) {
                float scale = (float)TREND_SAMPLES_3HOUR / (float)intervals;

                p_gradients->delta_p_3h_hpa = (p_curr->p0_hpa - p_oldest->p0_hpa) * scale;
                p_gradients->delta_rh_3h_pct = (p_curr->rh_pct - p_oldest->rh_pct) * scale;
                p_gradients->delta_t_3h_c = (p_curr->temp_c - p_oldest->temp_c) * scale;
                p_gradients->is_history_complete = false;
            }
        }
    }

    return STATUS_OK_CODE;
}
