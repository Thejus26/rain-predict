/**
 * @file    bsp_adc.h
 * @brief   Low-power ADC driver for battery voltage and supply telemetry on STM32WLE5 SoC.
 * @details Conforms to MISRA-C and zero-dynamic-allocation embedded standards.
 *          Features internal VREFINT factory calibration, gated resistor divider control,
 *          8x oversampling, and true VDDA supply compensation.
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Hardware Configuration Constants & Calibration Addresses
 * ============================================================================ */

/** @brief Address of internal factory VREFINT calibration word (3.0V @ 30C) in system memory */
#define BSP_ADC_VREFINT_CAL_ADDR            ((const uint16_t *)0x1FFF75AAUL)

/** @brief Factory calibration reference voltage in millivolts (3000 mV) */
#define BSP_ADC_VREFINT_CAL_VOLTAGE_MV      3000U

/** @brief Resistor divider multiplication factor (100k / 100k -> 2.0x scaling) */
#define BSP_ADC_VBAT_DIVIDER_SCALE          2U

/** @brief Gated resistor divider RC settling guard delay in milliseconds */
#define BSP_ADC_DIVIDER_SETTLING_MS         2U

/** @brief Number of oversampled conversions to average per reading */
#define BSP_ADC_OVERSAMPLING_COUNT          8U

/** @brief Full scale 12-bit ADC value (2^12 - 1) */
#define BSP_ADC_FULL_SCALE_12BIT            4095U

/** @brief Default fallback VREFINT calibration count when memory is unprogrammed or corrupted */
#define BSP_ADC_DEFAULT_VREFINT_CAL         1660U

/* ============================================================================
 * Public Driver API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes the ADC1 peripheral, PB0 analog channel, and executes self-calibration.
 * @return status_t STATUS_OK on success, or STATUS_ERR_INVALID_STATE / hardware error.
 */
status_t bsp_adc_init(void);

/**
 * @brief  De-initializes the ADC1 peripheral to eliminate sleep current.
 * @return status_t STATUS_OK on success.
 */
status_t bsp_adc_deinit(void);

/**
 * @brief  Reads the ST factory calibration value for the internal reference voltage.
 * @return 16-bit calibration raw count from 0x1FFF75AA (bounded in [1000, 3000], else 1660).
 */
uint16_t bsp_adc_get_vrefint_factory_cal(void);

/**
 * @brief  Samples the internal VREFINT channel with 8x oversampled averaging.
 * @param[out] p_raw_vref Pointer to store the raw 12-bit ADC reading.
 * @return status_t STATUS_OK on success, STATUS_ERR_NULL_PTR if pointer is NULL, or TIMEOUT.
 */
status_t bsp_adc_read_vrefint_raw(uint16_t *p_raw_vref);

/**
 * @brief  Gates PB1, delays 2 ms for RC stabilization, samples ADC_IN1 (PB0, 8x averaged),
 *         and immediately de-asserts PB1.
 * @param[out] p_raw_vbat Pointer to store the raw 12-bit ADC reading.
 * @return status_t STATUS_OK on success, STATUS_ERR_NULL_PTR if pointer is NULL, or error.
 */
status_t bsp_adc_read_vbat_raw(uint16_t *p_raw_vbat);

/**
 * @brief  Samples VREFINT and VBAT, computes real VDDA, and calculates compensated battery millivolts.
 * @details Uses 64-bit integer fixed-point arithmetic with half-LSB rounding:
 *          Vbat_mV = floor((6000 * CAL_VAL * RAW_VBAT + (4095 * RAW_VREFINT / 2)) / (4095 * RAW_VREFINT))
 * @param[out] p_vbat_mv Pointer to store compensated battery voltage in millivolts.
 * @return status_t STATUS_OK on success, STATUS_ERR_NULL_PTR if pointer is NULL, or error code.
 */
status_t bsp_adc_read_vbat_mv(uint16_t *p_vbat_mv);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated ADC driver states and metrics for unit testing.
 */
void bsp_adc_test_reset(void);

/**
 * @brief Overrides the simulated factory calibration value.
 * @param[in] cal_val Calibration raw count to return from bsp_adc_get_vrefint_factory_cal().
 */
void bsp_adc_test_set_factory_cal(uint16_t cal_val);

/**
 * @brief Overrides the raw ADC count returned by bsp_adc_read_vrefint_raw().
 * @param[in] raw_vref 12-bit raw VREFINT count (0 to 4095).
 */
void bsp_adc_test_set_vrefint_raw(uint16_t raw_vref);

/**
 * @brief Overrides the raw ADC count returned by bsp_adc_read_vbat_raw().
 * @param[in] raw_vbat 12-bit raw VBAT count (0 to 4095).
 */
void bsp_adc_test_set_vbat_raw(uint16_t raw_vbat);

/**
 * @brief Injects an error status code for the next conversion call.
 * @param[in] error_code Status code to inject (e.g. STATUS_ERR_TIMEOUT).
 */
void bsp_adc_test_inject_error(status_t error_code);

/**
 * @brief Checks whether the ADC driver is currently initialized in simulation.
 * @return bool true if initialized.
 */
bool bsp_adc_test_is_initialized(void);

/**
 * @brief Gets the total number of ADC conversions performed during test execution.
 * @return uint32_t Conversion count.
 */
uint32_t bsp_adc_test_get_conversion_count(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
