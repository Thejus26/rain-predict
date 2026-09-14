/**
 * @file    bme280_driver.h
 * @brief   Bosch BME280 environmental sensor driver header for STM32WLE5 SoC.
 * @details Handles I2C communication, trimming calibration readout, and forced-mode sampling.
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

/** @brief Buffer lengths for calibration burst transactions */
#define BME280_CALIB_BLOCK1_LEN         26U     /**< 0x88 to 0xA1 inclusive */
#define BME280_CALIB_BLOCK2_LEN         7U      /**< 0xE1 to 0xE7 inclusive */

/** @brief Maximum I2C transaction timeout in milliseconds */
#define BME280_I2C_TIMEOUT_MS           50U

/* ========================================================================== */
/* Data Types                                                                 */
/* ========================================================================== */

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

#ifdef __cplusplus
}
#endif

#endif /* BME280_DRIVER_H */
