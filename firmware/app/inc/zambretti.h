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

#ifdef __cplusplus
}
#endif

#endif /* ZAMBRETTI_H */
