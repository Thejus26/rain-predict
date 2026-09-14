/**
 * @file    bme280_driver.c
 * @brief   Bosch BME280 sensor driver implementation for STM32WLE5 SoC.
 * @details Implements 2-phase burst NVM calibration readout, hardware ID verification,
 *          forced-mode single-shot trigger, bounded conversion completion polling,
 *          atomic 8-byte raw ADC readout, and coefficient sanity validation.
 *          Adheres to C99 standards, MISRA-C guidelines, and zero-dynamic-memory allocation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)
#include "stm32wlxx_hal.h"
#endif

#include "i2c_bus.h"
#include "bme280_driver.h"

/* ========================================================================== */
/* Public Driver API Implementation                                           */
/* ========================================================================== */

status_t bme280_read_chip_id(bme280_dev_t *dev, uint8_t *p_chip_id) {
    if (dev == NULL || p_chip_id == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t chip_id = 0;
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_CHIP_ID,
                                   &chip_id,
                                   1U,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    *p_chip_id = chip_id;
    dev->chip_id = chip_id;

    if (chip_id != BME280_CHIP_ID) {
        return STATUS_ERR_HARDWARE;
    }

    return STATUS_OK;
}

status_t bme280_validate_calibration(const bme280_calib_data_t *calib) {
    if (calib == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* Check for uninitialized / floating bus patterns (all 0x00 or all 0xFF) */
    if (calib->dig_T1 == 0x0000U || calib->dig_T1 == 0xFFFFU ||
        calib->dig_P1 == 0x0000U || calib->dig_P1 == 0xFFFFU ||
        calib->dig_H1 == 0x00U   || calib->dig_H1 == 0xFFU   ||
        calib->dig_H3 == 0xFFU) {
        return STATUS_ERR_DATA_CORRUPT;
    }

    return STATUS_OK;
}

status_t bme280_read_calibration(bme280_dev_t *dev) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t block1[BME280_CALIB_BLOCK1_LEN];
    uint8_t block2[BME280_CALIB_BLOCK2_LEN];
    memset(block1, 0, sizeof(block1));
    memset(block2, 0, sizeof(block2));

    /* Phase 1: Burst read 26 bytes from 0x88 to 0xA1 */
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_CALIB_00_25,
                                   block1,
                                   BME280_CALIB_BLOCK1_LEN,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Phase 2: Burst read 7 bytes from 0xE1 to 0xE7 */
    status = i2c_bus_read(dev->i2c_address,
                          BME280_REG_CALIB_26_41,
                          block2,
                          BME280_CALIB_BLOCK2_LEN,
                          BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Unpack Temperature Trimming Parameters (Little-Endian) */
    dev->calib.dig_T1 = (uint16_t)(((uint16_t)block1[1] << 8) | (uint16_t)block1[0]);
    dev->calib.dig_T2 = (int16_t)(((int16_t)block1[3] << 8) | (int16_t)block1[2]);
    dev->calib.dig_T3 = (int16_t)(((int16_t)block1[5] << 8) | (int16_t)block1[4]);

    /* Unpack Pressure Trimming Parameters (Little-Endian) */
    dev->calib.dig_P1 = (uint16_t)(((uint16_t)block1[7] << 8) | (uint16_t)block1[6]);
    dev->calib.dig_P2 = (int16_t)(((int16_t)block1[9] << 8) | (int16_t)block1[8]);
    dev->calib.dig_P3 = (int16_t)(((int16_t)block1[11] << 8) | (int16_t)block1[10]);
    dev->calib.dig_P4 = (int16_t)(((int16_t)block1[13] << 8) | (int16_t)block1[12]);
    dev->calib.dig_P5 = (int16_t)(((int16_t)block1[15] << 8) | (int16_t)block1[14]);
    dev->calib.dig_P6 = (int16_t)(((int16_t)block1[17] << 8) | (int16_t)block1[16]);
    dev->calib.dig_P7 = (int16_t)(((int16_t)block1[19] << 8) | (int16_t)block1[18]);
    dev->calib.dig_P8 = (int16_t)(((int16_t)block1[21] << 8) | (int16_t)block1[20]);
    dev->calib.dig_P9 = (int16_t)(((int16_t)block1[23] << 8) | (int16_t)block1[22]);

    /* Unpack Humidity Trimming Parameters */
    dev->calib.dig_H1 = block1[25]; /* Register 0xA1 */
    dev->calib.dig_H2 = (int16_t)(((int16_t)block2[1] << 8) | (int16_t)block2[0]); /* 0xE1 / 0xE2 */
    dev->calib.dig_H3 = block2[2]; /* Register 0xE3 */

    /* Complex Bit-Split for dig_H4 and dig_H5:
     * dig_H4 = (int8_t)reg[0xE4] << 4 | (reg[0xE5] & 0x0F)
     * dig_H5 = (int8_t)reg[0xE6] << 4 | (reg[0xE5] >> 4)
     */
    dev->calib.dig_H4 = (int16_t)(((int16_t)(int8_t)block2[3] << 4) |
                                  (int16_t)(block2[4] & 0x0FU));

    dev->calib.dig_H5 = (int16_t)(((int16_t)(int8_t)block2[5] << 4) |
                                  (int16_t)((block2[4] >> 4) & 0x0FU));

    dev->calib.dig_H6 = (int8_t)block2[6]; /* Register 0xE7 */

    /* Validate parsed coefficients */
    return bme280_validate_calibration(&dev->calib);
}

status_t bme280_init(bme280_dev_t *dev, uint8_t i2c_addr) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (i2c_addr != BME280_I2C_ADDR_PRIMARY && i2c_addr != BME280_I2C_ADDR_SECONDARY) {
        return STATUS_ERR_INVALID_PARAM;
    }

    memset(dev, 0, sizeof(bme280_dev_t));
    dev->i2c_address = i2c_addr;

    /* Set default meteorological sampling configuration */
    dev->config.osrs_t = BME280_OVERSAMPLING_2X;
    dev->config.osrs_p = BME280_OVERSAMPLING_16X;
    dev->config.osrs_h = BME280_OVERSAMPLING_1X;
    dev->config.filter = BME280_FILTER_COEFF_4;

    /* 1. Verify Chip ID */
    uint8_t chip_id = 0;
    status_t status = bme280_read_chip_id(dev, &chip_id);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Read and unpack calibration NVM */
    status = bme280_read_calibration(dev);
    if (status != STATUS_OK) {
        return status;
    }

    dev->is_initialized = true;
    return STATUS_OK;
}

const bme280_calib_data_t* bme280_get_calibration(const bme280_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized) {
        return NULL;
    }
    return &dev->calib;
}

status_t bme280_configure(bme280_dev_t *dev, const bme280_config_t *config) {
    if (dev == NULL || config == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    dev->config = *config;

    /* 1. Write ctrl_hum (0xF2) -> osrs_h */
    uint8_t ctrl_hum = (uint8_t)(config->osrs_h & 0x07U);
    status_t status = i2c_bus_write(dev->i2c_address,
                                    BME280_REG_CTRL_HUM,
                                    &ctrl_hum,
                                    1U,
                                    BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Write config (0xF5) -> IIR Filter */
    uint8_t config_reg = (uint8_t)((config->filter & 0x07U) << 2);
    status = i2c_bus_write(dev->i2c_address,
                           BME280_REG_CONFIG,
                           &config_reg,
                           1U,
                           BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    return STATUS_OK;
}

status_t bme280_trigger_forced_mode(bme280_dev_t *dev) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* ctrl_meas: osrs_t[7:5] | osrs_p[4:2] | mode[1:0] */
    uint8_t ctrl_meas = (uint8_t)(((dev->config.osrs_t & 0x07U) << 5) |
                                  ((dev->config.osrs_p & 0x07U) << 2) |
                                  (BME280_MODE_FORCED & 0x03U));

    return i2c_bus_write(dev->i2c_address,
                         BME280_REG_CTRL_MEAS,
                         &ctrl_meas,
                         1U,
                         BME280_I2C_TIMEOUT_MS);
}

status_t bme280_is_measuring(bme280_dev_t *dev, bool *p_is_measuring) {
    if (dev == NULL || p_is_measuring == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t status_reg = 0;
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_STATUS,
                                   &status_reg,
                                   1U,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    *p_is_measuring = ((status_reg & BME280_REG_STATUS_MEASURING_BIT) != 0U);
    return STATUS_OK;
}

status_t bme280_wait_for_completion(bme280_dev_t *dev, uint32_t timeout_ms) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint32_t elapsed_ms = 0;
    const uint32_t poll_interval_ms = 5U;

#if defined(HAVE_STM32WLXX_HAL)
    /* Initial nominal sleep delay (20 ms) before polling */
    HAL_Delay(20);
    elapsed_ms += 20U;
#endif

    while (elapsed_ms <= timeout_ms) {
        bool measuring = true;
        status_t status = bme280_is_measuring(dev, &measuring);
        if (status != STATUS_OK) {
            return status;
        }

        if (!measuring) {
            return STATUS_OK; /* Measurement completed */
        }

#if defined(HAVE_STM32WLXX_HAL)
        HAL_Delay(poll_interval_ms);
#endif
        elapsed_ms += poll_interval_ms;
    }

    return STATUS_ERR_TIMEOUT;
}

status_t bme280_read_raw_data(bme280_dev_t *dev, bme280_raw_data_t *p_raw) {
    if (dev == NULL || p_raw == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t raw_buf[BME280_RAW_BURST_DATA_LEN];
    memset(raw_buf, 0, sizeof(raw_buf));

    /* Burst read 8 registers starting at 0xF7 (press_msb) */
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_PRESS_MSB,
                                   raw_buf,
                                   BME280_RAW_BURST_DATA_LEN,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Unpack 20-bit Pressure: raw_buf[0..2] */
    p_raw->adc_P = (int32_t)((((uint32_t)raw_buf[0]) << 12) |
                             (((uint32_t)raw_buf[1]) << 4)  |
                             (((uint32_t)raw_buf[2]) >> 4));

    /* Unpack 20-bit Temperature: raw_buf[3..5] */
    p_raw->adc_T = (int32_t)((((uint32_t)raw_buf[3]) << 12) |
                             (((uint32_t)raw_buf[4]) << 4)  |
                             (((uint32_t)raw_buf[5]) >> 4));

    /* Unpack 16-bit Humidity: raw_buf[6..7] */
    p_raw->adc_H = (int32_t)((((uint32_t)raw_buf[6]) << 8) |
                             ((uint32_t)raw_buf[7]));

    return STATUS_OK;
}

status_t bme280_sample_forced_raw(bme280_dev_t *dev, bme280_raw_data_t *p_raw) {
    if (dev == NULL || p_raw == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* 1. Apply oversampling and filter configuration */
    status_t status = bme280_configure(dev, &dev->config);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Trigger forced-mode single-shot measurement */
    status = bme280_trigger_forced_mode(dev);
    if (status != STATUS_OK) {
        return status;
    }

    /* 3. Wait for conversion to complete */
    status = bme280_wait_for_completion(dev, BME280_MEASUREMENT_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* 4. Burst-read raw ADC registers */
    return bme280_read_raw_data(dev, p_raw);
}
