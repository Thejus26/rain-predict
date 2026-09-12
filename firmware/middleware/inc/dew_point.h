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
 * @brief Hypsometric barometric reduction constants.
 */
#define HYPSO_LAPSE_RATE            (0.0065f)      /**< Standard tropospheric lapse rate (K/m) */
#define HYPSO_EXPONENT              (-5.257f)      /**< Hypsometric exponent -g*M/(R*Γ) */
#define HYPSO_EXPONENT_INV          (0.190222f)    /**< 1.0 / 5.257 for inverse altitude */
#define HYPSO_ALTITUDE_MIN_M        (-100.0f)      /**< Minimum valid station elevation (m) */
#define HYPSO_ALTITUDE_MAX_M        (5000.0f)      /**< Maximum valid station elevation (m) */
#define HYPSO_PRESSURE_MIN_HPA      (300.0f)       /**< Minimum physical station pressure (hPa) */
#define HYPSO_PRESSURE_MAX_HPA      (1100.0f)      /**< Maximum physical station pressure (hPa) */
#define HYPSO_DENOM_MIN_KELVIN      (200.0f)       /**< Minimum virtual denominator temperature (K) */

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
 
/**
 * @brief Computes sea-level equivalent barometric pressure (P0) from station pressure.
 * 
 * @param[in]  station_p_hpa Local measured barometric pressure at sensor mast (hPa).
 * @param[in]  temp_c        Local measured ambient temperature (°C).
 * @param[in]  altitude_m    Station elevation above Mean Sea Level (meters).
 * @param[out] p_p0_hpa      Pointer to store calculated sea-level pressure (hPa).
 * @return status_t          STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_sea_level_pressure(float station_p_hpa, float temp_c, float altitude_m, float *p_p0_hpa);

/**
 * @brief Computes estimated station pressure from known sea-level pressure P0.
 * 
 * @param[in]  p0_hpa        Reference sea-level barometric pressure (hPa).
 * @param[in]  temp_c        Local ambient temperature (°C).
 * @param[in]  altitude_m    Station elevation above Mean Sea Level (meters).
 * @param[out] p_station_p   Pointer to store calculated station pressure (hPa).
 * @return status_t          STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_station_pressure_from_p0(float p0_hpa, float temp_c, float altitude_m, float *p_station_p);

/**
 * @brief Computes estimated pressure altitude from station pressure and reference P0.
 * 
 * @param[in]  station_p_hpa Local measured barometric pressure (hPa).
 * @param[in]  p0_hpa        Sea-level reference pressure (e.g. 1013.25 hPa).
 * @param[in]  temp_c        Local ambient temperature (°C).
 * @param[out] p_altitude_m  Pointer to store calculated altitude (meters).
 * @return status_t          STATUS_OK on success, error code otherwise.
 */
status_t dew_point_calc_pressure_altitude(float station_p_hpa, float p0_hpa, float temp_c, float *p_altitude_m);

#ifdef __cplusplus
}
#endif

#endif /* DEW_POINT_H */
