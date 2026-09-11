/**
 * @file    mock_i2c_bus.c
 * @brief   Implementation of Mock I2C Bus Subsystem with Register Emulation.
 */

#include "mock_i2c_bus.h"
#include <string.h>

typedef struct {
    uint8_t  address;
    bool     is_configured;
    uint8_t  registers[MOCK_I2C_REGISTER_SPACE];
    uint32_t read_count;
    uint32_t write_count;
    uint8_t  last_reg;
} mock_device_t;

static mock_device_t s_devices[MOCK_I2C_MAX_DEVICES];
static mock_i2c_fault_t s_active_fault = MOCK_I2C_FAULT_NONE;
static uint32_t s_fault_delay_count = 0;
static uint32_t s_total_transactions = 0;

static mock_device_t *find_or_create_device(uint8_t dev_addr) {
    for (size_t i = 0; i < MOCK_I2C_MAX_DEVICES; i++) {
        if (s_devices[i].is_configured && s_devices[i].address == dev_addr) {
            return &s_devices[i];
        }
    }
    for (size_t i = 0; i < MOCK_I2C_MAX_DEVICES; i++) {
        if (!s_devices[i].is_configured) {
            s_devices[i].address = dev_addr;
            s_devices[i].is_configured = true;
            return &s_devices[i];
        }
    }
    return NULL;
}

void mock_i2c_init(void) {
    mock_i2c_reset();
}

void mock_i2c_reset(void) {
    (void)memset(s_devices, 0, sizeof(s_devices));
    s_active_fault = MOCK_I2C_FAULT_NONE;
    s_fault_delay_count = 0;
    s_total_transactions = 0;
}

status_t mock_i2c_set_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t value) {
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_INVALID_PARAM;
    }
    p_dev->registers[reg_addr] = value;
    return STATUS_OK;
}

status_t mock_i2c_set_registers(uint8_t dev_addr, uint8_t start_reg, const uint8_t *p_data, uint16_t length) {
    if (p_data == NULL || ((uint32_t)start_reg + (uint32_t)length) > MOCK_I2C_REGISTER_SPACE) {
        return STATUS_ERR_INVALID_PARAM;
    }
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_INVALID_PARAM;
    }
    (void)memcpy(&p_dev->registers[start_reg], p_data, (size_t)length);
    return STATUS_OK;
}

uint8_t mock_i2c_get_register(uint8_t dev_addr, uint8_t reg_addr) {
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return 0x00U;
    }
    return p_dev->registers[reg_addr];
}

void mock_i2c_inject_fault(mock_i2c_fault_t fault, uint32_t trigger_after_n_calls) {
    s_active_fault = fault;
    s_fault_delay_count = trigger_after_n_calls;
}

void mock_i2c_clear_faults(void) {
    s_active_fault = MOCK_I2C_FAULT_NONE;
    s_fault_delay_count = 0;
}

uint32_t mock_i2c_get_read_count(uint8_t dev_addr) {
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    return (p_dev != NULL) ? p_dev->read_count : 0U;
}

uint32_t mock_i2c_get_write_count(uint8_t dev_addr) {
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    return (p_dev != NULL) ? p_dev->write_count : 0U;
}

uint8_t mock_i2c_get_last_reg(uint8_t dev_addr) {
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    return (p_dev != NULL) ? p_dev->last_reg : 0x00U;
}

/* --- Implementation of Production I2C Bus Abstraction --- */

status_t i2c_bus_init(uint32_t clock_speed_hz) {
    (void)clock_speed_hz;
    return STATUS_OK;
}

status_t i2c_bus_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_data, uint16_t length, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    s_total_transactions++;
    if (s_active_fault != MOCK_I2C_FAULT_NONE) {
        if (s_fault_delay_count == 0U || s_total_transactions >= s_fault_delay_count) {
            switch (s_active_fault) {
                case MOCK_I2C_FAULT_NACK_ADDR:
                    return STATUS_ERR_SENSOR_NO_RESPONSE;
                case MOCK_I2C_FAULT_NACK_DATA:
                case MOCK_I2C_FAULT_BUS_ERROR:
                    return STATUS_ERR_I2C;
                case MOCK_I2C_FAULT_TIMEOUT:
                    return STATUS_ERR_TIMEOUT;
                case MOCK_I2C_FAULT_CORRUPT_DATA:
                    /* Let data copy continue, but corrupt values */
                    break;
                case MOCK_I2C_FAULT_NONE:
                default:
                    break;
            }
        }
    }

    mock_device_t *p_dev = find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }

    p_dev->read_count++;
    p_dev->last_reg = reg_addr;

    for (uint16_t i = 0; i < length; i++) {
        uint8_t curr_reg = (uint8_t)(reg_addr + i);
        uint8_t val = p_dev->registers[curr_reg];
        if (s_active_fault == MOCK_I2C_FAULT_CORRUPT_DATA &&
            (s_fault_delay_count == 0U || s_total_transactions >= s_fault_delay_count)) {
            val ^= 0xFFU;
        }
        p_data[i] = val;
    }
    return STATUS_OK;
}

status_t i2c_bus_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *p_data, uint16_t length, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    s_total_transactions++;
    if (s_active_fault != MOCK_I2C_FAULT_NONE) {
        if (s_fault_delay_count == 0U || s_total_transactions >= s_fault_delay_count) {
            switch (s_active_fault) {
                case MOCK_I2C_FAULT_NACK_ADDR:
                    return STATUS_ERR_SENSOR_NO_RESPONSE;
                case MOCK_I2C_FAULT_NACK_DATA:
                case MOCK_I2C_FAULT_BUS_ERROR:
                    return STATUS_ERR_I2C;
                case MOCK_I2C_FAULT_TIMEOUT:
                    return STATUS_ERR_TIMEOUT;
                case MOCK_I2C_FAULT_CORRUPT_DATA:
                case MOCK_I2C_FAULT_NONE:
                default:
                    break;
            }
        }
    }

    mock_device_t *p_dev = find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }

    p_dev->write_count++;
    p_dev->last_reg = reg_addr;

    for (uint16_t i = 0; i < length; i++) {
        uint8_t curr_reg = (uint8_t)(reg_addr + i);
        p_dev->registers[curr_reg] = p_data[i];
    }
    return STATUS_OK;
}

status_t i2c_bus_is_device_ready(uint8_t dev_addr, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (s_active_fault == MOCK_I2C_FAULT_NACK_ADDR) {
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }
    mock_device_t *p_dev = find_or_create_device(dev_addr);
    return (p_dev != NULL && p_dev->is_configured) ? STATUS_OK : STATUS_ERR_SENSOR_NO_RESPONSE;
}
