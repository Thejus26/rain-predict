/**
 * @file    rain_algo.h
 * @brief   Composite edge rain nowcasting scoring engine.
 * @details Evaluates normalized multi-variable sub-scores and computes
 *          the weighted Composite Precipitation Index (CPI 0..100%).
 * 
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#ifndef RAIN_ALGO_H
#define RAIN_ALGO_H

#include <stdint.h>
#include <stdbool.h>
#include "status.h"
#include "dew_point.h"
#include "zambretti.h"
#include "trend_detector.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STATUS_ERR_INVALID_ARG
#define STATUS_ERR_INVALID_ARG STATUS_ERR_INVALID_PARAM
#endif

/**
 * @brief Normalized sub-scores and composite prediction output structure.
 */
typedef struct {
    uint8_t score_pressure;                /**< Sp: Pressure sub-score (0..100) */
    uint8_t score_humidity;                /**< Srh: Humidity sub-score (0..100) */
    uint8_t score_dew_point;               /**< Sdpd: Dew point depression sub-score (0..100) */
    uint8_t score_solar;                   /**< Ssol: Solar cloud attenuation sub-score (0..100) */
    uint8_t score_zambretti;               /**< Szam: Zambretti macro sub-score (0..100) */
    float cpi_score_pct;                   /**< Composite Precipitation Index CPI (0.0 to 100.0%) */
    uint8_t z_index;                       /**< Evaluated Zambretti index (1..26) */
    pressure_trend_state_t pressure_state; /**< Classified barometric state */
    solar_cloud_state_t solar_state;       /**< Classified solar cloud state */
    rain_forecast_state_t forecast_state;  /**< Operational alert state (UNLIKELY, POSSIBLE, IMMINENT) */
} rain_forecast_t;

/**
 * @brief Standard scoring weights (Daytime).
 */
#define WEIGHT_PRESSURE_DAY         (0.30f)
#define WEIGHT_HUMIDITY_DAY         (0.25f)
#define WEIGHT_DEW_POINT_DAY        (0.20f)
#define WEIGHT_SOLAR_DAY            (0.15f)
#define WEIGHT_ZAMBRETTI_DAY        (0.10f)

/**
 * @brief Nighttime re-normalized divisor.
 */
#define WEIGHT_NIGHT_DIVISOR        (0.85f)

/**
 * @brief Computes relative humidity sub-score (0 to 100).
 * 
 * @param[in]  rh_pct         Current relative humidity (%).
 * @param[in]  delta_rh_1h    1-hour rate of humidity change (%/hr).
 * @param[out] p_score        Pointer to store score (0..100).
 * @return status_t           STATUS_OK on success, error code otherwise.
 */
status_t rain_algo_score_humidity(float rh_pct, float delta_rh_1h, uint8_t *p_score);

/**
 * @brief Computes dew point depression sub-score (0 to 100).
 * 
 * @param[in]  dpd_c          Dew point depression T - Tdew (°C).
 * @param[out] p_score        Pointer to store score (0..100).
 * @return status_t           STATUS_OK on success, error code otherwise.
 */
status_t rain_algo_score_dew_point(float dpd_c, uint8_t *p_score);

/**
 * @brief Computes Zambretti macro sub-score (0 to 100).
 * 
 * @param[in]  z_index        Zambretti index (1 to 26).
 * @param[out] p_score        Pointer to store score (0..100).
 * @return status_t           STATUS_OK on success, error code otherwise.
 */
status_t rain_algo_score_zambretti(uint8_t z_index, uint8_t *p_score);

/**
 * @brief Computes weighted Composite Precipitation Index (CPI).
 * 
 * @param[in]  s_p            Pressure score (0..100).
 * @param[in]  s_rh           Humidity score (0..100).
 * @param[in]  s_dpd          Dew point score (0..100).
 * @param[in]  s_sol          Solar attenuation score (0..100).
 * @param[in]  s_zam          Zambretti score (0..100).
 * @param[in]  is_daylight    True if daylight active, false for nighttime re-normalization.
 * @param[out] p_cpi_pct      Pointer to store final calculated CPI (0.0 to 100.0%).
 * @return status_t           STATUS_OK on success, error code otherwise.
 */
status_t rain_algo_compute_composite_score(uint8_t s_p,
                                           uint8_t s_rh,
                                           uint8_t s_dpd,
                                           uint8_t s_sol,
                                           uint8_t s_zam,
                                           bool is_daylight,
                                           float *p_cpi_pct);

#ifdef __cplusplus
}
#endif

#endif /* RAIN_ALGO_H */
