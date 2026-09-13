/**
 * @file    i2c_bus.h
 * @brief   Non-blocking bounded I2C bus driver with 9-clock lockup recovery for STM32WLE5.
 * @details Layer 2 Hardware Abstraction wrapper for Bosch BME280, TI OPT3001, and I2C sensors.
 *          Provides bounded millisecond timeouts, 9-clock SCL bus recovery, and big-endian word helpers.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Standard I2C Bus Constants
 * ============================================================================ */

#define I2C_BUS_SPEED_STANDARD_HZ       100000UL    /**< 100 kHz Standard Mode */
#define I2C_BUS_SPEED_FAST_HZ           400000UL    /**< 400 kHz Fast Mode */

#define I2C_BUS_DEFAULT_TIMEOUT_MS      25U         /**< Default transaction timeout in ms */
#define I2C_BUS_PROBE_TIMEOUT_MS        50U         /**< Slave readiness probe timeout in ms */
#define I2C_BUS_DEFAULT_PROBE_TRIALS    3U          /**< Default probe retry count */
#define I2C_BUS_MAX_RECOVERY_CLOCKS     9U          /**< Maximum recovery pulses */

/* ============================================================================
 * Public Driver API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes the hardware I2C1 bus peripheral.
 * @param[in] speed_hz Bus clock frequency (I2C_BUS_SPEED_STANDARD_HZ or I2C_BUS_SPEED_FAST_HZ).
 * @return status_t    STATUS_OK on success, or STATUS_ERR_I2C / STATUS_ERR_INVALID_PARAM.
 */
status_t i2c_bus_init(uint32_t speed_hz);

/**
 * @brief  De-initializes the I2C1 bus peripheral prior to low-power Stop 2 sleep.
 * @return status_t STATUS_OK on success.
 */
status_t i2c_bus_deinit(void);

/**
 * @brief  Reads a contiguous multi-byte sequence from a target slave device register.
 * @param[in]  dev_addr   7-bit I2C slave device address (e.g., 0x76 or 0x44).
 * @param[in]  reg_addr   8-bit starting register address.
 * @param[out] p_data     Pointer to destination byte buffer.
 * @param[in]  length     Number of bytes to read.
 * @param[in]  timeout_ms Maximum transaction timeout in milliseconds.
 * @return status_t       STATUS_OK on success, STATUS_ERR_NULL_PTR if buffer is NULL,
 *                        STATUS_ERR_TIMEOUT on timeout, STATUS_ERR_SENSOR_NO_RESPONSE on NACK.
 */
status_t i2c_bus_read(uint8_t dev_addr,
                      uint8_t reg_addr,
                      uint8_t *p_data,
                      uint16_t length,
                      uint32_t timeout_ms);

/**
 * @brief  Writes a contiguous multi-byte sequence to a target slave device register.
 * @param[in] dev_addr   7-bit I2C slave device address.
 * @param[in] reg_addr   8-bit starting register address.
 * @param[in] p_data     Pointer to data buffer to transmit.
 * @param[in] length     Number of bytes to write.
 * @param[in] timeout_ms Maximum transaction timeout in milliseconds.
 * @return status_t      STATUS_OK on success, STATUS_ERR_NULL_PTR if buffer is NULL,
 *                       STATUS_ERR_TIMEOUT on timeout, STATUS_ERR_SENSOR_NO_RESPONSE on NACK.
 */
status_t i2c_bus_write(uint8_t dev_addr,
                       uint8_t reg_addr,
                       const uint8_t *p_data,
                       uint16_t length,
                       uint32_t timeout_ms);

/**
 * @brief  Reads a 16-bit big-endian register word from target device.
 * @details Reads 2 bytes (MSB first) from target register and converts to host endianness.
 * @param[in]  dev_addr   7-bit I2C slave device address.
 * @param[in]  reg_addr   8-bit register address.
 * @param[out] p_value    Pointer to store 16-bit word (host endianness).
 * @param[in]  timeout_ms Maximum transaction timeout in milliseconds.
 * @return status_t       STATUS_OK on success, error code on failure.
 */
status_t i2c_bus_read16(uint8_t dev_addr,
                        uint8_t reg_addr,
                        uint16_t *p_value,
                        uint32_t timeout_ms);

/**
 * @brief  Writes a 16-bit word to target device in big-endian byte order.
 * @details Transmits MSB followed by LSB to target register.
 * @param[in] dev_addr   7-bit I2C slave device address.
 * @param[in] reg_addr   8-bit register address.
 * @param[in] value      16-bit word to transmit.
 * @param[in] timeout_ms Maximum transaction timeout in milliseconds.
 * @return status_t      STATUS_OK on success, error code on failure.
 */
status_t i2c_bus_write16(uint8_t dev_addr,
                         uint8_t reg_addr,
                         uint16_t value,
                         uint32_t timeout_ms);

/**
 * @brief  Probes target device address to verify ACK readiness.
 * @param[in] dev_addr   7-bit I2C slave address.
 * @param[in] trials     Number of probe attempts.
 * @param[in] timeout_ms Timeout per probe trial in ms.
 * @return status_t      STATUS_OK if device responds with ACK, error otherwise.
 */
status_t i2c_bus_is_device_ready(uint8_t dev_addr, uint32_t trials, uint32_t timeout_ms);

/**
 * @brief  Executes a 9-clock SCL pulse cycling sequence to clear stuck I2C slave devices.
 * @details Configures SCL/SDA as Open-Drain GPIOs, pulses SCL up to 9 times while SDA is LOW,
 *          emits an explicit STOP condition, and re-initializes I2C1 peripheral.
 * @return status_t STATUS_OK if bus was successfully recovered.
 */
status_t i2c_bus_recover(void);

/**
 * @brief  Checks whether the I2C peripheral is actively performing a transfer.
 * @return bool true if bus is busy.
 */
bool i2c_bus_is_busy(void);

/* ============================================================================
 * Host Unit Testing & Simulation Inspection Hooks
 * ============================================================================ */

#if !defined(HAVE_STM32WLXX_HAL)

/**
 * @brief Resets simulated I2C driver state and register bank.
 */
void i2c_bus_test_reset(void);

/**
 * @brief Sets simulated SDA stuck state (simulating a hung slave holding SDA LOW).
 * @param[in] stuck_low true to simulate stuck LOW SDA line.
 */
void i2c_bus_test_set_sda_stuck(bool stuck_low);

/**
 * @brief Returns current simulated SDA stuck state.
 * @return bool true if SDA is stuck LOW.
 */
bool i2c_bus_test_get_sda_stuck(void);

/**
 * @brief Returns the number of recovery clock pulses generated during the last recover call.
 * @return uint32_t Number of SCL clock pulses (0 to 9).
 */
uint32_t i2c_bus_test_get_recovery_pulse_count(void);

/**
 * @brief Returns whether a STOP condition was emitted during the last recovery sequence.
 * @return bool true if STOP condition was generated.
 */
bool i2c_bus_test_get_stop_condition_emitted(void);

/**
 * @brief Returns currently configured bus clock speed in Hz.
 * @return uint32_t Speed in Hz.
 */
uint32_t i2c_bus_test_get_speed_hz(void);

/**
 * @brief Returns whether driver reports initialized state.
 * @return bool true if initialized.
 */
bool i2c_bus_test_is_initialized(void);

/**
 * @brief Injects a simulated fault on subsequent operations.
 * @param[in] fault Status code to return (STATUS_OK for none).
 */
void i2c_bus_test_inject_fault(status_t fault);

/**
 * @brief Configures a simulated slave register in test mode.
 * @param[in] dev_addr 7-bit device address.
 * @param[in] reg_addr 8-bit register address.
 * @param[in] value    Byte value to store.
 */
void i2c_bus_test_set_slave_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t value);

/**
 * @brief Reads a simulated slave register in test mode.
 * @param[in] dev_addr 7-bit device address.
 * @param[in] reg_addr 8-bit register address.
 * @return uint8_t Current byte value.
 */
uint8_t i2c_bus_test_get_slave_reg(uint8_t dev_addr, uint8_t reg_addr);

#endif /* !HAVE_STM32WLXX_HAL */

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
