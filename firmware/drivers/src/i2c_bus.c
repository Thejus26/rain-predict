/**
 * @file    i2c_bus.c
 * @brief   Implementation of Bounded Non-Blocking I2C Driver with 9-Clock Lockup Recovery.
 * @details Target driver for STM32WLE5 I2C1 (PB6 SCL, PB7 SDA) with complete host simulation
 *          state tracking for unit testing.
 */

#include "i2c_bus.h"
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

static I2C_HandleTypeDef s_hi2c1;
static bool s_is_initialized = false;
static uint32_t s_current_speed_hz = I2C_BUS_SPEED_FAST_HZ;

/**
 * @brief Software microsecond delay for recovery clock generation.
 * @param[in] us Duration in microseconds.
 */
static void i2c_bus_delay_us(uint32_t us) {
    /* 48 MHz CPU clock: ~12 cycles per iteration */
    uint32_t cycles = us * 12U;
    while (cycles > 0U) {
        __NOP();
        cycles--;
    }
}

status_t i2c_bus_init(uint32_t speed_hz) {
    if (speed_hz != I2C_BUS_SPEED_STANDARD_HZ && speed_hz != I2C_BUS_SPEED_FAST_HZ) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_current_speed_hz = speed_hz;

    /* 1. Enable GPIO and Peripheral Clocks */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* 2. Configure PB6 (SCL) and PB7 (SDA) in Open-Drain mode with Alternate Function */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin       = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
    gpio_init.Mode      = GPIO_MODE_AF_OD;
    gpio_init.Pull      = GPIO_NOPULL; /* External 4.7k pull-up resistors on PCB */
    gpio_init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = PIN_I2C1_SCL_AF;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* 3. Configure I2C1 Timing Register for 48 MHz PCLK1 */
    s_hi2c1.Instance              = I2C1;
    s_hi2c1.Init.Timing           = (s_current_speed_hz == I2C_BUS_SPEED_FAST_HZ) ?
                                    0x00802172U : 0x10909CECU;
    s_hi2c1.Init.OwnAddress1      = 0U;
    s_hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    s_hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    s_hi2c1.Init.OwnAddress2      = 0U;
    s_hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    s_hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    s_hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&s_hi2c1) != HAL_OK) {
        return STATUS_ERR_I2C;
    }

    /* 4. Enable Analog Noise Filter */
    if (HAL_I2CEx_ConfigAnalogFilter(&s_hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        return STATUS_ERR_I2C;
    }

    s_is_initialized = true;
    return STATUS_OK;
}

status_t i2c_bus_deinit(void) {
    if (s_is_initialized) {
        (void)HAL_I2C_DeInit(&s_hi2c1);
        __HAL_RCC_I2C1_CLK_DISABLE();
        s_is_initialized = false;
    }
    return STATUS_OK;
}

status_t i2c_bus_read(uint8_t dev_addr,
                      uint8_t reg_addr,
                      uint8_t *p_data,
                      uint16_t length,
                      uint32_t timeout_ms) {
    if (p_data == NULL || length == 0U) {
        return STATUS_ERR_NULL_PTR;
    }
    if (!s_is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    uint16_t hal_addr = (uint16_t)((uint16_t)dev_addr << 1);

    HAL_StatusTypeDef hal_status = HAL_I2C_Mem_Read(&s_hi2c1,
                                                    hal_addr,
                                                    (uint16_t)reg_addr,
                                                    I2C_MEMADD_SIZE_8BIT,
                                                    p_data,
                                                    length,
                                                    timeout_ms);

    if (hal_status == HAL_OK) {
        return STATUS_OK;
    } else if (hal_status == HAL_TIMEOUT) {
        (void)i2c_bus_recover();
        return STATUS_ERR_TIMEOUT;
    } else {
        (void)i2c_bus_recover();
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }
}

status_t i2c_bus_write(uint8_t dev_addr,
                       uint8_t reg_addr,
                       const uint8_t *p_data,
                       uint16_t length,
                       uint32_t timeout_ms) {
    if (p_data == NULL || length == 0U) {
        return STATUS_ERR_NULL_PTR;
    }
    if (!s_is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    uint16_t hal_addr = (uint16_t)((uint16_t)dev_addr << 1);

    HAL_StatusTypeDef hal_status = HAL_I2C_Mem_Write(&s_hi2c1,
                                                     hal_addr,
                                                     (uint16_t)reg_addr,
                                                     I2C_MEMADD_SIZE_8BIT,
                                                     (uint8_t *)p_data,
                                                     length,
                                                     timeout_ms);

    if (hal_status == HAL_OK) {
        return STATUS_OK;
    } else if (hal_status == HAL_TIMEOUT) {
        (void)i2c_bus_recover();
        return STATUS_ERR_TIMEOUT;
    } else {
        (void)i2c_bus_recover();
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }
}

status_t i2c_bus_read16(uint8_t dev_addr,
                        uint8_t reg_addr,
                        uint16_t *p_value,
                        uint32_t timeout_ms) {
    if (p_value == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t raw_buf[2] = {0};
    status_t status = i2c_bus_read(dev_addr, reg_addr, raw_buf, 2U, timeout_ms);
    if (status == STATUS_OK) {
        /* Convert Big-Endian byte order to host endianness */
        *p_value = (uint16_t)(((uint16_t)raw_buf[0] << 8) | (uint16_t)raw_buf[1]);
    }
    return status;
}

status_t i2c_bus_write16(uint8_t dev_addr,
                         uint8_t reg_addr,
                         uint16_t value,
                         uint32_t timeout_ms) {
    uint8_t raw_buf[2];
    raw_buf[0] = (uint8_t)((value >> 8) & 0xFFU);
    raw_buf[1] = (uint8_t)(value & 0xFFU);
    return i2c_bus_write(dev_addr, reg_addr, raw_buf, 2U, timeout_ms);
}

status_t i2c_bus_is_device_ready(uint8_t dev_addr, uint32_t trials, uint32_t timeout_ms) {
    if (!s_is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    uint16_t hal_addr = (uint16_t)((uint16_t)dev_addr << 1);
    HAL_StatusTypeDef hal_status = HAL_I2C_IsDeviceReady(&s_hi2c1, hal_addr, trials, timeout_ms);

    return (hal_status == HAL_OK) ? STATUS_OK : STATUS_ERR_SENSOR_NO_RESPONSE;
}

status_t i2c_bus_recover(void) {
    /* 1. De-initialize I2C1 peripheral */
    (void)HAL_I2C_DeInit(&s_hi2c1);

    /* 2. Reconfigure PB6 (SCL) and PB7 (SDA) as Open-Drain GPIO outputs */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin   = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* Release SCL and SDA high */
    HAL_GPIO_WritePin(GPIOB, PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN, GPIO_PIN_SET);
    i2c_bus_delay_us(10);

    /* 3. If SDA is stuck LOW, clock SCL up to 9 times */
    for (uint8_t i = 0; i < I2C_BUS_MAX_RECOVERY_CLOCKS; i++) {
        if (HAL_GPIO_ReadPin(GPIOB, PIN_I2C1_SDA_PIN) == GPIO_PIN_SET) {
            break; /* Slave device released SDA */
        }
        HAL_GPIO_WritePin(GPIOB, PIN_I2C1_SCL_PIN, GPIO_PIN_RESET);
        i2c_bus_delay_us(5);
        HAL_GPIO_WritePin(GPIOB, PIN_I2C1_SCL_PIN, GPIO_PIN_SET);
        i2c_bus_delay_us(5);
    }

    /* 4. Generate explicit I2C STOP condition */
    HAL_GPIO_WritePin(GPIOB, PIN_I2C1_SDA_PIN, GPIO_PIN_RESET);
    i2c_bus_delay_us(5);
    HAL_GPIO_WritePin(GPIOB, PIN_I2C1_SCL_PIN, GPIO_PIN_SET);
    i2c_bus_delay_us(5);
    HAL_GPIO_WritePin(GPIOB, PIN_I2C1_SDA_PIN, GPIO_PIN_SET);
    i2c_bus_delay_us(10);

    /* 5. Re-initialize I2C1 peripheral with previous clock speed */
    return i2c_bus_init(s_current_speed_hz);
}

bool i2c_bus_is_busy(void) {
    if (!s_is_initialized) {
        return false;
    }
    return (HAL_I2C_GetState(&s_hi2c1) != HAL_I2C_STATE_READY);
}

#else

/* ============================================================================
 * Host Simulation & Unit Testing Implementation
 * ============================================================================ */

#define SIM_I2C_MAX_DEVICES     8
#define SIM_I2C_REG_SPACE       256

typedef struct {
    uint8_t  address;
    bool     is_active;
    uint8_t  registers[SIM_I2C_REG_SPACE];
    uint32_t read_count;
    uint32_t write_count;
    uint8_t  last_reg;
} sim_i2c_device_t;

static sim_i2c_device_t s_sim_devices[SIM_I2C_MAX_DEVICES];
static bool s_sim_initialized = true;
static uint32_t s_sim_speed_hz = I2C_BUS_SPEED_FAST_HZ;
static bool s_sim_sda_stuck = false;
static uint32_t s_sim_recovery_pulse_count = 0;
static bool s_sim_stop_condition_emitted = false;
static status_t s_sim_injected_fault = STATUS_OK;
static uint32_t s_sim_fault_trigger_delay = 0;
static uint32_t s_sim_transaction_count = 0;
static uint32_t s_sim_active_fault_type = 0;

static sim_i2c_device_t *sim_find_device(uint8_t dev_addr) {
    for (size_t i = 0; i < SIM_I2C_MAX_DEVICES; i++) {
        if (s_sim_devices[i].is_active && s_sim_devices[i].address == dev_addr) {
            return &s_sim_devices[i];
        }
    }
    return NULL;
}

static sim_i2c_device_t *sim_find_or_create_device(uint8_t dev_addr) {
    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    if (p_dev != NULL) {
        return p_dev;
    }
    for (size_t i = 0; i < SIM_I2C_MAX_DEVICES; i++) {
        if (!s_sim_devices[i].is_active) {
            s_sim_devices[i].address   = dev_addr;
            s_sim_devices[i].is_active = true;
            return &s_sim_devices[i];
        }
    }
    return NULL;
}

void i2c_bus_test_reset(void) {
    (void)memset(s_sim_devices, 0, sizeof(s_sim_devices));
    s_sim_initialized = true;
    s_sim_speed_hz = I2C_BUS_SPEED_FAST_HZ;
    s_sim_sda_stuck = false;
    s_sim_recovery_pulse_count = 0;
    s_sim_stop_condition_emitted = false;
    s_sim_injected_fault = STATUS_OK;
    s_sim_fault_trigger_delay = 0;
    s_sim_transaction_count = 0;
    s_sim_active_fault_type = 0;
}

void i2c_bus_test_set_sda_stuck(bool stuck_low) {
    s_sim_sda_stuck = stuck_low;
}

bool i2c_bus_test_get_sda_stuck(void) {
    return s_sim_sda_stuck;
}

uint32_t i2c_bus_test_get_recovery_pulse_count(void) {
    return s_sim_recovery_pulse_count;
}

bool i2c_bus_test_get_stop_condition_emitted(void) {
    return s_sim_stop_condition_emitted;
}

uint32_t i2c_bus_test_get_speed_hz(void) {
    return s_sim_speed_hz;
}

bool i2c_bus_test_is_initialized(void) {
    return s_sim_initialized;
}

void i2c_bus_test_inject_fault(status_t fault) {
    s_sim_injected_fault = fault;
}

void i2c_bus_test_inject_fault_advanced(uint32_t fault_type, uint32_t trigger_after_n_calls) {
    s_sim_active_fault_type = fault_type;
    s_sim_fault_trigger_delay = trigger_after_n_calls;
}

void i2c_bus_test_clear_faults(void) {
    s_sim_injected_fault = STATUS_OK;
    s_sim_active_fault_type = 0;
    s_sim_fault_trigger_delay = 0;
}

status_t i2c_bus_test_set_slave_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t value) {
    sim_i2c_device_t *p_dev = sim_find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_INVALID_PARAM;
    }
    p_dev->registers[reg_addr] = value;
    return STATUS_OK;
}

status_t i2c_bus_test_set_slave_regs(uint8_t dev_addr, uint8_t start_reg, const uint8_t *p_data, uint16_t length) {
    if (p_data == NULL || ((uint32_t)start_reg + (uint32_t)length) > SIM_I2C_REG_SPACE) {
        return STATUS_ERR_INVALID_PARAM;
    }
    sim_i2c_device_t *p_dev = sim_find_or_create_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_INVALID_PARAM;
    }
    (void)memcpy(&p_dev->registers[start_reg], p_data, (size_t)length);
    return STATUS_OK;
}

uint8_t i2c_bus_test_get_slave_reg(uint8_t dev_addr, uint8_t reg_addr) {
    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    return (p_dev != NULL) ? p_dev->registers[reg_addr] : 0x00U;
}

uint32_t i2c_bus_test_get_read_count(uint8_t dev_addr) {
    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    return (p_dev != NULL) ? p_dev->read_count : 0U;
}

uint32_t i2c_bus_test_get_write_count(uint8_t dev_addr) {
    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    return (p_dev != NULL) ? p_dev->write_count : 0U;
}

uint8_t i2c_bus_test_get_last_reg(uint8_t dev_addr) {
    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    return (p_dev != NULL) ? p_dev->last_reg : 0x00U;
}

status_t i2c_bus_init(uint32_t speed_hz) {
    if (speed_hz != I2C_BUS_SPEED_STANDARD_HZ && speed_hz != I2C_BUS_SPEED_FAST_HZ) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_sim_speed_hz = speed_hz;
    s_sim_initialized = true;
    return STATUS_OK;
}

status_t i2c_bus_deinit(void) {
    s_sim_initialized = false;
    return STATUS_OK;
}

status_t i2c_bus_read(uint8_t dev_addr,
                      uint8_t reg_addr,
                      uint8_t *p_data,
                      uint16_t length,
                      uint32_t timeout_ms) {
    (void)timeout_ms;
    if (p_data == NULL || length == 0U) {
        return STATUS_ERR_NULL_PTR;
    }
    if (!s_sim_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    s_sim_transaction_count++;

    /* Process advanced mock fault injection if configured */
    if (s_sim_active_fault_type != 0U) {
        if (s_sim_fault_trigger_delay == 0U || s_sim_transaction_count >= s_sim_fault_trigger_delay) {
            switch (s_sim_active_fault_type) {
                case 1: /* MOCK_I2C_FAULT_NACK_ADDR */
                    return STATUS_ERR_SENSOR_NO_RESPONSE;
                case 2: /* MOCK_I2C_FAULT_NACK_DATA */
                case 4: /* MOCK_I2C_FAULT_BUS_ERROR */
                    return STATUS_ERR_I2C;
                case 3: /* MOCK_I2C_FAULT_TIMEOUT */
                    return STATUS_ERR_TIMEOUT;
                case 5: /* MOCK_I2C_FAULT_CORRUPT_DATA */
                    break;
                default:
                    break;
            }
        }
    }

    if (s_sim_injected_fault != STATUS_OK) {
        status_t fault = s_sim_injected_fault;
        if (fault == STATUS_ERR_TIMEOUT || fault == STATUS_ERR_SENSOR_NO_RESPONSE) {
            (void)i2c_bus_recover();
        }
        return fault;
    }
    if (s_sim_sda_stuck) {
        (void)i2c_bus_recover();
        return STATUS_ERR_TIMEOUT;
    }

    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    if (p_dev == NULL) {
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }

    p_dev->read_count++;
    p_dev->last_reg = reg_addr;

    for (uint16_t i = 0; i < length; i++) {
        uint8_t curr_reg = (uint8_t)(reg_addr + i);
        uint8_t val = p_dev->registers[curr_reg];
        if (s_sim_active_fault_type == 5U &&
            (s_sim_fault_trigger_delay == 0U || s_sim_transaction_count >= s_sim_fault_trigger_delay)) {
            val ^= 0xFFU;
        }
        p_data[i] = val;
    }
    return STATUS_OK;
}

status_t i2c_bus_write(uint8_t dev_addr,
                       uint8_t reg_addr,
                       const uint8_t *p_data,
                       uint16_t length,
                       uint32_t timeout_ms) {
    (void)timeout_ms;
    if (p_data == NULL || length == 0U) {
        return STATUS_ERR_NULL_PTR;
    }
    if (!s_sim_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    s_sim_transaction_count++;

    if (s_sim_active_fault_type != 0U) {
        if (s_sim_fault_trigger_delay == 0U || s_sim_transaction_count >= s_sim_fault_trigger_delay) {
            switch (s_sim_active_fault_type) {
                case 1: /* MOCK_I2C_FAULT_NACK_ADDR */
                    return STATUS_ERR_SENSOR_NO_RESPONSE;
                case 2: /* MOCK_I2C_FAULT_NACK_DATA */
                case 4: /* MOCK_I2C_FAULT_BUS_ERROR */
                    return STATUS_ERR_I2C;
                case 3: /* MOCK_I2C_FAULT_TIMEOUT */
                    return STATUS_ERR_TIMEOUT;
                default:
                    break;
            }
        }
    }

    if (s_sim_injected_fault != STATUS_OK) {
        status_t fault = s_sim_injected_fault;
        if (fault == STATUS_ERR_TIMEOUT || fault == STATUS_ERR_SENSOR_NO_RESPONSE) {
            (void)i2c_bus_recover();
        }
        return fault;
    }
    if (s_sim_sda_stuck) {
        (void)i2c_bus_recover();
        return STATUS_ERR_TIMEOUT;
    }

    sim_i2c_device_t *p_dev = sim_find_or_create_device(dev_addr);
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

status_t i2c_bus_read16(uint8_t dev_addr,
                        uint8_t reg_addr,
                        uint16_t *p_value,
                        uint32_t timeout_ms) {
    if (p_value == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t raw_buf[2] = {0};
    status_t status = i2c_bus_read(dev_addr, reg_addr, raw_buf, 2U, timeout_ms);
    if (status == STATUS_OK) {
        /* Convert Big-Endian byte order to host endianness */
        *p_value = (uint16_t)(((uint16_t)raw_buf[0] << 8) | (uint16_t)raw_buf[1]);
    }
    return status;
}

status_t i2c_bus_write16(uint8_t dev_addr,
                         uint8_t reg_addr,
                         uint16_t value,
                         uint32_t timeout_ms) {
    uint8_t raw_buf[2];
    raw_buf[0] = (uint8_t)((value >> 8) & 0xFFU);
    raw_buf[1] = (uint8_t)(value & 0xFFU);
    return i2c_bus_write(dev_addr, reg_addr, raw_buf, 2U, timeout_ms);
}

status_t i2c_bus_is_device_ready(uint8_t dev_addr, uint32_t trials, uint32_t timeout_ms) {
    (void)trials;
    (void)timeout_ms;
    if (!s_sim_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (s_sim_active_fault_type == 1U) { /* MOCK_I2C_FAULT_NACK_ADDR */
        return STATUS_ERR_SENSOR_NO_RESPONSE;
    }
    if (s_sim_injected_fault != STATUS_OK) {
        return s_sim_injected_fault;
    }
    sim_i2c_device_t *p_dev = sim_find_device(dev_addr);
    return (p_dev != NULL) ? STATUS_OK : STATUS_ERR_SENSOR_NO_RESPONSE;
}

status_t i2c_bus_recover(void) {
    /* Simulate SCL pulse clocking up to 9 pulses */
    s_sim_recovery_pulse_count = 0;
    for (uint8_t i = 0; i < I2C_BUS_MAX_RECOVERY_CLOCKS; i++) {
        s_sim_recovery_pulse_count++;
        if (!s_sim_sda_stuck) {
            break;
        }
        if (i == 4U) {
            /* Simulate slave releasing SDA on 5th clock */
            s_sim_sda_stuck = false;
        }
    }
    s_sim_sda_stuck = false;
    s_sim_stop_condition_emitted = true;
    return i2c_bus_init(s_sim_speed_hz);
}

bool i2c_bus_is_busy(void) {
    return false;
}

#endif /* HAVE_STM32WLXX_HAL */
