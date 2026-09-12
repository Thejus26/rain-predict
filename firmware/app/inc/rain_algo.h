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
 * @brief 4-tier operational rain forecast states.
 */
typedef enum {
    RAIN_ALERT_UNLIKELY     = 0, /**< CPI < 30% (Settled fine weather, Green Alert) */
    RAIN_ALERT_POSSIBLE     = 1, /**< 30% <= CPI < 60% (Unsettled / Showers possible, Yellow Alert) */
    RAIN_ALERT_LIKELY       = 2, /**< 60% <= CPI < 80% (High probability in 1-2h, Orange Alert) */
    RAIN_ALERT_IMMINENT     = 3  /**< CPI >= 80% or Critical Trigger (Active storm in 15-30m, Red Alert) */
} rain_alert_state_t;

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
    rain_alert_state_t forecast_state;     /**< Operational alert state (UNLIKELY, POSSIBLE, LIKELY, IMMINENT) */
} rain_forecast_t;

/**
 * @brief CPI operational alert thresholds.
 */
#define CPI_THRESH_POSSIBLE_PCT     (30.0f)
#define CPI_THRESH_LIKELY_PCT       (60.0f)
#define CPI_THRESH_IMMINENT_PCT     (80.0f)

/**
 * @brief Critical emergency override thresholds.
 */
#define CRITICAL_DROP_1H_HPA        (-2.00f)
#define CRITICAL_DROP_RH_MIN_PCT    (88.0f)
#define CRITICAL_SOLAR_DROP_PCT     (75.0f)
#define CRITICAL_SOLAR_LUX_MAX      (2000.0f)
#define CRITICAL_SAT_DPD_MAX_C      (0.30f)
#define CRITICAL_SAT_RH_MIN_PCT     (98.0f)

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

/**
 * @brief Classifies the discrete rain alert state from CPI and critical override parameters.
 * 
 * @param[in]  cpi_pct        Composite Precipitation Index (0.0 to 100.0%).
 * @param[in]  delta_p_1h     1-hour pressure delta in hPa.
 * @param[in]  rh_pct         Current relative humidity in %.
 * @param[in]  dpd_c          Dew point depression in °C.
 * @param[in]  drop_solar_pct 30-minute relative solar drop percentage.
 * @param[in]  lux_curr       Current solar illuminance in Lux.
 * @param[out] p_state        Pointer to store classified rain_alert_state_t.
 * @return status_t           STATUS_OK on success, error code otherwise.
 */
status_t rain_algo_classify_state(float cpi_pct,
                                  float delta_p_1h,
                                  float rh_pct,
                                  float dpd_c,
                                  float drop_solar_pct,
                                  float lux_curr,
                                  rain_alert_state_t *p_state);

/**
 * @brief Master Coordinator: Executes the full end-to-end rain nowcasting pipeline.
 * 
 * @param[in]  p_samples      Historical array of environmental samples (oldest to newest).
 * @param[in]  sample_count   Number of valid samples in array (must be >= 1).
 * @param[in]  altitude_m     Station mast elevation above MSL (meters).
 * @param[in]  month_1_to_12  Current RTC calendar month (1 to 12).
 * @param[in]  wind_dir       16-point wind direction compass code.
 * @param[in]  wind_speed_mps Current wind speed in m/s.
 * @param[out] p_forecast     Pointer to destination rain_forecast_t struct.
 * @return status_t           STATUS_OK on success, error code otherwise.
 */
status_t rain_algo_evaluate(const env_sample_t *p_samples,
                            uint32_t sample_count,
                            float altitude_m,
                            uint8_t month_1_to_12,
                            wind_dir_t wind_dir,
                            float wind_speed_mps,
                            rain_forecast_t *p_forecast);

/**
 * @brief Returns descriptive English name for a rain alert state.
 * 
 * @param[in]  state          Operational rain alert state.
 * @return const char*        Pointer to flash string descriptor.
 */
const char *rain_algo_get_alert_state_name(rain_alert_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* RAIN_ALGO_H */
