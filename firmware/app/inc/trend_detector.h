/**
 * @file    trend_detector.h
 * @brief   Multi-variable gradient differential and environmental trend detection engine.
 * @details Computes 1-hour and 3-hour rates of change for barometric pressure,
 *          relative humidity, temperature, and solar irradiance cloud attenuation.
 * 
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#ifndef TREND_DETECTOR_H
#define TREND_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status and return codes for trend detector functions.
 */
#define TREND_STATUS_OK             (0)
#define TREND_ERR_NULL_PTR         (-1)
#define TREND_ERR_INVALID_ARG      (-2)
#define TREND_ERR_INSUFFICIENT_DATA (-3)

/* Aliases matching specification naming */
#define STATUS_OK_CODE              TREND_STATUS_OK
#define ERR_NULL_PTR_CODE           TREND_ERR_NULL_PTR
#define ERR_INVALID_ARG_CODE        TREND_ERR_INVALID_ARG
#define ERR_INSUFFICIENT_DATA_CODE  TREND_ERR_INSUFFICIENT_DATA

/**
 * @brief History window index sample counts (at nominal 10-minute sampling interval).
 */
#define TREND_SAMPLES_30MIN         (3U)        /**< 30 minutes = 3 intervals */
#define TREND_SAMPLES_1HOUR         (6U)        /**< 1 hour = 6 intervals */
#define TREND_SAMPLES_3HOUR         (18U)       /**< 3 hours = 18 intervals */

/**
 * @brief Minimum daylight solar illuminance required to evaluate cloud attenuation drop ratio.
 */
#define SOLAR_DAYLIGHT_MIN_LUX      (5000.0f)   /**< Minimum prior lux to evaluate % drop */

/**
 * @brief Environmental sample snapshot for historical window buffers.
 */
typedef struct {
    float p0_hpa;            /**< Sea-level reduced barometric pressure (hPa) */
    float rh_pct;            /**< Relative humidity (%) */
    float temp_c;            /**< Ambient temperature (°C) */
    float lux;               /**< Solar illuminance (Lux) */
    uint32_t timestamp_s;    /**< RTC timestamp in seconds */
} env_sample_t;

/**
 * @brief Calculated multi-variable atmospheric gradient differentials.
 */
typedef struct {
    float delta_p_1h_hpa;        /**< 1-hour barometric delta (hPa/hr) */
    float delta_p_3h_hpa;        /**< 3-hour barometric delta (hPa/3hr) */
    float delta_rh_1h_pct;       /**< 1-hour relative humidity delta (%/hr) */
    float delta_rh_3h_pct;       /**< 3-hour relative humidity delta (%/3hr) */
    float delta_t_1h_c;          /**< 1-hour temperature delta (°C/hr) */
    float delta_t_3h_c;          /**< 3-hour temperature delta (°C/3hr) */
    float delta_lux_30m;         /**< 30-minute solar illuminance delta (Lux) */
    float delta_lux_1h;          /**< 1-hour solar illuminance delta (Lux) */
    float solar_drop_pct_30m;    /**< 30-minute relative solar attenuation (%) */
    bool is_history_complete;    /**< True if full 3-hour history (>= 18 intervals / 19 samples) available */
} multi_gradient_t;

/**
 * @brief Computes multi-variable gradient differentials from a historical array of samples.
 * 
 * @param[in]  p_samples      Chronological array of samples [0 = oldest, count-1 = newest].
 * @param[in]  sample_count   Number of valid samples in the array (must be >= 1).
 * @param[out] p_gradients    Pointer to output multi_gradient_t struct.
 * @return int32_t            0 on success, negative error code on failure.
 */
int32_t trend_detector_compute_gradients(const env_sample_t *p_samples,
                                         uint32_t sample_count,
                                         multi_gradient_t *p_gradients);

/**
 * @brief Barometric pressure trend state classifications.
 */
typedef enum {
    PRESSURE_RAPID_DROP     = 0, /**< Drop > 2.0 hPa/3h or > 1.5 hPa/1h (Severe storm risk) */
    PRESSURE_MODERATE_DROP  = 1, /**< Drop 1.0 to 2.0 hPa/3h (Developing low trough) */
    PRESSURE_SLOW_DROP      = 2, /**< Drop 0.0 to 1.0 hPa/3h (Weak diurnal decrease) */
    PRESSURE_STEADY         = 3, /**< Pressure change 0.0 to +1.0 hPa/3h (Stable field) */
    PRESSURE_RISING         = 4  /**< Pressure rise > 1.0 hPa/3h (Building anticyclone/clearing) */
} pressure_trend_state_t;

/**
 * @brief Pressure trend threshold constants.
 */
#define BARO_THRESH_RAPID_DROP_3H_HPA       (-2.00f)
#define BARO_THRESH_RAPID_DROP_1H_HPA       (-1.50f)
#define BARO_THRESH_MOD_DROP_3H_HPA         (-1.00f)
#define BARO_THRESH_SEVERE_SQUALL_3H_HPA    (-3.00f)

/**
 * @brief Classifies the discrete barometric pressure trend state.
 * 
 * @param[in]  delta_p_1h    1-hour barometric delta in hPa.
 * @param[in]  delta_p_3h    3-hour barometric delta in hPa.
 * @param[out] p_state       Pointer to store classified pressure_trend_state_t.
 * @return int32_t           0 on success, negative error code on failure.
 */
int32_t trend_detector_classify_pressure(float delta_p_1h,
                                         float delta_p_3h,
                                         pressure_trend_state_t *p_state);

/**
 * @brief Computes normalized barometric precipitation sub-score (0 to 100).
 * 
 * @param[in]  delta_p_1h    1-hour barometric delta in hPa.
 * @param[in]  delta_p_3h    3-hour barometric delta in hPa.
 * @param[out] p_score       Pointer to store normalized score (0 to 100).
 * @return int32_t           0 on success, negative error code on failure.
 */
int32_t trend_detector_score_pressure(float delta_p_1h,
                                      float delta_p_3h,
                                      uint8_t *p_score);

/**
 * @brief Returns descriptive name string for a given pressure trend state.
 * 
 * @param[in]  state         Classified pressure trend state.
 * @return const char*       Pointer to flash string descriptor.
 */
const char *trend_detector_get_pressure_state_name(pressure_trend_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* TREND_DETECTOR_H */
