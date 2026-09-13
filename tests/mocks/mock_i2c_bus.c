/**
 * @file    mock_i2c_bus.c
 * @brief   Implementation of Mock I2C Bus Subsystem with Register Emulation.
 * @details Delegates to i2c_bus driver simulation backend without redefining driver APIs.
 */

#include "mock_i2c_bus.h"

void mock_i2c_init(void) {
    (void)i2c_bus_init(I2C_BUS_SPEED_FAST_HZ);
    i2c_bus_test_reset();
}

void mock_i2c_reset(void) {
    (void)i2c_bus_init(I2C_BUS_SPEED_FAST_HZ);
    i2c_bus_test_reset();
}

status_t mock_i2c_set_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t value) {
    return i2c_bus_test_set_slave_reg(dev_addr, reg_addr, value);
}

status_t mock_i2c_set_registers(uint8_t dev_addr, uint8_t start_reg, const uint8_t *p_data, uint16_t length) {
    return i2c_bus_test_set_slave_regs(dev_addr, start_reg, p_data, length);
}

uint8_t mock_i2c_get_register(uint8_t dev_addr, uint8_t reg_addr) {
    return i2c_bus_test_get_slave_reg(dev_addr, reg_addr);
}

void mock_i2c_inject_fault(mock_i2c_fault_t fault, uint32_t trigger_after_n_calls) {
    i2c_bus_test_inject_fault_advanced((uint32_t)fault, trigger_after_n_calls);
}

void mock_i2c_clear_faults(void) {
    i2c_bus_test_clear_faults();
}

uint32_t mock_i2c_get_read_count(uint8_t dev_addr) {
    return i2c_bus_test_get_read_count(dev_addr);
}

uint32_t mock_i2c_get_write_count(uint8_t dev_addr) {
    return i2c_bus_test_get_write_count(dev_addr);
}

uint8_t mock_i2c_get_last_reg(uint8_t dev_addr) {
    return i2c_bus_test_get_last_reg(dev_addr);
}
