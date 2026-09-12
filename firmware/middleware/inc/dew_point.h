/**
 * @file dew_point.h
 * @brief Thermodynamic psychrometric and dew point calculation engine.
 * @details Implements Magnus-Tetens formulations, vapor pressure calculations,
 *          absolute humidity, and barometric hypsometric sea-level reduction.
 * 
 * Target: STM32WLE5 (ARM Cortex-M4 with Single-Precision FPU)
 */

#ifndef DEW_POINT_H
#define DEW_POINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#ifndef STATUS_ERR_INVALID_ARG
#define STATUS_ERR_INVALID_ARG STATUS_ERR_INVALID_PARAM
#endif

/**
 * @brief Comprehensive psychrometric thermodynamic state structure.
 */
typedef struct {
    float temp_c;            /**< Ambient dry-bulb temperature (°C) */
    float rh_pct;            /**< Relative humidity (%) */
    float saturation_vp_hpa; /**< Saturation vapor pressure es(T) (hPa) */
    float actual_vp_hpa;     /**< Actual partial vapor pressure e(T, RH) (hPa) */
    float abs_humidity_gm3;  /**< Absolute humidity (g/m³) */
    float vpd_hpa;           /**< Vapor pressure deficit (hPa) */
    float dew_point_c;       /**< Dew point temperature Tdew (°C) */
    float dew_point_dep_c;   /**< Dew point depression T - Tdew (°C) */
} psychrometric_state_t;

/**
 * @brief Magnus-Tetens empirical constants (WMO / Sonntag standard over liquid water).
 */
#define MAGNUS_COEFF_A              (17.67f)
#define MAGNUS_COEFF_B              (243.5f)
#define MAGNUS_COEFF_C              (6.112f)
#define ABS_HUMIDITY_COEFF          (216.7f)
#define KELVIN_OFFSET               (273.15f)

/**
 * @brief Physical operational clamps.
 */
#define PSYCHRO_TEMP_MIN_C          (-40.0f)
#define PSYCHRO_TEMP_MAX_C          (85.0f)
#define PSYCHRO_RH_MIN_PCT          (0.1f)
#define PSYCHRO_RH_MAX_PCT          (100.0f)

/**
 * @brief Computes saturation vapor pressure es(T) over liquid water.
 * 
 * @param[in]  temp_c     Ambient temperature in degrees Celsius.
 * @param[out] p_es_hpa   Pointer to store calculated saturation vapor pressure (hPa).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_saturation_vp(float temp_c, float *p_es_hpa);

/**
 * @brief Computes actual partial vapor pressure e(T, RH).
 * 
 * @param[in]  temp_c     Ambient temperature in degrees Celsius.
 * @param[in]  rh_pct     Relative humidity in percent (0.0 to 100.0).
 * @param[out] p_e_hpa    Pointer to store calculated actual vapor pressure (hPa).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_actual_vp(float temp_c, float rh_pct, float *p_e_hpa);

/**
 * @brief Computes absolute humidity (volumetric vapor density) in g/m³.
 * 
 * @param[in]  temp_c     Ambient temperature in degrees Celsius.
 * @param[in]  rh_pct     Relative humidity in percent (0.0 to 100.0).
 * @param[out] p_ah_gm3   Pointer to store calculated absolute humidity (g/m³).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_abs_humidity(float temp_c, float rh_pct, float *p_ah_gm3);

/**
 * @brief Computes vapor pressure deficit (VPD) in hPa.
 * 
 * @param[in]  temp_c     Ambient temperature in degrees Celsius.
 * @param[in]  rh_pct     Relative humidity in percent (0.0 to 100.0).
 * @param[out] p_vpd_hpa  Pointer to store calculated VPD (hPa).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_vpd(float temp_c, float rh_pct, float *p_vpd_hpa);

/**
 * @brief Computes dew point temperature Tdew (°C) from ambient temperature and relative humidity.
 * 
 * @param[in]  temp_c     Ambient dry-bulb temperature in degrees Celsius (-40.0 to +85.0).
 * @param[in]  rh_pct     Relative humidity in percent (0.1 to 100.0).
 * @param[out] p_dew_c    Pointer to store calculated dew point temperature (°C).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_tdew(float temp_c, float rh_pct, float *p_dew_c);

/**
 * @brief Computes dew point depression (T - Tdew) in degrees Celsius.
 * 
 * @param[in]  temp_c     Ambient dry-bulb temperature in degrees Celsius.
 * @param[in]  rh_pct     Relative humidity in percent.
 * @param[out] p_dpd_c    Pointer to store calculated dew point depression (°C).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_depression(float temp_c, float rh_pct, float *p_dpd_c);

/**
 * @brief Computes dew point temperature directly from known actual partial vapor pressure e.
 * 
 * @param[in]  e_hpa      Actual vapor pressure in hPa (> 0.0 hPa).
 * @param[out] p_dew_c    Pointer to store calculated dew point temperature (°C).
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_from_vapor_pressure(float e_hpa, float *p_dew_c);

/**
 * @brief Computes complete psychrometric thermodynamic state in a single call.
 * 
 * @param[in]  temp_c     Ambient dry-bulb temperature (°C).
 * @param[in]  rh_pct     Relative humidity (%).
 * @param[out] p_state    Pointer to destination psychrometric_state_t struct.
 * @return status_t       STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_psychrometric_state(float temp_c, float rh_pct, psychrometric_state_t *p_state);

#ifdef __cplusplus
}
#endif

#endif /* DEW_POINT_H */
