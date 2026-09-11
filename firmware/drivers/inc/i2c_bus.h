/**
 * @file    i2c_bus.h
 * @brief   Hardware-independent I2C Master Bus Driver Interface.
 * @details Declares bounded, non-blocking I2C bus read/write operations for sensors.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "status.h"

/**
 * @brief Initializes the primary I2C bus hardware peripheral.
 *
 * @param[in] clock_speed_hz I2C bus clock frequency in Hz (e.g. 100000 or 400000).
 * @return STATUS_OK on success, or STATUS_ERR_I2C on hardware failure.
 */
status_t i2c_bus_init(uint32_t clock_speed_hz);

/**
 * @brief Performs a bounded synchronous register read over the I2C bus.
 *
 * @param[in]  dev_addr   7-bit I2C target slave address.
 * @param[in]  reg_addr   Target internal register address.
 * @param[out] p_data     Pointer to destination memory buffer.
 * @param[in]  length     Number of contiguous bytes to read.
 * @param[in]  timeout_ms Maximum allowable transaction duration before aborting.
 * @return STATUS_OK on success, STATUS_ERR_NULL_PTR if buffer is NULL,
 *         STATUS_ERR_SENSOR_NO_RESPONSE on address NACK, or STATUS_ERR_TIMEOUT.
 */
status_t i2c_bus_read(uint8_t dev_addr,
                      uint8_t reg_addr,
                      uint8_t *p_data,
                      uint16_t length,
                      uint32_t timeout_ms);

/**
 * @brief Performs a bounded synchronous register write over the I2C bus.
 *
 * @param[in] dev_addr   7-bit I2C target slave address.
 * @param[in] reg_addr   Target internal register address.
 * @param[in] p_data     Pointer to source data buffer to transmit.
 * @param[in] length     Number of contiguous bytes to write.
 * @param[in] timeout_ms Maximum allowable transaction duration before aborting.
 * @return STATUS_OK on success, STATUS_ERR_NULL_PTR if buffer is NULL,
 *         STATUS_ERR_SENSOR_NO_RESPONSE on address NACK, or STATUS_ERR_TIMEOUT.
 */
status_t i2c_bus_write(uint8_t dev_addr,
                       uint8_t reg_addr,
                       const uint8_t *p_data,
                       uint16_t length,
                       uint32_t timeout_ms);

/**
 * @brief Checks whether a target slave device responds to its 7-bit address.
 *
 * @param[in] dev_addr   7-bit I2C target slave address.
 * @param[in] timeout_ms Maximum allowable probe duration.
 * @return STATUS_OK if device ACKed, STATUS_ERR_SENSOR_NO_RESPONSE if NACKed or missing.
 */
status_t i2c_bus_is_device_ready(uint8_t dev_addr, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
