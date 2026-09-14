/**
 * @file    bme280_driver.h
 * @brief   Bosch BME280 environmental sensor driver header for STM32WLE5 SoC.
 * @details Handles I2C communication, trimming calibration readout, forced-mode sampling,
 *          and Cortex-M4 single-precision floating-point / fixed-point compensation.
 *          Adheres to C99 standards, MISRA-C guidelines, and zero-dynamic-memory allocation.
 */

#ifndef BME280_DRIVER_H
#define BME280_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"

/* ========================================================================== */
/* Hardware & Register Definitions                                            */
/* ========================================================================== */

/** @brief Default BME280 primary I2C 7-bit address (SDO tied to GND) */
#define BME280_I2C_ADDR_PRIMARY         0x76U

/** @brief Secondary BME280 I2C 7-bit address (SDO tied to VDD) */
#define BME280_I2C_ADDR_SECONDARY       0x77U

/** @brief Expected Chip ID returned from register 0xD0 */
#define BME280_CHIP_ID                  0x60U

/** @brief Register address definitions */
#define BME280_REG_CALIB_00_25          0x88U   /**< Start of calib block 1 (0x88..0xA1) */
#define BME280_REG_CHIP_ID              0xD0U   /**< Chip identification register */
#define BME280_REG_RESET                0xE0U   /**< Soft reset register (0xB6 triggers reset) */
#define BME280_REG_CALIB_26_41          0xE1U   /**< Start of calib block 2 (0xE1..0xE7) */
#define BME280_REG_CTRL_HUM             0xF2U   /**< Humidity oversampling control */
#define BME280_REG_STATUS               0xF3U   /**< Device status register */
#define BME280_REG_CTRL_MEAS            0xF4U   /**< Pressure/Temp oversampling and mode */
#define BME280_REG_CONFIG               0xF5U   /**< Standby time, IIR filter, SPI 3-wire */
#define BME280_REG_PRESS_MSB            0xF7U   /**< Start of burst data readout (0xF7..0xFE) */

/** @brief Status register bitmasks (0xF3) */
#define BME280_REG_STATUS_MEASURING_BIT (1U << 3)   /**< Bit 3: 1 when conversion is running */
#define BME280_REG_STATUS_IM_UPDATE_BIT (1U << 0)   /**< Bit 0: 1 when NVM is copying to image */

/** @brief Buffer lengths for burst transactions */
#define BME280_CALIB_BLOCK1_LEN         26U     /**< 0x88 to 0xA1 inclusive */
#define BME280_CALIB_BLOCK2_LEN         7U      /**< 0xE1 to 0xE7 inclusive */
#define BME280_RAW_BURST_DATA_LEN       8U      /**< 0xF7 to 0xFE inclusive (Press, Temp, Hum) */

/** @brief Maximum I2C transaction timeout in milliseconds */
#define BME280_I2C_TIMEOUT_MS           50U

/** @brief Maximum hardware conversion duration guard in milliseconds */
#define BME280_MEASUREMENT_TIMEOUT_MS   60U

/* ========================================================================== */
/* Physical Boundary Constants                                                */
/* ========================================================================== */

#define BME280_PRESS_MIN_HPA            300.0f      /**< Minimum valid terrestrial pressure (hPa) */
#define BME280_PRESS_MAX_HPA            1100.0f     /**< Maximum valid terrestrial pressure (hPa) */
#define BME280_TEMP_MIN_C               -40.0f      /**< Minimum valid operating temperature (°C) */
#define BME280_TEMP_MAX_C               85.0f       /**< Maximum valid operating temperature (°C) */
#define BME280_HUM_MIN_PERCENT          0.0f        /**< Minimum relative humidity (%RH) */
#define BME280_HUM_MAX_PERCENT          100.0f      /**< Maximum relative humidity (%RH) */

/* ========================================================================== */
/* Saturation & Condensation Recovery Constants                               */
/* ========================================================================== */

#define BME280_SATURATION_THRESHOLD_RH      98.0f   /**< High humidity saturation entry threshold (%RH) */
#define BME280_SATURATION_HYSTERESIS_RH     95.0f   /**< High humidity saturation exit hysteresis threshold (%RH) */
#define BME280_SATURATION_MIN_CYCLES_1H     6U      /**< Minimum consecutive cycles for 1h assertion (6 @ 10m) */
#define BME280_SATURATION_MAX_CYCLES_24H    144U    /**< Maximum consecutive cycles before 24h reset (144 @ 10m) */

#define BME280_CONDENSATION_MIN_LUX         10000U  /**< Sunlight irradiance threshold for condensation creep (lux) */
#define BME280_CONDENSATION_MIN_TEMP_RISE   1.5f    /**< Rapid warming rate threshold for creep (°C/hour) */
#define BME280_CONDENSATION_DEBIAS_OFFSET   1.5f    /**< De-biasing correction offset applied during creep (%RH) */

#define BME280_SOFT_RESET_KEY               0xB6U   /**< Soft reset command key written to register 0xE0 */
#define BME280_SOFT_RESET_SETTLE_MS         5U      /**< Settling delay in milliseconds after soft reset */

/* ========================================================================== */
/* Data Types                                                                 */
/* ========================================================================== */

/**
 * @brief BME280 oversampling settings.
 */
typedef enum {
    BME280_OVERSAMPLING_SKIPPED = 0x00U,
    BME280_OVERSAMPLING_1X      = 0x01U,
    BME280_OVERSAMPLING_2X      = 0x02U,
    BME280_OVERSAMPLING_4X      = 0x03U,
    BME280_OVERSAMPLING_8X      = 0x04U,
    BME280_OVERSAMPLING_16X     = 0x05U
} bme280_oversampling_t;

/**
 * @brief BME280 IIR filter coefficient settings.
 */
typedef enum {
    BME280_FILTER_OFF       = 0x00U,
    BME280_FILTER_COEFF_2   = 0x01U,
    BME280_FILTER_COEFF_4   = 0x02U,
    BME280_FILTER_COEFF_8   = 0x03U,
    BME280_FILTER_COEFF_16  = 0x04U
} bme280_filter_t;

/**
 * @brief BME280 operating mode.
 */
typedef enum {
    BME280_MODE_SLEEP   = 0x00U,
    BME280_MODE_FORCED  = 0x01U,
    BME280_MODE_NORMAL  = 0x03U
} bme280_mode_t;

/**
 * @brief Consolidated sensor configuration structure.
 */
typedef struct {
    bme280_oversampling_t osrs_t;   /**< Temperature oversampling (Default: 2X) */
    bme280_oversampling_t osrs_p;   /**< Pressure oversampling (Default: 16X) */
    bme280_oversampling_t osrs_h;   /**< Humidity oversampling (Default: 1X) */
    bme280_filter_t       filter;   /**< IIR filter coefficient (Default: COEFF_4) */
} bme280_config_t;

/**
 * @brief Raw ADC 20-bit/16-bit uncompensated sensor readout.
 */
typedef struct {
    int32_t adc_T;      /**< 20-bit uncompensated raw temperature */
    int32_t adc_P;      /**< 20-bit uncompensated raw pressure */
    int32_t adc_H;      /**< 16-bit uncompensated raw humidity */
} bme280_raw_data_t;

/**
 * @brief Floating-point compensated physical sensor readings.
 */
typedef struct {
    float   temperature_c;      /**< Ambient temperature in degrees Celsius (°C) */
    float   pressure_hpa;       /**< Barometric pressure in hectopascals (hPa) */
    float   humidity_percent;   /**< Relative humidity in %RH (0.0 to 100.0) */
    bool    is_valid;           /**< True if readings passed all boundary validations */
} bme280_data_t;

/**
 * @brief Fixed-point scaled integer compensated sensor readings for compact telemetry.
 */
typedef struct {
    int16_t     temp_centi_c;       /**< Scaled temperature: 0.01 °C (e.g. 2452 = 24.52 °C) */
    uint32_t    press_pascals;      /**< Absolute pressure: Pascals (e.g. 101325 Pa) */
    uint16_t    hum_centi_percent;  /**< Scaled humidity: 0.01 %RH (e.g. 8540 = 85.40%) */
    bool        is_valid;           /**< True if valid */
} bme280_fixed_data_t;

/**
 * @brief BME280 operational humidity saturation and condensation state.
 */
typedef enum {
    BME280_STATE_NORMAL             = 0,    /**< Normal humidity (< 98.0% RH) */
    BME280_STATE_HIGH_HUMIDITY_SAT  = 1,    /**< Atmospheric saturation / fog / rain (>= 98.0% RH) */
    BME280_STATE_CONDENSATION_CREEP = 2,    /**< Surface condensation mismatch (>= 98% under bright sun) */
    BME280_STATE_SOFT_RECOVERY      = 3     /**< Automated NVM reload / soft reset executed */
} bme280_saturation_state_t;

/**
 * @brief Diagnostic status flags for BME280 environmental integrity and recovery.
 */
typedef struct {
    bool                        is_saturated;           /**< True if RH >= 98.0% for >= 1 hour */
    bool                        condensation_detected;  /**< True if drying condensation creep detected */
    bool                        recovery_triggered;     /**< True if soft reset was performed this cycle */
    uint16_t                    saturation_cycles;      /**< Consecutive cycles with RH >= 98.0% */
    bme280_saturation_state_t   state;                  /**< Current operational saturation state */
    float                       debiased_humidity_pct;  /**< Corrected humidity (%RH) after de-biasing */
} bme280_saturation_status_t;

/**
 * @brief Bosch BME280 factory calibration trimming coefficients structure.
 */
typedef struct {
    uint16_t dig_T1;    /**< Temperature coefficient T1 (unsigned 16-bit) */
    int16_t  dig_T2;    /**< Temperature coefficient T2 (signed 16-bit) */
    int16_t  dig_T3;    /**< Temperature coefficient T3 (signed 16-bit) */
    uint16_t dig_P1;    /**< Pressure coefficient P1 (unsigned 16-bit) */
    int16_t  dig_P2;    /**< Pressure coefficient P2 (signed 16-bit) */
    int16_t  dig_P3;    /**< Pressure coefficient P3 (signed 16-bit) */
    int16_t  dig_P4;    /**< Pressure coefficient P4 (signed 16-bit) */
    int16_t  dig_P5;    /**< Pressure coefficient P5 (signed 16-bit) */
    int16_t  dig_P6;    /**< Pressure coefficient P6 (signed 16-bit) */
    int16_t  dig_P7;    /**< Pressure coefficient P7 (signed 16-bit) */
    int16_t  dig_P8;    /**< Pressure coefficient P8 (signed 16-bit) */
    int16_t  dig_P9;    /**< Pressure coefficient P9 (signed 16-bit) */
    uint8_t  dig_H1;    /**< Humidity coefficient H1 (unsigned 8-bit) */
    int16_t  dig_H2;    /**< Humidity coefficient H2 (signed 16-bit) */
    uint8_t  dig_H3;    /**< Humidity coefficient H3 (unsigned 8-bit) */
    int16_t  dig_H4;    /**< Humidity coefficient H4 (signed 12-bit) */
    int16_t  dig_H5;    /**< Humidity coefficient H5 (signed 12-bit) */
    int8_t   dig_H6;    /**< Humidity coefficient H6 (signed 8-bit) */
} bme280_calib_data_t;

/**
 * @brief BME280 device state handle.
 */
typedef struct {
    uint8_t             i2c_address;        /**< I2C device address (0x76 or 0x77) */
    uint8_t             chip_id;            /**< Detected hardware chip ID */
    bme280_config_t     config;             /**< Currently applied configuration */
    bme280_calib_data_t calib;              /**< Cached factory calibration data */
    bool                is_initialized;     /**< True if initialized and ready */
} bme280_dev_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the BME280 device handle, verifies Chip ID, and loads calibration.
 * @param[in,out] dev Pointer to BME280 device structure.
 * @param[in]     i2c_addr I2C 7-bit address (BME280_I2C_ADDR_PRIMARY or _SECONDARY).
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_init(bme280_dev_t *dev, uint8_t i2c_addr);

/**
 * @brief  Reads the BME280 Chip ID register (0xD0).
 * @param[in]  dev Pointer to BME280 device structure.
 * @param[out] p_chip_id Pointer to store the read Chip ID value.
 * @return STATUS_OK on success, STATUS_ERR_HARDWARE on mismatch, or bus error.
 */
status_t bme280_read_chip_id(bme280_dev_t *dev, uint8_t *p_chip_id);

/**
 * @brief  Reads and unpacks all 26 trimming calibration coefficients from sensor NVM.
 * @param[in,out] dev Pointer to BME280 device structure with calib member populated.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_read_calibration(bme280_dev_t *dev);

/**
 * @brief  Validates that calibration coefficients are within plausible physical bounds.
 * @param[in] calib Pointer to calibration data structure.
 * @return STATUS_OK if valid, or STATUS_ERR_DATA_CORRUPT if all-zero or corrupt.
 */
status_t bme280_validate_calibration(const bme280_calib_data_t *calib);

/**
 * @brief  Returns a const pointer to the device's cached calibration structure.
 * @param[in] dev Pointer to BME280 device structure.
 * @return Const pointer to bme280_calib_data_t, or NULL if dev is invalid or uninitialized.
 */
const bme280_calib_data_t* bme280_get_calibration(const bme280_dev_t *dev);

/**
 * @brief  Applies meteorological configuration profile (oversampling and IIR filter) to sensor.
 * @param[in,out] dev Pointer to BME280 device structure.
 * @param[in]     config Pointer to configuration settings structure.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_configure(bme280_dev_t *dev, const bme280_config_t *config);

/**
 * @brief  Arms and triggers a single-shot forced mode measurement.
 * @param[in] dev Pointer to BME280 device structure.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_trigger_forced_mode(bme280_dev_t *dev);

/**
 * @brief  Polls status register to check if ADC conversion is running.
 * @param[in]  dev Pointer to BME280 device structure.
 * @param[out] p_is_measuring Set to true if measuring bit is 1.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_is_measuring(bme280_dev_t *dev, bool *p_is_measuring);

/**
 * @brief  Blocks with bounded polling until measurement completes.
 * @param[in] dev Pointer to BME280 device structure.
 * @param[in] timeout_ms Maximum timeout in milliseconds (e.g. 60 ms).
 * @return STATUS_OK if complete, STATUS_ERR_TIMEOUT if elapsed.
 */
status_t bme280_wait_for_completion(bme280_dev_t *dev, uint32_t timeout_ms);

/**
 * @brief  Burst reads 8 registers (0xF7..0xFE) and unpacks 20-bit/16-bit raw ADC words.
 * @param[in]  dev Pointer to BME280 device structure.
 * @param[out] p_raw Pointer to raw data structure.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_read_raw_data(bme280_dev_t *dev, bme280_raw_data_t *p_raw);

/**
 * @brief  High-level convenience routine: triggers forced mode, waits, and reads raw ADC.
 * @param[in,out] dev Pointer to BME280 device structure.
 * @param[out]    p_raw Pointer to raw data structure.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_sample_forced_raw(bme280_dev_t *dev, bme280_raw_data_t *p_raw);

/**
 * @brief  Compensates raw temperature ADC value into degrees Celsius using single-precision FPU math.
 * @param[in]  adc_T Raw 20-bit temperature ADC reading.
 * @param[in]  calib Pointer to calibration data structure.
 * @param[out] p_t_fine Pointer to store internal high-resolution temperature.
 * @return Temperature in degrees Celsius.
 */
float bme280_compensate_temperature(int32_t adc_T, const bme280_calib_data_t *calib, float *p_t_fine);

/**
 * @brief  Compensates raw pressure ADC value into hectopascals (hPa) using single-precision FPU math.
 * @param[in] adc_P Raw 20-bit pressure ADC reading.
 * @param[in] calib Pointer to calibration data structure.
 * @param[in] t_fine High-resolution temperature from bme280_compensate_temperature().
 * @return Barometric pressure in hPa with zero-division protection and range clamping.
 */
float bme280_compensate_pressure(int32_t adc_P, const bme280_calib_data_t *calib, float t_fine);

/**
 * @brief  Compensates raw humidity ADC value into relative humidity (%RH) using single-precision FPU math.
 * @param[in] adc_H Raw 16-bit humidity ADC reading.
 * @param[in] calib Pointer to calibration data structure.
 * @param[in] t_fine High-resolution temperature from bme280_compensate_temperature().
 * @return Relative humidity in %RH clamped between 0.0% and 100.0%.
 */
float bme280_compensate_humidity(int32_t adc_H, const bme280_calib_data_t *calib, float t_fine);

/**
 * @brief  Executes full single-precision floating-point compensation for temperature, pressure, and humidity.
 * @param[in]  raw Pointer to raw ADC values.
 * @param[in]  calib Pointer to calibration structure.
 * @param[out] out_data Pointer to output structure in physical engineering units.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_compensate_raw(const bme280_raw_data_t *raw,
                               const bme280_calib_data_t *calib,
                               bme280_data_t *out_data);

/**
 * @brief  Executes fixed-point integer compensation for compact LoRaWAN serialization.
 * @param[in]  raw Pointer to raw ADC values.
 * @param[in]  calib Pointer to calibration structure.
 * @param[out] out_data Pointer to fixed-point output structure.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_compensate_raw_fixed(const bme280_raw_data_t *raw,
                                     const bme280_calib_data_t *calib,
                                     bme280_fixed_data_t *out_data);

/**
 * @brief  Master sampling routine: triggers forced mode, waits, reads ADC, and executes float compensation.
 * @param[in,out] dev Pointer to initialized BME280 device context.
 * @param[out]    out_data Pointer to store compensated environmental readings.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_read_data(bme280_dev_t *dev, bme280_data_t *out_data);

/**
 * @brief  Executes a hardware soft-reset via register 0xE0, delays 5ms, and reloads calibration NVM.
 * @param[in,out] dev Pointer to BME280 device context.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_soft_reset(bme280_dev_t *dev);

/**
 * @brief  Evaluates saturation tracking, condensation creep, and automated recovery.
 * @param[in,out] dev Pointer to BME280 device context.
 * @param[in]     current_rh Compensated relative humidity from bme280_compensate_humidity().
 * @param[in]     temp_delta_1h 1-hour temperature trend in °C/hr (from ring buffer / trend detector).
 * @param[in]     ambient_lux Ambient optical illuminance from OPT3001 sensor (lux).
 * @param[in]     rain_tips_24h Total rain gauge tips logged over past 24 hours.
 * @return STATUS_OK on success, or error status code.
 */
status_t bme280_process_saturation(bme280_dev_t *dev,
                                   float current_rh,
                                   float temp_delta_1h,
                                   uint16_t ambient_lux,
                                   uint32_t rain_tips_24h);

/**
 * @brief  Returns a const pointer to the device's cached saturation diagnostic status structure.
 * @param[in] dev Pointer to BME280 device context.
 * @return Const pointer to bme280_saturation_status_t, or NULL if dev is NULL.
 */
const bme280_saturation_status_t* bme280_get_saturation_status(const bme280_dev_t *dev);

/**
 * @brief  Clears the saturation tracking counters and resets state to BME280_STATE_NORMAL.
 * @param[in,out] dev Pointer to BME280 device context.
 */
void bme280_reset_saturation_tracking(bme280_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BME280_DRIVER_H */
