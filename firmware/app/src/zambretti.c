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

/* Flash-resident monthly seasonal offset LUT (indices 0..11 for months 1..12) */
static const int8_t s_seasonal_monthly_offsets[12] = {
    -2, /* Jan: Winter Dry */
    -2, /* Feb: Winter Dry */
     0, /* Mar: Early Pre-Monsoon */
    +1, /* Apr: Peak Pre-Monsoon */
    +1, /* May: Pre-Monsoon Squall Transition */
    +2, /* Jun: Southwest Monsoon */
    +2, /* Jul: Peak Southwest Monsoon */
    +2, /* Aug: Southwest Monsoon */
    +1, /* Sep: Southwest Monsoon Weakening */
    +2, /* Oct: Northeast Monsoon */
    +1, /* Nov: Northeast Monsoon */
    -1  /* Dec: Early Winter */
};

/* Flash-resident wind direction offset LUT (indices match wind_dir_t 0..17) */
static const int8_t s_wind_direction_offsets[18] = {
     0, /* WIND_DIR_CALM */
    -1, /* WIND_DIR_N */
    -1, /* WIND_DIR_NNE */
    -1, /* WIND_DIR_NE */
     0, /* WIND_DIR_ENE */
     0, /* WIND_DIR_E */
     0, /* WIND_DIR_ESE */
    +1, /* WIND_DIR_SE */
    +1, /* WIND_DIR_SSE */
    +1, /* WIND_DIR_S */
    +2, /* WIND_DIR_SSW */
    +2, /* WIND_DIR_SW */
    +2, /* WIND_DIR_WSW */
    +1, /* WIND_DIR_W */
     0, /* WIND_DIR_WNW */
     0, /* WIND_DIR_NW */
    -1, /* WIND_DIR_NNW */
     0  /* WIND_DIR_UNKNOWN */
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

int32_t zambretti_get_seasonal_offset(uint8_t month_1_to_12, int8_t *p_offset)
{
    if (p_offset == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }
    if (month_1_to_12 < 1U || month_1_to_12 > 12U) {
        return ZAMBRETTI_ERR_INVALID_ARG;
    }

    *p_offset = s_seasonal_monthly_offsets[month_1_to_12 - 1U];
    return ZAMBRETTI_OK;
}

int32_t zambretti_get_monsoon_offset(monsoon_season_t season, int8_t *p_offset)
{
    if (p_offset == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }

    switch (season) {
        case MONSOON_SEASON_DRY:
            *p_offset = -2;
            break;
        case MONSOON_SEASON_PRE_MONSOON:
            *p_offset = +1;
            break;
        case MONSOON_SEASON_SW_MONSOON:
            *p_offset = +2;
            break;
        case MONSOON_SEASON_NE_MONSOON:
            *p_offset = +2;
            break;
        default:
            return ZAMBRETTI_ERR_INVALID_ARG;
    }

    return ZAMBRETTI_OK;
}

int32_t zambretti_get_wind_offset(wind_dir_t dir, float speed_mps, int8_t *p_offset)
{
    if (p_offset == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }
    if (isnan(speed_mps) || speed_mps < 0.0f) {
        return ZAMBRETTI_ERR_INVALID_ARG;
    }

    /* If calm or unconfigured, zero offset */
    if (speed_mps < WIND_CALM_THRESHOLD_MPS || dir == WIND_DIR_CALM || dir >= WIND_DIR_UNKNOWN) {
        *p_offset = 0;
        return ZAMBRETTI_OK;
    }

    *p_offset = s_wind_direction_offsets[(uint8_t)dir];
    return ZAMBRETTI_OK;
}

wind_dir_t zambretti_azimuth_to_wind_dir(float azimuth_deg)
{
    if (isnan(azimuth_deg)) {
        return WIND_DIR_UNKNOWN;
    }

    /* Wrap to [0, 360) */
    float az = fmodf(azimuth_deg, 360.0f);
    if (az < 0.0f) {
        az += 360.0f;
    }

    /* 16 sectors of 22.5° each, offset by 11.25° for North centering */
    int32_t sector = (int32_t)floorf((az + 11.25f) / 22.5f) % 16;
    return (wind_dir_t)(sector + 1); /* +1 because WIND_DIR_CALM = 0 */
}

int32_t zambretti_calculate_weighted(float p0_hpa,
                                     float delta_p_3h,
                                     uint8_t month_1_to_12,
                                     wind_dir_t wind_dir,
                                     float wind_speed_mps,
                                     uint8_t *p_z_index,
                                     rain_forecast_state_t *p_state)
{
    if (p_z_index == NULL || p_state == NULL) {
        return ZAMBRETTI_ERR_NULL_PTR;
    }

    uint8_t z_base = 0U;
    rain_forecast_state_t base_state;
    int32_t ret = zambretti_calculate(p0_hpa, delta_p_3h, &z_base, &base_state);
    if (ret != ZAMBRETTI_OK) {
        return ret;
    }

    int8_t season_offset = 0;
    ret = zambretti_get_seasonal_offset(month_1_to_12, &season_offset);
    if (ret != ZAMBRETTI_OK) {
        season_offset = 0; /* Fallback to neutral on invalid month */
    }

    int8_t wind_offset = 0;
    ret = zambretti_get_wind_offset(wind_dir, wind_speed_mps, &wind_offset);
    if (ret != ZAMBRETTI_OK) {
        wind_offset = 0;
    }

    int32_t z_weighted = (int32_t)z_base + (int32_t)season_offset + (int32_t)wind_offset;
    if (z_weighted < (int32_t)ZAMBRETTI_INDEX_MIN) {
        z_weighted = (int32_t)ZAMBRETTI_INDEX_MIN;
    } else if (z_weighted > (int32_t)ZAMBRETTI_INDEX_MAX) {
        z_weighted = (int32_t)ZAMBRETTI_INDEX_MAX;
    }

    *p_z_index = (uint8_t)z_weighted;
    *p_state = zambretti_map_to_state((uint8_t)z_weighted);

    return ZAMBRETTI_OK;
}
