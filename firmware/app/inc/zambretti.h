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

#ifdef __cplusplus
}
#endif

#endif /* ZAMBRETTI_H */
