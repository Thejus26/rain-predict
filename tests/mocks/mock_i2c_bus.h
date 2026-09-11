/**
 * @file    mock_i2c_bus.h
 * @brief   Host Mock I2C Bus Interface & Fault Injection Controller.
 * @details Provides register-level emulation for BME280, OPT3001, and I2C sensors.
 */

#ifndef MOCK_I2C_BUS_H
#define MOCK_I2C_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "i2c_bus.h"

/* Maximum simulated I2C devices and register address space */
#define MOCK_I2C_MAX_DEVICES        4
#define MOCK_I2C_REGISTER_SPACE     256

/**
 * @brief Simulated hardware fault types.
 */
typedef enum {
    MOCK_I2C_FAULT_NONE = 0,
    MOCK_I2C_FAULT_NACK_ADDR,
    MOCK_I2C_FAULT_NACK_DATA,
    MOCK_I2C_FAULT_TIMEOUT,
    MOCK_I2C_FAULT_BUS_ERROR,
    MOCK_I2C_FAULT_CORRUPT_DATA
} mock_i2c_fault_t;

/* --- Mock Control API for Unit Tests --- */

void mock_i2c_init(void);
void mock_i2c_reset(void);

status_t mock_i2c_set_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t value);
status_t mock_i2c_set_registers(uint8_t dev_addr, uint8_t start_reg, const uint8_t *p_data, uint16_t length);
uint8_t  mock_i2c_get_register(uint8_t dev_addr, uint8_t reg_addr);

void mock_i2c_inject_fault(mock_i2c_fault_t fault, uint32_t trigger_after_n_calls);
void mock_i2c_clear_faults(void);

uint32_t mock_i2c_get_read_count(uint8_t dev_addr);
uint32_t mock_i2c_get_write_count(uint8_t dev_addr);
uint8_t  mock_i2c_get_last_reg(uint8_t dev_addr);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_I2C_BUS_H */
