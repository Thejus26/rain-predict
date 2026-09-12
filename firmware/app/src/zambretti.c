/**
 * @file zambretti.c
 * @brief Implementation of Zambretti pressure trend classification and forecasting engine.
 */

#include "zambretti.h"
#include <math.h>
#include <stddef.h>

static const char * const s_zambretti_descriptions[26] = {
    "Settled Fine Weather",                 /* 1 */
    "Fine Weather",                         /* 2 */
    "Becoming Fine",                        /* 3 */
    "Fine, Becoming Less Settled",          /* 4 */
    "Fine, Possible Showers",               /* 5 */
    "Fairly Fine, Improving",               /* 6 */
    "Fairly Fine, Possible Showers Early",  /* 7 */
    "Fairly Fine, Showers Later",           /* 8 */
    "Showery Early, Improving",             /* 9 */
    "Changeable, Mending",                  /* 10 */
    "Fairly Fine, Showers Likely",          /* 11 */
    "Rather Unsettled, Clearing Later",     /* 12 */
    "Unsettled, Probably Improving",        /* 13 */
    "Showery, Bright Intervals",            /* 14 */
    "Showery, Becoming More Unsettled",     /* 15 */
    "Changeable, Some Rain",                /* 16 */
    "Unsettled, Short Fine Intervals",      /* 17 */
    "Unsettled, Rain Later",                /* 18 */
    "Unsettled, Some Rain",                 /* 19 */
    "Mostly Very Unsettled",                /* 20 */
    "Occasional Rain, Worsening",           /* 21 */
    "Rain at Times, Very Unsettled",        /* 22 */
    "Rain at Frequent Intervals",           /* 23 */
    "Rain, Very Unsettled",                 /* 24 */
    "Stormy, Much Rain",                    /* 25 */
    "Severe Storm, Heavy Rain"              /* 26 */
};

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

int32_t zambretti_calc_raw_index(float p0_hpa, baro_trend_t trend, float *p_z_raw)
{
    if (p_z_raw == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }
    if (isnan(p0_hpa)) {
        return ZAMBRETTI_ERR_INVALID_ARG;
    }

    /* Clamp P0 to nominal meteorological boundaries */
    float p0 = p0_hpa;
    if (p0 < ZAMBRETTI_P0_MIN_HPA) {
        p0 = ZAMBRETTI_P0_MIN_HPA;
    } else if (p0 > ZAMBRETTI_P0_MAX_HPA) {
        p0 = ZAMBRETTI_P0_MAX_HPA;
    }

    switch (trend) {
        case BARO_TREND_RISING:
            /* Formula A: Z = 185 - 0.16 * P0 */
            *p_z_raw = 185.0f - (0.16f * p0);
            break;

        case BARO_TREND_STEADY:
            /* Formula B: Z = 144 - 0.13 * P0 */
            *p_z_raw = 144.0f - (0.13f * p0);
            break;

        case BARO_TREND_FALLING:
            /* Formula C: Z = 127 - 0.12 * P0 */
            *p_z_raw = 127.0f - (0.12f * p0);
            break;

        default:
            return ZAMBRETTI_ERR_INVALID_ARG;
    }

    return ZAMBRETTI_OK;
}

int32_t zambretti_map_to_index(float p0_hpa, baro_trend_t trend, uint8_t *p_z_index)
{
    if (p_z_index == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }

    float z_raw = 0.0f;
    int32_t ret = zambretti_calc_raw_index(p0_hpa, trend, &z_raw);
    if (ret != ZAMBRETTI_OK) {
        return ret;
    }

    int32_t z_int = (int32_t)roundf(z_raw);
    if (z_int < (int32_t)ZAMBRETTI_INDEX_MIN) {
        z_int = (int32_t)ZAMBRETTI_INDEX_MIN;
    } else if (z_int > (int32_t)ZAMBRETTI_INDEX_MAX) {
        z_int = (int32_t)ZAMBRETTI_INDEX_MAX;
    }

    *p_z_index = (uint8_t)z_int;
    return ZAMBRETTI_OK;
}

rain_forecast_state_t zambretti_map_to_state(uint8_t z_index)
{
    if (z_index <= ZAMBRETTI_ALERT_UNLIKELY_MAX) {
        return RAIN_STATE_UNLIKELY;
    } else if (z_index <= ZAMBRETTI_ALERT_POSSIBLE_MAX) {
        return RAIN_STATE_POSSIBLE;
    } else {
        return RAIN_STATE_IMMINENT;
    }
}

const char *zambretti_get_forecast_text(uint8_t z_index)
{
    if (z_index < ZAMBRETTI_INDEX_MIN || z_index > ZAMBRETTI_INDEX_MAX) {
        return "Unknown Zambretti Index";
    }
    return s_zambretti_descriptions[z_index - 1U];
}

int32_t zambretti_calculate(float p0_hpa,
                            float delta_p_3h,
                            uint8_t *p_z_index,
                            rain_forecast_state_t *p_state)
{
    baro_trend_t trend = BARO_TREND_STEADY;
    int32_t ret = zambretti_classify_trend(delta_p_3h, &trend);
    if (ret != ZAMBRETTI_OK) {
        return ret;
    }

    uint8_t z = 0U;
    ret = zambretti_map_to_index(p0_hpa, trend, &z);
    if (ret != ZAMBRETTI_OK) {
        return ret;
    }

    if (p_z_index != NULL) {
        *p_z_index = z;
    }
    if (p_state != NULL) {
        *p_state = zambretti_map_to_state(z);
    }

    return ZAMBRETTI_OK;
}
