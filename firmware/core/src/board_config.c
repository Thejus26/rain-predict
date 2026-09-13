/**
 * @file    board_config.c
 * @brief   Target board pin mapping and hardware configuration implementation.
 * @details Implements GPIO initialization, Stop 2 deep sleep conditioning, power gating,
 *          actuator drivers, and host simulation hooks for the STM32WLE5 SoC platform.
 */

#include "board_config.h"

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

status_t board_gpio_init(void) {
    /* 1. Enable GPIO Peripheral Bus Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {0};

    /* 2. Configure safe default output levels BEFORE enabling push-pull drivers */
    HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);      /* Sensor Rail OFF */
    HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_RESET);    /* RS-485 RX Mode */
    HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);  /* Battery Divider OFF (Active LOW) */
    HAL_GPIO_WritePin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, GPIO_PIN_RESET);          /* Buzzer Silent */
    HAL_GPIO_WritePin(PIN_RELAY_PORT, PIN_RELAY_PIN, GPIO_PIN_RESET);            /* Relay De-energized */
    HAL_GPIO_WritePin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, GPIO_PIN_RESET);          /* Status LED OFF */
    HAL_GPIO_WritePin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, GPIO_PIN_RESET);      /* Warning LED OFF */
    HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_RESET);    /* SDI-12 RX Mode */
    HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);      /* RF Switch Shutdown */
    HAL_GPIO_WritePin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);

    /* 3. Configure Output Pins */
    /* PA4: High-Side Switched Sensor Power Gate */
    gpio_init.Pin = PIN_PWR_SENS_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(PIN_PWR_SENS_PORT, &gpio_init);

    /* PA1: RS-485 Direction Control */
    gpio_init.Pin = PIN_RS485_DIR_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PIN_RS485_DIR_PORT, &gpio_init);

    /* PB1 (VBAT Div En), PB2 (Buzzer), PB4 (Relay), PB8 (LED OK), PB9 (LED WARN) */
    gpio_init.Pin = PIN_VBAT_DIV_EN_PIN | PIN_BUZZER_PIN | PIN_RELAY_PIN | PIN_LED_OK_PIN | PIN_LED_WARN_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PC2 (SDI-12 DIR), PC3/PC4/PC5 (RF Switch Lines) */
    gpio_init.Pin = PIN_SDI12_DIR_PIN | PIN_FE_CTRL1_PIN | PIN_FE_CTRL2_PIN | PIN_FE_CTRL3_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    /* 4. Configure Inputs & EXTI Interrupt Lines */
    /* PC13: User Diagnostic Push-Button */
    gpio_init.Pin = PIN_USER_BTN_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_USER_BTN_PORT, &gpio_init);

    /* PA0: Rain Gauge EXTI Falling-Edge Interrupt */
    gpio_init.Pin = PIN_RAIN_GAUGE_PIN;
    gpio_init.Mode = GPIO_MODE_IT_FALLING;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIN_RAIN_GAUGE_PORT, &gpio_init);

    HAL_NVIC_SetPriority(PIN_RAIN_GAUGE_EXTI_IRQn, 0x02, 0x00);
    HAL_NVIC_EnableIRQ(PIN_RAIN_GAUGE_EXTI_IRQn);

    /* 5. Condition Unpowered Digital Buses and ADC to Analog to Prevent Leakage */
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;

    /* PB0: Battery ADC Input */
    gpio_init.Pin = PIN_VBAT_MEAS_PIN;
    HAL_GPIO_Init(PIN_VBAT_MEAS_PORT, &gpio_init);

    /* PB6, PB7: I2C1 Bus */
    gpio_init.Pin = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PA2, PA3: USART1 RS-485 Bus */
    gpio_init.Pin = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    /* PC0, PC1: LPUART1 SDI-12 Bus */
    gpio_init.Pin = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    return STATUS_OK;
}

status_t board_gpio_sleep_prepare(void) {
    /* 1. Ensure all actuators and switched loads are driven to safe de-energized states */
    HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_RELAY_PORT, PIN_RELAY_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);

    GPIO_InitTypeDef gpio_analog = {0};
    gpio_analog.Mode = GPIO_MODE_ANALOG;
    gpio_analog.Pull = GPIO_NOPULL;

    /* 2. Isolate sensor buses, SPI, ADC, and button to Analog No-Pull */
    /* PB0, PB6, PB7 */
    gpio_analog.Pin = PIN_VBAT_MEAS_PIN | PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
    HAL_GPIO_Init(GPIOB, &gpio_analog);

    /* PA2, PA3, PA5, PA6, PA7 */
    gpio_analog.Pin = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio_analog);

    /* PC0, PC1, PC13 */
    gpio_analog.Pin = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN | PIN_USER_BTN_PIN;
    HAL_GPIO_Init(GPIOC, &gpio_analog);

    /* PA0 (Rain gauge EXTI0) and PC14/PC15 (LSE) remain active */
    return STATUS_OK;
}

status_t board_gpio_wake_restore(void) {
    GPIO_InitTypeDef gpio_init = {0};

    /* Restore User Diagnostic Button PC13 with internal pull-up */
    gpio_init.Pin = PIN_USER_BTN_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_USER_BTN_PORT, &gpio_init);

    return STATUS_OK;
}

status_t board_sensor_power_enable(bool enable) {
    GPIO_InitTypeDef gpio_init = {0};

    if (enable) {
        /* 1. Energize high-side P-MOSFET gate (Active HIGH on PA4) */
        HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_SET);

        /* 2. Mandatory 20ms RC rail stabilization delay */
        HAL_Delay(BOARD_POWER_RAIL_STABILIZATION_MS);

        /* 3. Reconfigure I2C1 pins: PB6 (SCL), PB7 (SDA) -> AF4 Open-Drain */
        gpio_init.Pin = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init.Alternate = PIN_I2C1_SCL_AF;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        /* 4. Reconfigure USART1 RS-485 pins: PA2 (TX), PA3 (RX) -> AF7 Push-Pull */
        gpio_init.Pin = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = PIN_RS485_TX_AF;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        /* 5. Reconfigure LPUART1 SDI-12 pins: PC0 (TX), PC1 (RX) -> AF8 Push-Pull */
        gpio_init.Pin = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init.Alternate = PIN_SDI12_TX_AF;
        HAL_GPIO_Init(GPIOC, &gpio_init);
    } else {
        /* 1. Isolate digital sensor bus pins to Analog No-Pull to prevent phantom leakage */
        gpio_init.Mode = GPIO_MODE_ANALOG;
        gpio_init.Pull = GPIO_NOPULL;

        gpio_init.Pin = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        gpio_init.Pin = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        gpio_init.Pin = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        /* 2. De-energize high-side P-MOSFET gate (Active LOW on PA4) */
        HAL_GPIO_WritePin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    }

    return STATUS_OK;
}

status_t board_vbat_divider_enable(bool enable) {
    if (enable) {
        /* Active LOW turns ON high-side P-MOSFET connecting divider */
        HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_RESET);
    } else {
        /* HIGH turns OFF high-side P-MOSFET isolating divider (< 10 nA standby) */
        HAL_GPIO_WritePin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    }
    return STATUS_OK;
}

void board_led_set(board_led_t led, bool state) {
    GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    if (led == BOARD_LED_OK) {
        HAL_GPIO_WritePin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, pin_state);
    } else if (led == BOARD_LED_WARN) {
        HAL_GPIO_WritePin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, pin_state);
    }
}

void board_led_toggle(board_led_t led) {
    if (led == BOARD_LED_OK) {
        HAL_GPIO_TogglePin(PIN_LED_OK_PORT, PIN_LED_OK_PIN);
    } else if (led == BOARD_LED_WARN) {
        HAL_GPIO_TogglePin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN);
    }
}

void board_buzzer_set(bool state) {
    HAL_GPIO_WritePin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_relay_set(bool state) {
    HAL_GPIO_WritePin(PIN_RELAY_PORT, PIN_RELAY_PIN, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_rs485_dir_set(board_rs485_dir_t dir) {
    HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN,
                      (dir == BOARD_RS485_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_sdi12_dir_set(board_sdi12_dir_t dir) {
    HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN,
                      (dir == BOARD_SDI12_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_rf_switch_set(board_rf_mode_t mode) {
    switch (mode) {
        case RF_SWITCH_RX:
            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
            break;
        case RF_SWITCH_TX_LP:
            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
            break;
        case RF_SWITCH_TX_HP:
            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_SET);
            break;
        case RF_SWITCH_SHUTDOWN:
        default:
            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
            break;
    }
}

bool board_button_is_pressed(void) {
    /* Active LOW: pressed when read value is RESET */
    return (HAL_GPIO_ReadPin(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN) == GPIO_PIN_RESET);
}

#else

/* ============================================================================
 * Host Simulation & Unit Test Implementation
 * ============================================================================ */

#include <string.h>

#define SIM_PORT_A 0
#define SIM_PORT_B 1
#define SIM_PORT_C 2
#define SIM_PORT_MAX 3
#define SIM_PIN_MAX 16

typedef struct {
    GPIO_PinState state;
    uint32_t mode;
    uint32_t pull;
    uint32_t af;
} sim_pin_t;

typedef struct {
    sim_pin_t pins[SIM_PIN_MAX];
} sim_port_t;

static sim_port_t s_sim_ports[SIM_PORT_MAX];
static uint32_t s_delay_call_count = 0;
static uint32_t s_last_delay_ms = 0;
static board_rf_mode_t s_current_rf_mode = RF_SWITCH_SHUTDOWN;

static uint32_t port_to_index(GPIO_TypeDef port) {
    if (port == GPIOB) {
        return SIM_PORT_B;
    }
    if (port == GPIOC) {
        return SIM_PORT_C;
    }
    return SIM_PORT_A;
}

static uint32_t pin_mask_to_index(uint16_t pin_mask) {
    for (uint32_t i = 0; i < SIM_PIN_MAX; i++) {
        if (pin_mask & (1U << i)) {
            return i;
        }
    }
    return 0;
}

static void sim_set_pin_config(GPIO_TypeDef port, uint16_t pin_mask, uint32_t mode, uint32_t pull, uint32_t af) {
    uint32_t p = port_to_index(port);
    for (uint32_t i = 0; i < SIM_PIN_MAX; i++) {
        if (pin_mask & (1U << i)) {
            s_sim_ports[p].pins[i].mode = mode;
            s_sim_ports[p].pins[i].pull = pull;
            s_sim_ports[p].pins[i].af = af;
        }
    }
}

static void sim_write_pin(GPIO_TypeDef port, uint16_t pin_mask, GPIO_PinState state) {
    uint32_t p = port_to_index(port);
    for (uint32_t i = 0; i < SIM_PIN_MAX; i++) {
        if (pin_mask & (1U << i)) {
            s_sim_ports[p].pins[i].state = state;
        }
    }
}

static GPIO_PinState sim_read_pin(GPIO_TypeDef port, uint16_t pin_mask) {
    uint32_t p = port_to_index(port);
    uint32_t idx = pin_mask_to_index(pin_mask);
    return s_sim_ports[p].pins[idx].state;
}

static void sim_toggle_pin(GPIO_TypeDef port, uint16_t pin_mask) {
    uint32_t p = port_to_index(port);
    uint32_t idx = pin_mask_to_index(pin_mask);
    s_sim_ports[p].pins[idx].state =
        (s_sim_ports[p].pins[idx].state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void board_test_reset(void) {
    (void)memset(s_sim_ports, 0, sizeof(s_sim_ports));
    s_delay_call_count = 0;
    s_last_delay_ms = 0;
    s_current_rf_mode = RF_SWITCH_SHUTDOWN;

    /* Default inputs to unpressed/idle states */
    s_sim_ports[SIM_PORT_C].pins[13].state = GPIO_PIN_SET; /* User button unpressed (pull-up) */
    s_sim_ports[SIM_PORT_A].pins[0].state = GPIO_PIN_SET;  /* Rain gauge reed switch idle */
    s_sim_ports[SIM_PORT_B].pins[1].state = GPIO_PIN_SET;  /* VBAT divider disabled (Active LOW) */
}

void board_test_set_button_pressed(bool pressed) {
    s_sim_ports[SIM_PORT_C].pins[13].state = pressed ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

GPIO_PinState board_test_get_pin_state(GPIO_TypeDef port, uint16_t pin) {
    return sim_read_pin(port, pin);
}

uint32_t board_test_get_pin_mode(GPIO_TypeDef port, uint16_t pin) {
    uint32_t p = port_to_index(port);
    uint32_t idx = pin_mask_to_index(pin);
    return s_sim_ports[p].pins[idx].mode;
}

uint32_t board_test_get_pin_pull(GPIO_TypeDef port, uint16_t pin) {
    uint32_t p = port_to_index(port);
    uint32_t idx = pin_mask_to_index(pin);
    return s_sim_ports[p].pins[idx].pull;
}

uint32_t board_test_get_pin_af(GPIO_TypeDef port, uint16_t pin) {
    uint32_t p = port_to_index(port);
    uint32_t idx = pin_mask_to_index(pin);
    return s_sim_ports[p].pins[idx].af;
}

bool board_test_is_sensor_power_enabled(void) {
    return (sim_read_pin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN) == GPIO_PIN_SET);
}

bool board_test_is_vbat_divider_enabled(void) {
    return (sim_read_pin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN) == GPIO_PIN_RESET);
}

board_rf_mode_t board_test_get_rf_mode(void) {
    return s_current_rf_mode;
}

uint32_t board_test_get_delay_call_count(void) {
    return s_delay_call_count;
}

uint32_t board_test_get_last_delay_ms(void) {
    return s_last_delay_ms;
}

void board_test_set_pin_config(GPIO_TypeDef port, uint16_t pin_mask, uint32_t mode, uint32_t pull, uint32_t af) {
    sim_set_pin_config(port, pin_mask, mode, pull, af);
}

void board_test_set_pin_state(GPIO_TypeDef port, uint16_t pin_mask, GPIO_PinState state) {
    sim_write_pin(port, pin_mask, state);
}

status_t board_gpio_init(void) {
    /* 1. Safe default output levels */
    sim_write_pin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    sim_write_pin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_RELAY_PORT, PIN_RELAY_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
    s_current_rf_mode = RF_SWITCH_SHUTDOWN;

    /* 2. Configure output pin modes */
    sim_set_pin_config(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
    sim_set_pin_config(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);

    sim_set_pin_config(GPIOB,
                       PIN_VBAT_DIV_EN_PIN | PIN_BUZZER_PIN | PIN_RELAY_PIN | PIN_LED_OK_PIN | PIN_LED_WARN_PIN,
                       GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);

    sim_set_pin_config(GPIOC,
                       PIN_SDI12_DIR_PIN | PIN_FE_CTRL1_PIN | PIN_FE_CTRL2_PIN | PIN_FE_CTRL3_PIN,
                       GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);

    /* 3. Configure input & EXTI pins */
    sim_set_pin_config(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN, GPIO_MODE_INPUT, GPIO_PULLUP, 0);
    sim_write_pin(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN, GPIO_PIN_SET); /* default high (unpressed) */

    sim_set_pin_config(PIN_RAIN_GAUGE_PORT, PIN_RAIN_GAUGE_PIN, GPIO_MODE_IT_FALLING, GPIO_NOPULL, 0);
    sim_write_pin(PIN_RAIN_GAUGE_PORT, PIN_RAIN_GAUGE_PIN, GPIO_PIN_SET); /* default high (idle) */

    /* 4. Condition unpowered buses & ADC to analog */
    sim_set_pin_config(PIN_VBAT_MEAS_PORT, PIN_VBAT_MEAS_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    sim_set_pin_config(GPIOB, PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    sim_set_pin_config(GPIOA, PIN_RS485_TX_PIN | PIN_RS485_RX_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    sim_set_pin_config(GPIOC, PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    return STATUS_OK;
}

status_t board_gpio_sleep_prepare(void) {
    /* 1. Drive all actuators and switched gates to inactive */
    sim_write_pin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    sim_write_pin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_RELAY_PORT, PIN_RELAY_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
    sim_write_pin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
    s_current_rf_mode = RF_SWITCH_SHUTDOWN;

    /* 2. Isolate sensor buses, SPI, ADC, and user button to Analog No-Pull */
    sim_set_pin_config(GPIOB, PIN_VBAT_MEAS_PIN | PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN,
                       GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    sim_set_pin_config(GPIOA, PIN_RS485_TX_PIN | PIN_RS485_RX_PIN | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                       GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    sim_set_pin_config(GPIOC, PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN | PIN_USER_BTN_PIN,
                       GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    /* PA0 (Rain gauge EXTI0) remains active */
    return STATUS_OK;
}

status_t board_gpio_wake_restore(void) {
    sim_set_pin_config(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN, GPIO_MODE_INPUT, GPIO_PULLUP, 0);
    return STATUS_OK;
}

status_t board_sensor_power_enable(bool enable) {
    if (enable) {
        sim_write_pin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_SET);

        /* Record 20ms stabilization delay */
        s_delay_call_count++;
        s_last_delay_ms = BOARD_POWER_RAIL_STABILIZATION_MS;

        /* Reconfigure sensor buses to Active Alternate Function modes */
        sim_set_pin_config(GPIOB, PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN,
                           GPIO_MODE_AF_OD, GPIO_NOPULL, PIN_I2C1_SCL_AF);
        sim_set_pin_config(GPIOA, PIN_RS485_TX_PIN | PIN_RS485_RX_PIN,
                           GPIO_MODE_AF_PP, GPIO_NOPULL, PIN_RS485_TX_AF);
        sim_set_pin_config(GPIOC, PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN,
                           GPIO_MODE_AF_PP, GPIO_NOPULL, PIN_SDI12_TX_AF);
    } else {
        /* First isolate sensor buses to Analog No-Pull to prevent phantom leakage */
        sim_set_pin_config(GPIOB, PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN,
                           GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
        sim_set_pin_config(GPIOA, PIN_RS485_TX_PIN | PIN_RS485_RX_PIN,
                           GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
        sim_set_pin_config(GPIOC, PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN,
                           GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

        sim_write_pin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    }

    return STATUS_OK;
}

status_t board_vbat_divider_enable(bool enable) {
    if (enable) {
        sim_write_pin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_RESET);
    } else {
        sim_write_pin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    }
    return STATUS_OK;
}

void board_led_set(board_led_t led, bool state) {
    GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    if (led == BOARD_LED_OK) {
        sim_write_pin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, pin_state);
    } else if (led == BOARD_LED_WARN) {
        sim_write_pin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, pin_state);
    }
}

void board_led_toggle(board_led_t led) {
    if (led == BOARD_LED_OK) {
        sim_toggle_pin(PIN_LED_OK_PORT, PIN_LED_OK_PIN);
    } else if (led == BOARD_LED_WARN) {
        sim_toggle_pin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN);
    }
}

void board_buzzer_set(bool state) {
    sim_write_pin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_relay_set(bool state) {
    sim_write_pin(PIN_RELAY_PORT, PIN_RELAY_PIN, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_rs485_dir_set(board_rs485_dir_t dir) {
    sim_write_pin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN,
                  (dir == BOARD_RS485_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_sdi12_dir_set(board_sdi12_dir_t dir) {
    sim_write_pin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN,
                  (dir == BOARD_SDI12_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_rf_switch_set(board_rf_mode_t mode) {
    s_current_rf_mode = mode;
    switch (mode) {
        case RF_SWITCH_RX:
            sim_write_pin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_SET);
            sim_write_pin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
            sim_write_pin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
            break;
        case RF_SWITCH_TX_LP:
            sim_write_pin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
            sim_write_pin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_SET);
            sim_write_pin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
            break;
        case RF_SWITCH_TX_HP:
            sim_write_pin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
            sim_write_pin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
            sim_write_pin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_SET);
            break;
        case RF_SWITCH_SHUTDOWN:
        default:
            sim_write_pin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
            sim_write_pin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN, GPIO_PIN_RESET);
            sim_write_pin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN, GPIO_PIN_RESET);
            break;
    }
}

bool board_button_is_pressed(void) {
    return (sim_read_pin(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN) == GPIO_PIN_RESET);
}

#endif
