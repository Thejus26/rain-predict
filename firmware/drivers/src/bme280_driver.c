/**
 * @file    bme280_driver.c
 * @brief   Bosch BME280 sensor driver implementation for STM32WLE5 SoC.
 * @details Implements 2-phase burst NVM calibration readout, hardware ID verification,
 *          little-endian and bit-split unpacking, and coefficient sanity validation.
 *          Adheres to C99 standards, MISRA-C guidelines, and zero-dynamic-memory allocation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
