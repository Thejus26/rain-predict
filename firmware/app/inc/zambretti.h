/**
 * @file zambretti.h
 * @brief Zambretti barometric heuristic forecaster and trend classification engine.
 * @details Implements 3-hour pressure trend classification, sea-level mapping,
 *          seasonal wind weighting, and 26-state heuristic lookups.
 * 
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#ifndef ZAMBRETTI_H
#define ZAMBRETTI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Barometric 3-hour pressure trend states.
 */
typedef enum {
    BARO_TREND_FALLING = 0, /**< Pressure dropping > 1.5 hPa/3h (Approaching trough/storm) */
    BARO_TREND_STEADY  = 1, /**< Pressure stable within +/- 1.5 hPa/3h */
    BARO_TREND_RISING  = 2  /**< Pressure rising > 1.5 hPa/3h (Building anticyclone/clearing) */
} baro_trend_t;

/**
 * @brief Operational rain forecast alert states.
 */
typedef enum {
    RAIN_STATE_UNLIKELY  = 0, /**< Settled / Fine / Clear (Green Alert) */
    RAIN_STATE_POSSIBLE  = 1, /**< Unsettled / Showers Likely (Yellow Alert) */
    RAIN_STATE_IMMINENT  = 2  /**< Active Rain / Storm / Heavy Squall (Red Alert) */
} rain_forecast_state_t;

/**
 * @brief Plantation monsoon seasons.
 */
typedef enum {
    MONSOON_SEASON_DRY          = 0, /**< Winter dry period (Dec-Feb) */
    MONSOON_SEASON_PRE_MONSOON  = 1, /**< Pre-monsoon convective squalls (Mar-May) */
    MONSOON_SEASON_SW_MONSOON   = 2, /**< Southwest monsoon heavy rain (Jun-Sep) */
    MONSOON_SEASON_NE_MONSOON   = 3  /**< Northeast retreating monsoon (Oct-Nov) */
} monsoon_season_t;

/**
 * @brief 16-point compass wind direction enumeration.
 */
typedef enum {
    WIND_DIR_CALM       = 0,
    WIND_DIR_N          = 1,
    WIND_DIR_NNE        = 2,
    WIND_DIR_NE         = 3,
    WIND_DIR_ENE        = 4,
    WIND_DIR_E          = 5,
    WIND_DIR_ESE        = 6,
    WIND_DIR_SE         = 7,
    WIND_DIR_SSE        = 8,
    WIND_DIR_S          = 9,
    WIND_DIR_SSW        = 10,
    WIND_DIR_SW         = 11,
    WIND_DIR_WSW        = 12,
    WIND_DIR_W          = 13,
    WIND_DIR_WNW        = 14,
    WIND_DIR_NW         = 15,
    WIND_DIR_NNW        = 16,
    WIND_DIR_UNKNOWN    = 17
} wind_dir_t;

/**
 * @brief Standard barometric trend classification constants.
 */
#define ZAMBRETTI_TREND_THRESHOLD_HPA   (1.50f)     /**< 3-hour delta threshold in hPa */
#define ZAMBRETTI_MIN_HISTORY_SAMPLES   (6U)        /**< Minimum 1-hour history (6 samples @ 10m) */
#define ZAMBRETTI_FULL_HISTORY_SAMPLES  (18U)       /**< Full 3-hour history (18 samples @ 10m) */

/**
 * @brief Pressure clamping constants for Zambretti polynomials.
 */
#define ZAMBRETTI_P0_MIN_HPA            (985.0f)    /**< Lowest nominal sea-level pressure */
#define ZAMBRETTI_P0_MAX_HPA            (1050.0f)   /**< Highest nominal sea-level pressure */

/**
 * @brief Zambretti index bounds.
 */
#define ZAMBRETTI_INDEX_MIN             (1U)        /**< Settled Fine Weather */
#define ZAMBRETTI_INDEX_MAX             (26U)       /**< Severe Storm, Heavy Rain */

/**
 * @brief Operational alert threshold partitions.
 */
#define ZAMBRETTI_ALERT_UNLIKELY_MAX    (10U)       /**< Z <= 10 -> RAIN_STATE_UNLIKELY */
#define ZAMBRETTI_ALERT_POSSIBLE_MAX    (19U)       /**< 11 <= Z <= 19 -> RAIN_STATE_POSSIBLE */

#define WIND_CALM_THRESHOLD_MPS         (0.5f)      /**< Speed below which wind is considered calm */

#define ZAMBRETTI_OK                    (0)
#define ZAMBRETTI_ERR_NULL_PTR          (-1)
#define ZAMBRETTI_ERR_INVALID_ARG       (-2)
#define ZAMBRETTI_ERR_INSUFFICIENT_DATA (-3)

/**
 * @brief Classifies barometric trend directly from a 3-hour pressure delta.
 * 
 * @param[in]  delta_p_3h  Barometric pressure change over 3 hours (hPa).
 * @param[out] p_trend     Pointer to output baro_trend_t enum.
 * @return int32_t         0 on success, negative error code on failure.
 */
int32_t zambretti_classify_trend(float delta_p_3h, baro_trend_t *p_trend);

/**
 * @brief Computes 3-hour pressure delta and trend from historical pressure array.
 * 
 * @param[in]  p_history      Pointer to array of chronological sea-level pressure samples.
 * @param[in]  sample_count   Number of valid samples in history (must be >= 6).
 * @param[out] p_delta_p_3h   Pointer to store calculated (or scaled) 3-hour delta (hPa).
 * @param[out] p_trend        Pointer to store classified baro_trend_t.
 * @return int32_t            0 on success, negative error code on failure.
 */
int32_t zambretti_compute_trend_from_history(const float *p_history,
                                             uint32_t sample_count,
                                             float *p_delta_p_3h,
                                             baro_trend_t *p_trend);

/**
 * @brief Calculates raw continuous Zambretti index from P0 and classified trend.
 * 
 * @param[in]  p0_hpa     Sea-level equivalent pressure in hPa.
 * @param[in]  trend      Classified barometric trend (FALLING, STEADY, RISING).
 * @param[out] p_z_raw    Pointer to store raw continuous index value.
 * @return int32_t        0 on success, negative error code on failure.
 */
int32_t zambretti_calc_raw_index(float p0_hpa, baro_trend_t trend, float *p_z_raw);

/**
 * @brief Maps sea-level pressure and trend to a discrete Zambretti index (1 to 26).
 * 
 * @param[in]  p0_hpa     Sea-level equivalent pressure in hPa.
 * @param[in]  trend      Classified barometric trend.
 * @param[out] p_z_index  Pointer to store discrete integer index (1 to 26).
 * @return int32_t        0 on success, negative error code on failure.
 */
int32_t zambretti_map_to_index(float p0_hpa, baro_trend_t trend, uint8_t *p_z_index);

/**
 * @brief Maps a discrete Zambretti index (1..26) to an operational alert state.
 * 
 * @param[in]  z_index    Discrete Zambretti index (1 to 26).
 * @return rain_forecast_state_t Evaluated alert state (UNLIKELY, POSSIBLE, IMMINENT).
 */
rain_forecast_state_t zambretti_map_to_state(uint8_t z_index);

/**
 * @brief Returns English descriptive forecast string for a given Zambretti index.
 * 
 * @param[in]  z_index    Discrete Zambretti index (1 to 26).
 * @return const char*    Pointer to flash-resident null-terminated string descriptor.
 */
const char *zambretti_get_forecast_text(uint8_t z_index);

/**
 * @brief Full end-to-end Zambretti forecast calculation.
 * 
 * @param[in]  p0_hpa       Current sea-level pressure (hPa).
 * @param[in]  delta_p_3h   Pressure change over 3 hours (hPa).
 * @param[out] p_z_index    Optional pointer to receive discrete index (1..26).
 * @param[out] p_state      Optional pointer to receive operational forecast state.
 * @return int32_t          0 on success, negative error code on failure.
 */
int32_t zambretti_calculate(float p0_hpa,
                            float delta_p_3h,
                            uint8_t *p_z_index,
                            rain_forecast_state_t *p_state);

/**
 * @brief Retrieves seasonal Zambretti index adjustment based on month of the year.
 * 
 * @param[in]  month_1_to_12 Month index (1 = January, 12 = December).
 * @param[out] p_offset      Pointer to store seasonal index offset (-2 to +2).
 * @return int32_t           0 on success, negative error code on invalid month.
 */
int32_t zambretti_get_seasonal_offset(uint8_t month_1_to_12, int8_t *p_offset);

/**
 * @brief Retrieves seasonal adjustment from explicit monsoon season enum.
 * 
 * @param[in]  season        Monsoon season enum.
 * @param[out] p_offset      Pointer to store index offset (-2 to +2).
 * @return int32_t           0 on success, negative error code on failure.
 */
int32_t zambretti_get_monsoon_offset(monsoon_season_t season, int8_t *p_offset);

/**
 * @brief Retrieves wind direction Zambretti index adjustment.
 * 
 * @param[in]  dir           16-point wind direction compass code.
 * @param[in]  speed_mps     Wind speed in meters per second.
 * @param[out] p_offset      Pointer to store wind index offset (-1 to +2).
 * @return int32_t           0 on success, negative error code on failure.
 */
int32_t zambretti_get_wind_offset(wind_dir_t dir, float speed_mps, int8_t *p_offset);

/**
 * @brief Converts azimuth degrees (0.0 to 360.0) into 16-point wind_dir_t enum.
 * 
 * @param[in]  azimuth_deg   Wind direction in azimuth degrees (0.0° = North, clockwise).
 * @return wind_dir_t        Corresponding 16-point compass sector.
 */
wind_dir_t zambretti_azimuth_to_wind_dir(float azimuth_deg);

/**
 * @brief Computes fully adjusted, weighted Zambretti forecast index.
 * 
 * @param[in]  p0_hpa        Current sea-level pressure in hPa.
 * @param[in]  delta_p_3h    3-hour barometric pressure delta in hPa.
 * @param[in]  month_1_to_12 Current month from RTC (1 to 12).
 * @param[in]  wind_dir      Wind direction compass sector.
 * @param[in]  wind_speed_mps Current wind speed in m/s.
 * @param[out] p_z_index     Pointer to store final weighted Zambretti index (1 to 26).
 * @param[out] p_state       Pointer to store operational alert state.
 * @return int32_t           0 on success, negative error code on failure.
 */
int32_t zambretti_calculate_weighted(float p0_hpa,
                                     float delta_p_3h,
                                     uint8_t month_1_to_12,
                                     wind_dir_t wind_dir,
                                     float wind_speed_mps,
                                     uint8_t *p_z_index,
                                     rain_forecast_state_t *p_state);

#ifdef __cplusplus
}
#endif

#endif /* ZAMBRETTI_H */
