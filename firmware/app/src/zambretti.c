/**
 * @file zambretti.c
 * @brief Implementation of Zambretti pressure trend classification and forecasting engine.
 */

#include "zambretti.h"
#include <math.h>
#include <stddef.h>

int32_t zambretti_classify_trend(float delta_p_3h, baro_trend_t *p_trend)
{
    if (p_trend == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }
    if (isnan(delta_p_3h)) {
        return ZAMBRETTI_ERR_INVALID_ARG;
    }

    if (delta_p_3h < -ZAMBRETTI_TREND_THRESHOLD_HPA) {
        *p_trend = BARO_TREND_FALLING;
    } else if (delta_p_3h > ZAMBRETTI_TREND_THRESHOLD_HPA) {
        *p_trend = BARO_TREND_RISING;
    } else {
        *p_trend = BARO_TREND_STEADY;
    }

    return ZAMBRETTI_OK;
}

int32_t zambretti_compute_trend_from_history(const float *p_history,
                                             uint32_t sample_count,
                                             float *p_delta_p_3h,
                                             baro_trend_t *p_trend)
{
    if (p_history == NULL || p_delta_p_3h == NULL || p_trend == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }
    if (sample_count < ZAMBRETTI_MIN_HISTORY_SAMPLES) {
        return ZAMBRETTI_ERR_INSUFFICIENT_DATA;
    }

    float p_newest = p_history[sample_count - 1];
    float p_oldest = p_history[0];

    if (isnan(p_newest) || isnan(p_oldest)) {
        return ZAMBRETTI_ERR_INVALID_ARG;
    }

    float delta_raw = p_newest - p_oldest;
    float delta_3h;

    if (sample_count >= ZAMBRETTI_FULL_HISTORY_SAMPLES) {
        /* Full 3-hour window available */
        delta_3h = delta_raw;
    } else {
        /* Linearly scale partial history (between 1h and 3h) to 3-hour equivalent */
        delta_3h = delta_raw * ((float)ZAMBRETTI_FULL_HISTORY_SAMPLES / (float)sample_count);
    }

    *p_delta_p_3h = delta_3h;
    return zambretti_classify_trend(delta_3h, p_trend);
}
