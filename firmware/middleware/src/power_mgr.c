/**
 * @file    power_mgr.c
 * @brief   Implementation of Low-Power Sleep Management & Pre-Sleep GPIO Conditioning.
 * @details Eliminates parasitic sensor back-powering and CMOS shoot-through leakage
 *          for STM32WLE5 SoC ultra-low-power Stop 2 deep sleep (< 3.0 uA).
 */

#include "power_mgr.h"
#include "bsp_power_rails.h"
#include "bsp_indicators.h"

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

status_t power_mgr_init(void) {
    /* 1. Enable Ultra-Low-Power mode and Backup Domain access */
    HAL_PWREx_EnableUltraLowPowerMode();
    HAL_PWR_EnableBkUpAccess();
    return STATUS_OK;
}

status_t power_mgr_isolate_sensor_buses(void) {
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;

    /* 1. Isolate RS-485 UART Bus (PA2 TX / PA3 RX) & SPI1 Bus (PA5 SCK / PA6 MISO / PA7 MOSI) */
    gpio_init.Pin = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    /* 2. Isolate I2C1 Sensor Bus (PB6 SCL / PB7 SDA) */
    gpio_init.Pin = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* 3. Isolate SDI-12 Auxiliary Bus (PC0 TX / PC1 RX) */
    gpio_init.Pin = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    return STATUS_OK;
}

status_t power_mgr_restore_sensor_buses(void) {
    GPIO_InitTypeDef gpio_init = {0};

    /* 1. Restore I2C1 pins: PB6 (SCL), PB7 (SDA) -> AF4 Open-Drain */
    gpio_init.Pin = PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN;
    gpio_init.Mode = GPIO_MODE_AF_OD;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = PIN_I2C1_SCL_AF;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* 2. Restore USART1 RS-485 pins: PA2 (TX), PA3 (RX) -> AF7 Push-Pull */
    gpio_init.Pin = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = PIN_RS485_TX_AF;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    /* 3. Restore LPUART1 SDI-12 pins: PC0 (TX), PC1 (RX) -> AF8 Push-Pull */
    gpio_init.Pin = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = PIN_SDI12_TX_AF;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    /* 4. Restore SPI1 pins: PA5 (SCK), PA6 (MISO), PA7 (MOSI) -> AF5 Push-Pull */
    gpio_init.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    return STATUS_OK;
}

status_t power_mgr_gpio_sleep_prepare(void) {
    /* 1. De-energize all sensor power rails and battery divider */
    (void)bsp_power_rails_all_off();

    /* 2. Silence buzzer, turn off LEDs, de-energize siren relay */
    (void)bsp_indicators_all_off();

    /* 3. Set RF Switch to shutdown mode */
    board_rf_switch_set(RF_SWITCH_SHUTDOWN);

    /* 4. Isolate all active sensor communication buses */
    (void)power_mgr_isolate_sensor_buses();

    /* 5. Condition all unrouted/unused pins to Analog mode (No-Pull) to prevent CMOS shoot-through */
    GPIO_InitTypeDef gpio_analog = {0};
    gpio_analog.Mode = GPIO_MODE_ANALOG;
    gpio_analog.Pull = GPIO_NOPULL;

    /* Port A Unused Pins: PA8, PA9, PA10, PA11, PA12, PA15 */
    gpio_analog.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &gpio_analog);

    /* Port B Unused Pins & ADC: PB0, PB3, PB5, PB10, PB11, PB12, PB13, PB14, PB15 */
    gpio_analog.Pin = GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 |
                      GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &gpio_analog);

    /* Port C Unused Pins & Button: PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13 */
    gpio_analog.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
                      GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &gpio_analog);

    /* Preserved Wakeup & Oscillator Exemptions:
     * - PA0 (Rain Gauge EXTI0): GPIO_MODE_IT_FALLING preserved for wakeup.
     * - PC14/PC15 (LSE Quartz): RCC Analog preserved for continuous RTC timing.
     * - PA13/PA14 (SWDIO/SWCLK): AF0 SWD preserved for debug attachment.
     * - PA4 (PIN_PWR_SENS_EN): Output Push-Pull driven LOW (0V).
     * - PB1 (PIN_VBAT_DIV_EN): Output Push-Pull driven HIGH (Vbat, < 10 nA).
     * - PB8/PB9/PB2/PB4: Output Push-Pull driven LOW (de-energized).
     * - PC3/PC4/PC5: Output Push-Pull driven LOW (RF switch shutdown).
     */

    return STATUS_OK;
}

status_t power_mgr_gpio_wake_restore(void) {
    /* 1. Restore diagnostic button pin */
    (void)board_gpio_wake_restore();

    /* 2. Restore active bus peripheral pin multiplexing */
    (void)power_mgr_restore_sensor_buses();

    return STATUS_OK;
}

status_t power_mgr_verify_leakage_state(void) {
    /* Verify PA4 is LOW (Sensor power OFF) */
    if (HAL_GPIO_ReadPin(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN) != GPIO_PIN_RESET) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify PB1 is HIGH (Battery divider OFF) */
    if (HAL_GPIO_ReadPin(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN) != GPIO_PIN_SET) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify actuators and indicators are de-energized */
    if (HAL_GPIO_ReadPin(PIN_BUZZER_PORT, PIN_BUZZER_PIN) != GPIO_PIN_RESET ||
        HAL_GPIO_ReadPin(PIN_RELAY_PORT, PIN_RELAY_PIN) != GPIO_PIN_RESET ||
        HAL_GPIO_ReadPin(PIN_LED_OK_PORT, PIN_LED_OK_PIN) != GPIO_PIN_RESET ||
        HAL_GPIO_ReadPin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN) != GPIO_PIN_RESET) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify RF switch lines are LOW (Shutdown) */
    if (HAL_GPIO_ReadPin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN) != GPIO_PIN_RESET ||
        HAL_GPIO_ReadPin(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN) != GPIO_PIN_RESET ||
        HAL_GPIO_ReadPin(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN) != GPIO_PIN_RESET) {
        return STATUS_ERR_INVALID_STATE;
    }

    return STATUS_OK;
}

#else

/* ============================================================================
 * Host Simulation & Unit Testing Backend
 * ============================================================================ */

static power_state_t s_sim_power_state = POWER_STATE_RUN;

void power_mgr_test_reset(void) {
    s_sim_power_state = POWER_STATE_RUN;
}

power_state_t power_mgr_test_get_state(void) {
    return s_sim_power_state;
}

void power_mgr_test_set_state(power_state_t state) {
    s_sim_power_state = state;
}

status_t power_mgr_init(void) {
    s_sim_power_state = POWER_STATE_RUN;
    return STATUS_OK;
}

status_t power_mgr_isolate_sensor_buses(void) {
    /* 1. Isolate RS-485 UART Bus (PA2 TX / PA3 RX) & SPI1 Bus (PA5 SCK / PA6 MISO / PA7 MOSI) */
    board_test_set_pin_config(GPIOA,
                              PIN_RS485_TX_PIN | PIN_RS485_RX_PIN | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                              GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    /* 2. Isolate I2C1 Sensor Bus (PB6 SCL / PB7 SDA) */
    board_test_set_pin_config(GPIOB,
                              PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN,
                              GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    /* 3. Isolate SDI-12 Auxiliary Bus (PC0 TX / PC1 RX) */
    board_test_set_pin_config(GPIOC,
                              PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN,
                              GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    return STATUS_OK;
}

status_t power_mgr_restore_sensor_buses(void) {
    /* 1. Restore I2C1 pins: PB6 (SCL), PB7 (SDA) -> AF4 Open-Drain */
    board_test_set_pin_config(GPIOB,
                              PIN_I2C1_SCL_PIN | PIN_I2C1_SDA_PIN,
                              GPIO_MODE_AF_OD, GPIO_NOPULL, PIN_I2C1_SCL_AF);

    /* 2. Restore USART1 RS-485 pins: PA2 (TX), PA3 (RX) -> AF7 Push-Pull */
    board_test_set_pin_config(GPIOA,
                              PIN_RS485_TX_PIN | PIN_RS485_RX_PIN,
                              GPIO_MODE_AF_PP, GPIO_NOPULL, PIN_RS485_TX_AF);

    /* 3. Restore LPUART1 SDI-12 pins: PC0 (TX), PC1 (RX) -> AF8 Push-Pull */
    board_test_set_pin_config(GPIOC,
                              PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN,
                              GPIO_MODE_AF_PP, GPIO_NOPULL, PIN_SDI12_TX_AF);

    /* 4. Restore SPI1 pins: PA5 (SCK), PA6 (MISO), PA7 (MOSI) -> AF5 Push-Pull */
    board_test_set_pin_config(GPIOA,
                              GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                              GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI1);

    return STATUS_OK;
}

status_t power_mgr_gpio_sleep_prepare(void) {
    /* 1. De-energize all sensor power rails and battery divider */
    (void)bsp_power_rails_all_off();

    /* 2. Silence buzzer, turn off LEDs, de-energize siren relay */
    (void)bsp_indicators_all_off();

    /* 3. Set RF Switch to shutdown mode */
    board_rf_switch_set(RF_SWITCH_SHUTDOWN);

    /* 4. Isolate all active sensor communication buses */
    (void)power_mgr_isolate_sensor_buses();

    /* 5. Condition all unrouted/unused pins to Analog mode (No-Pull) */
    /* Port A Unused Pins: PA8, PA9, PA10, PA11, PA12, PA15 */
    board_test_set_pin_config(GPIOA,
                              GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15,
                              GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    /* Port B Unused Pins & ADC: PB0, PB3, PB5, PB10, PB11, PB12, PB13, PB14, PB15 */
    board_test_set_pin_config(GPIOB,
                              GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 |
                              GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
                              GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    /* Port C Unused Pins & Button: PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13 */
    board_test_set_pin_config(GPIOC,
                              GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
                              GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13,
                              GPIO_MODE_ANALOG, GPIO_NOPULL, 0);

    s_sim_power_state = POWER_STATE_STOP2;

    return STATUS_OK;
}

status_t power_mgr_gpio_wake_restore(void) {
    /* 1. Restore diagnostic button pin */
    (void)board_gpio_wake_restore();

    /* 2. Restore active bus peripheral pin multiplexing */
    (void)power_mgr_restore_sensor_buses();

    s_sim_power_state = POWER_STATE_RUN;

    return STATUS_OK;
}

status_t power_mgr_verify_leakage_state(void) {
    /* Verify PA4 is LOW (Sensor power OFF) */
    if (board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN) != GPIO_PIN_RESET) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify PB1 is HIGH (Battery divider OFF) */
    if (board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN) != GPIO_PIN_SET) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify power rail driver reporting */
    if (bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS) ||
        bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE)) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify actuators and indicators are de-energized */
    if (bsp_led_get(BSP_LED_GREEN) || bsp_led_get(BSP_LED_RED) ||
        bsp_buzzer_get() || bsp_relay_get()) {
        return STATUS_ERR_INVALID_STATE;
    }

    if (board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN) != GPIO_PIN_RESET ||
        board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN) != GPIO_PIN_RESET ||
        board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN) != GPIO_PIN_RESET ||
        board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN) != GPIO_PIN_RESET) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify RF switch is shutdown */
    if (board_test_get_rf_mode() != RF_SWITCH_SHUTDOWN) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify digital sensor communication buses are in Analog mode */
    if (board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(GPIOA, GPIO_PIN_5) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(GPIOA, GPIO_PIN_6) != GPIO_MODE_ANALOG ||
        board_test_get_pin_mode(GPIOA, GPIO_PIN_7) != GPIO_MODE_ANALOG) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Verify all unused pins are in Analog mode */
    static const uint16_t unused_a[] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_15};
    for (size_t i = 0; i < sizeof(unused_a) / sizeof(unused_a[0]); i++) {
        if (board_test_get_pin_mode(GPIOA, unused_a[i]) != GPIO_MODE_ANALOG) {
            return STATUS_ERR_INVALID_STATE;
        }
    }

    static const uint16_t unused_b[] = {GPIO_PIN_0, GPIO_PIN_3, GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_11,
                                         GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15};
    for (size_t i = 0; i < sizeof(unused_b) / sizeof(unused_b[0]); i++) {
        if (board_test_get_pin_mode(GPIOB, unused_b[i]) != GPIO_MODE_ANALOG) {
            return STATUS_ERR_INVALID_STATE;
        }
    }

    static const uint16_t unused_c[] = {GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9,
                                         GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13};
    for (size_t i = 0; i < sizeof(unused_c) / sizeof(unused_c[0]); i++) {
        if (board_test_get_pin_mode(GPIOC, unused_c[i]) != GPIO_MODE_ANALOG) {
            return STATUS_ERR_INVALID_STATE;
        }
    }

    return STATUS_OK;
}

#endif /* HAVE_STM32WLXX_HAL */
