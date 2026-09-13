/**
 * @file    bsp_power_rails.c
 * @brief   Implementation of Switched Power Rail Controller with Stabilization Guard Timing.
 * @details Target driver for STM32WLE5 high-side P-MOSFET load switches with host simulation backend.
 */

#include "bsp_power_rails.h"
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

static bool s_rail_states[BSP_POWER_RAIL_MAX] = {false, false};

status_t bsp_power_rails_init(void) {
    /* 1. Enable GPIO port clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {0};

    /* 2. Sensor Rail Gate PA4: Default OFF (LOW -> P-MOSFET gate HIGH / isolated) */
    HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    gpio_init.Pin   = PIN_PWR_SENS_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(PIN_PWR_SENS_PORT, &gpio_init);

    /* 3. Battery Divider Gate PB1: Default OFF (HIGH -> P-MOSFET gate HIGH / disconnected) */
    HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    gpio_init.Pin   = PIN_VBAT_DIV_EN_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_VBAT_DIV_EN_PORT, &gpio_init);

    s_rail_states[BSP_POWER_RAIL_SENSORS]    = false;
    s_rail_states[BSP_POWER_RAIL_VBAT_SENSE] = false;

    return STATUS_OK;
}

status_t bsp_power_rail_enable(bsp_power_rail_t rail, bool enable) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }

    if (rail == BSP_POWER_RAIL_SENSORS) {
        if (enable) {
            /* PA4 HIGH activates N-MOSFET gate, pulling P-MOSFET gate to GND (ON) */
            HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_SET);
            s_rail_states[BSP_POWER_RAIL_SENSORS] = true;
            (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_SENSORS);
        } else {
            /* PA4 LOW de-activates gate (OFF), bleeder resistor discharges rail */
            HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
            s_rail_states[BSP_POWER_RAIL_SENSORS] = false;
        }
    } else if (rail == BSP_POWER_RAIL_VBAT_SENSE) {
        if (enable) {
            /* PB1 LOW turns ON P-MOSFET battery divider gate */
            HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_RESET);
            s_rail_states[BSP_POWER_RAIL_VBAT_SENSE] = true;
            (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_VBAT_SENSE);
        } else {
            /* PB1 HIGH turns OFF P-MOSFET divider gate */
            HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
            s_rail_states[BSP_POWER_RAIL_VBAT_SENSE] = false;
        }
    }

    return STATUS_OK;
}

bool bsp_power_rail_is_enabled(bsp_power_rail_t rail) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return false;
    }
    return s_rail_states[rail];
}

status_t bsp_power_rails_all_off(void) {
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, false);
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
    return STATUS_OK;
}

status_t bsp_power_rail_stabilize(bsp_power_rail_t rail) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    uint32_t delay_ms = bsp_power_rail_get_stabilization_ms(rail);
    if (delay_ms > 0U) {
        HAL_Delay(delay_ms);
    }
    return STATUS_OK;
}

uint32_t bsp_power_rail_get_stabilization_ms(bsp_power_rail_t rail) {
    switch (rail) {
        case BSP_POWER_RAIL_SENSORS:
            return BSP_POWER_RAIL_SENSORS_STABILIZE_MS;
        case BSP_POWER_RAIL_VBAT_SENSE:
            return BSP_POWER_RAIL_VBAT_STABILIZE_MS;
        default:
            return 0U;
    }
}

#else

/* ============================================================================
 * Host Simulation & Unit Testing Backend
 * ============================================================================ */

static bool s_sim_rails[BSP_POWER_RAIL_MAX]      = {false, false};
static bool s_sim_pin_states[BSP_POWER_RAIL_MAX] = {false, true}; /* PA4 LOW, PB1 HIGH */
static uint32_t s_sim_delay_calls                = 0U;
static uint32_t s_sim_last_delay_ms              = 0U;

void bsp_power_rails_test_reset(void) {
    s_sim_rails[BSP_POWER_RAIL_SENSORS]         = false;
    s_sim_rails[BSP_POWER_RAIL_VBAT_SENSE]      = false;
    s_sim_pin_states[BSP_POWER_RAIL_SENSORS]    = false; /* PA4 LOW (OFF) */
    s_sim_pin_states[BSP_POWER_RAIL_VBAT_SENSE] = true;  /* PB1 HIGH (OFF) */
    s_sim_delay_calls                           = 0U;
    s_sim_last_delay_ms                         = 0U;
}

uint32_t bsp_power_rails_test_get_delay_calls(void) {
    return s_sim_delay_calls;
}

uint32_t bsp_power_rails_test_get_last_delay_ms(void) {
    return s_sim_last_delay_ms;
}

bool bsp_power_rails_test_get_raw_pin_state(bsp_power_rail_t rail) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return false;
    }
    return s_sim_pin_states[rail];
}

status_t bsp_power_rails_init(void) {
    bsp_power_rails_test_reset();
    return STATUS_OK;
}

status_t bsp_power_rail_enable(bsp_power_rail_t rail, bool enable) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }

    if (rail == BSP_POWER_RAIL_SENSORS) {
        if (enable) {
            s_sim_pin_states[BSP_POWER_RAIL_SENSORS] = true; /* PA4 HIGH = ON */
            s_sim_rails[BSP_POWER_RAIL_SENSORS]      = true;
            (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_SENSORS);
        } else {
            s_sim_pin_states[BSP_POWER_RAIL_SENSORS] = false; /* PA4 LOW = OFF */
            s_sim_rails[BSP_POWER_RAIL_SENSORS]      = false;
        }
    } else if (rail == BSP_POWER_RAIL_VBAT_SENSE) {
        if (enable) {
            s_sim_pin_states[BSP_POWER_RAIL_VBAT_SENSE] = false; /* PB1 LOW = ON */
            s_sim_rails[BSP_POWER_RAIL_VBAT_SENSE]      = true;
            (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_VBAT_SENSE);
        } else {
            s_sim_pin_states[BSP_POWER_RAIL_VBAT_SENSE] = true;  /* PB1 HIGH = OFF */
            s_sim_rails[BSP_POWER_RAIL_VBAT_SENSE]      = false;
        }
    }

    return STATUS_OK;
}

bool bsp_power_rail_is_enabled(bsp_power_rail_t rail) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return false;
    }
    return s_sim_rails[rail];
}

status_t bsp_power_rails_all_off(void) {
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, false);
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
    return STATUS_OK;
}

status_t bsp_power_rail_stabilize(bsp_power_rail_t rail) {
    if (rail >= BSP_POWER_RAIL_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    uint32_t delay_ms = bsp_power_rail_get_stabilization_ms(rail);
    s_sim_delay_calls++;
    s_sim_last_delay_ms = delay_ms;
    return STATUS_OK;
}

uint32_t bsp_power_rail_get_stabilization_ms(bsp_power_rail_t rail) {
    switch (rail) {
        case BSP_POWER_RAIL_SENSORS:
            return BSP_POWER_RAIL_SENSORS_STABILIZE_MS;
        case BSP_POWER_RAIL_VBAT_SENSE:
            return BSP_POWER_RAIL_VBAT_STABILIZE_MS;
        default:
            return 0U;
    }
}

#endif /* HAVE_STM32WLXX_HAL */
