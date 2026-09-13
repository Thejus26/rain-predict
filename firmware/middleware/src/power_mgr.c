/**
 * @file    power_mgr.c
 * @brief   Implementation of STM32WLE5 Stop 2 Deep Sleep & RTC Wakeup Manager.
 * @details Eliminates parasitic sensor back-powering and CMOS shoot-through leakage,
 *          manages ultra-low-power Stop 2 deep sleep (< 3.0 uA) with full SRAM1/SRAM2 retention,
 *          arms LSE-clocked hardware RTC periodic wakeup timer (EXTI19), detects multi-source
 *          wake triggers, and restores 48 MHz MSI clock execution in < 5 us.
 */

#include "power_mgr.h"
#include "system_clock.h"
#include "bsp_power_rails.h"
#include "bsp_indicators.h"
#include "bsp_adc.h"

/* ============================================================================
 * Cached Battery Status & Default State
 * ============================================================================ */

static const power_battery_status_t s_battery_status_default = {
    .vbat_mv               = 3300U,
    .soc_percent           = 80U,
    .health                = POWER_BATTERY_HEALTH_OPTIMAL,
    .solar_status          = SOLAR_STATUS_NIGHT,
    .delta_vbat_mv_per_hr  = 0,
    .last_sample_timestamp = 0,
    .throttling_active     = false
};

static power_battery_status_t s_battery_status = {
    .vbat_mv               = 3300U,
    .soc_percent           = 80U,
    .health                = POWER_BATTERY_HEALTH_OPTIMAL,
    .solar_status          = SOLAR_STATUS_NIGHT,
    .delta_vbat_mv_per_hr  = 0,
    .last_sample_timestamp = 0,
    .throttling_active     = false
};

static uint16_t s_prev_vbat_mv = 3300U;

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

extern RTC_HandleTypeDef hrtc;

static power_wake_reason_t s_last_wake_reason = POWER_WAKE_REASON_UNKNOWN;
static uint32_t s_cumulative_sleep_sec = 0;

status_t power_mgr_init(void) {
    /* 1. Enable Ultra-Low-Power mode and Backup Domain access */
    HAL_PWREx_EnableUltraLowPowerMode();
    HAL_PWR_EnableBkUpAccess();

    /* 2. Configure SRAM1 and SRAM2 full content retention in Stop 2 */
    HAL_PWREx_EnableSRAM1ContentRetention();
    HAL_PWREx_EnableSRAM2ContentRetention();

    /* 3. Configure Flash memory to enter deep power-down during Stop 2 */
    HAL_PWREx_EnableFlashPowerDown(PWR_FLASHPD_STOP);

    /* 4. Initialize internal tracking metrics */
    s_last_wake_reason = POWER_WAKE_REASON_UNKNOWN;
    s_cumulative_sleep_sec = 0;
    s_battery_status = s_battery_status_default;
    s_prev_vbat_mv = 3300U;

    return STATUS_OK;
}

status_t power_mgr_set_rtc_wakeup(uint32_t interval_sec) {
    if (interval_sec < POWER_MGR_MIN_SLEEP_SEC || interval_sec > POWER_MGR_MAX_SLEEP_SEC) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* Deactivate any pending wakeup timer */
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

    /* Clear RTC Wakeup Timer Flag */
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);

    /* Clock source: 1 Hz ck_spre (when interval_sec >= 1) */
    uint32_t wut_counter = interval_sec - 1U;

    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wut_counter, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0) != HAL_OK) {
        return STATUS_ERR_GENERIC;
    }

    return STATUS_OK;
}

status_t power_mgr_cancel_rtc_wakeup(void) {
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    return STATUS_OK;
}

status_t power_mgr_enter_stop2(uint32_t sleep_duration_sec) {
    if (sleep_duration_sec < POWER_MGR_MIN_SLEEP_SEC || sleep_duration_sec > POWER_MGR_MAX_SLEEP_SEC) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* 1. Pre-sleep hardware and GPIO conditioning */
    status_t status = power_mgr_gpio_sleep_prepare();
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Configure clock tree wake-up source to MSI */
    (void)system_clock_sleep_prepare();

    /* 3. Arm RTC Periodic Wakeup Timer */
    status = power_mgr_set_rtc_wakeup(sleep_duration_sec);
    if (status != STATUS_OK) {
        return status;
    }

    /* 4. Clear all pending EXTI and Wakeup flags */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);

    /* 5. Enter STM32WLE5 Stop 2 Deep Sleep Mode */
    HAL_SuspendTick();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
    HAL_ResumeTick();

    /* --- MCU Execution Resumes Here Upon Wakeup --- */

    /* 6. Restore system clocks, Flash latency, and active GPIO multiplexing */
    (void)power_mgr_wake_restore();

    /* 7. Track cumulative sleep metrics */
    s_cumulative_sleep_sec += sleep_duration_sec;

    return STATUS_OK;
}

status_t power_mgr_wake_restore(void) {
    /* 1. Restore MSI 48 MHz System Clock and Flash Latency */
    (void)system_clock_wake_restore();

    /* 2. Restore Peripheral Pin Assignments and GPIO Modes */
    (void)power_mgr_gpio_wake_restore();

    /* 3. Identify Wakeup Trigger Reason */
    if (__HAL_RTC_WAKEUPTIMER_GET_FLAG(&hrtc, RTC_FLAG_WUTF) != RESET) {
        s_last_wake_reason = POWER_WAKE_REASON_RTC;
        __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    } else if (__HAL_GPIO_EXTI_GET_IT(PIN_RAIN_GAUGE_PIN) != RESET) {
        s_last_wake_reason = POWER_WAKE_REASON_RAIN_EXTI;
        __HAL_GPIO_EXTI_CLEAR_IT(PIN_RAIN_GAUGE_PIN);
    } else if (__HAL_GPIO_EXTI_GET_IT(PIN_USER_BTN_PIN) != RESET) {
        s_last_wake_reason = POWER_WAKE_REASON_BUTTON;
        __HAL_GPIO_EXTI_CLEAR_IT(PIN_USER_BTN_PIN);
    } else {
        s_last_wake_reason = POWER_WAKE_REASON_UNKNOWN;
    }

    return STATUS_OK;
}

power_wake_reason_t power_mgr_get_wake_reason(void) {
    return s_last_wake_reason;
}

uint32_t power_mgr_get_total_sleep_time_sec(void) {
    return s_cumulative_sleep_sec;
}

void power_mgr_reset_total_sleep_time(void) {
    s_cumulative_sleep_sec = 0;
}

status_t power_mgr_enter_standby(void) {
    (void)power_mgr_gpio_sleep_prepare();
    HAL_PWR_EnterSTANDBYMode();
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

static power_state_t       s_sim_power_state          = POWER_STATE_RUN;
static power_wake_reason_t s_sim_last_wake_reason     = POWER_WAKE_REASON_UNKNOWN;
static power_wake_reason_t s_sim_next_wake_reason     = POWER_WAKE_REASON_RTC;
static uint32_t            s_sim_cumulative_sleep_sec = 0;
static uint32_t            s_sim_rtc_interval_sec     = 0;
static bool                s_sim_rtc_armed            = false;
static uint32_t            s_sim_sleep_cycles         = 0;

void power_mgr_test_reset(void) {
    s_sim_power_state          = POWER_STATE_RUN;
    s_sim_last_wake_reason     = POWER_WAKE_REASON_UNKNOWN;
    s_sim_next_wake_reason     = POWER_WAKE_REASON_RTC;
    s_sim_cumulative_sleep_sec = 0;
    s_sim_rtc_interval_sec     = 0;
    s_sim_rtc_armed            = false;
    s_sim_sleep_cycles         = 0;
    s_battery_status           = s_battery_status_default;
    s_prev_vbat_mv             = 3300U;
}

power_state_t power_mgr_test_get_state(void) {
    return s_sim_power_state;
}

void power_mgr_test_set_state(power_state_t state) {
    s_sim_power_state = state;
}

void power_mgr_test_set_wake_reason(power_wake_reason_t reason) {
    s_sim_next_wake_reason = reason;
    s_sim_last_wake_reason = reason;
}

void power_mgr_test_inject_wake_event(power_wake_reason_t reason) {
    s_sim_next_wake_reason = reason;
}

uint32_t power_mgr_test_get_rtc_wakeup_interval(void) {
    return s_sim_rtc_interval_sec;
}

bool power_mgr_test_is_rtc_wakeup_armed(void) {
    return s_sim_rtc_armed;
}

uint32_t power_mgr_test_get_sleep_cycle_count(void) {
    return s_sim_sleep_cycles;
}

status_t power_mgr_init(void) {
    s_sim_power_state          = POWER_STATE_RUN;
    s_sim_last_wake_reason     = POWER_WAKE_REASON_UNKNOWN;
    s_sim_next_wake_reason     = POWER_WAKE_REASON_RTC;
    s_sim_cumulative_sleep_sec = 0;
    s_sim_rtc_interval_sec     = 0;
    s_sim_rtc_armed            = false;
    s_sim_sleep_cycles         = 0;
    s_battery_status           = s_battery_status_default;
    s_prev_vbat_mv             = 3300U;
    return STATUS_OK;
}

status_t power_mgr_set_rtc_wakeup(uint32_t interval_sec) {
    if (interval_sec < POWER_MGR_MIN_SLEEP_SEC || interval_sec > POWER_MGR_MAX_SLEEP_SEC) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_sim_rtc_interval_sec = interval_sec;
    s_sim_rtc_armed = true;
    return STATUS_OK;
}

status_t power_mgr_cancel_rtc_wakeup(void) {
    s_sim_rtc_interval_sec = 0;
    s_sim_rtc_armed = false;
    return STATUS_OK;
}

status_t power_mgr_enter_stop2(uint32_t sleep_duration_sec) {
    if (sleep_duration_sec < POWER_MGR_MIN_SLEEP_SEC || sleep_duration_sec > POWER_MGR_MAX_SLEEP_SEC) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* 1. Pre-sleep hardware and GPIO conditioning */
    status_t status = power_mgr_gpio_sleep_prepare();
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Configure clock tree wake-up source to MSI */
    (void)system_clock_sleep_prepare();

    /* 3. Arm RTC Periodic Wakeup Timer */
    status = power_mgr_set_rtc_wakeup(sleep_duration_sec);
    if (status != STATUS_OK) {
        return status;
    }

    s_sim_power_state = POWER_STATE_STOP2;
    s_sim_sleep_cycles++;

    /* --- Wakeup Event Restoration Simulation --- */
    (void)power_mgr_wake_restore();

    /* Track cumulative sleep metrics */
    s_sim_cumulative_sleep_sec += sleep_duration_sec;

    return STATUS_OK;
}

status_t power_mgr_wake_restore(void) {
    /* 1. Restore MSI 48 MHz System Clock and Flash Latency */
    (void)system_clock_wake_restore();

    /* 2. Restore Peripheral Pin Assignments and GPIO Modes */
    (void)power_mgr_gpio_wake_restore();

    /* 3. Apply and record injected/detected wake event reason */
    s_sim_last_wake_reason = s_sim_next_wake_reason;
    s_sim_power_state = POWER_STATE_RUN;

    return STATUS_OK;
}

power_wake_reason_t power_mgr_get_wake_reason(void) {
    return s_sim_last_wake_reason;
}

uint32_t power_mgr_get_total_sleep_time_sec(void) {
    return s_sim_cumulative_sleep_sec;
}

void power_mgr_reset_total_sleep_time(void) {
    s_sim_cumulative_sleep_sec = 0;
}

status_t power_mgr_enter_standby(void) {
    (void)power_mgr_gpio_sleep_prepare();
    s_sim_power_state = POWER_STATE_STANDBY;
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

#endif /* HAVE_STM32WLXX_HAL */

/* ============================================================================
 * Battery & Power Telemetry Implementation
 * ============================================================================ */

uint8_t power_mgr_battery_calc_soc(uint16_t vbat_mv) {
    if (vbat_mv >= 3400U) {
        return 100U;
    } else if (vbat_mv >= 3330U) {
        /* 3330 mV - 3399 mV -> 90% - 99% (Slope: 10% / 70 mV) */
        return (uint8_t)(90U + (((uint32_t)(vbat_mv - 3330U) * 10U) / 70U));
    } else if (vbat_mv >= 3300U) {
        /* 3300 mV - 3329 mV -> 70% - 89% (Slope: 20% / 30 mV) */
        return (uint8_t)(70U + (((uint32_t)(vbat_mv - 3300U) * 20U) / 30U));
    } else if (vbat_mv >= 3250U) {
        /* 3250 mV - 3299 mV -> 40% - 69% (Slope: 30% / 50 mV) */
        return (uint8_t)(40U + (((uint32_t)(vbat_mv - 3250U) * 30U) / 50U));
    } else if (vbat_mv >= 3200U) {
        /* 3200 mV - 3249 mV -> 20% - 39% (Slope: 20% / 50 mV) */
        return (uint8_t)(20U + (((uint32_t)(vbat_mv - 3200U) * 20U) / 50U));
    } else if (vbat_mv >= 3100U) {
        /* 3100 mV - 3199 mV -> 10% - 19% (Slope: 10% / 100 mV) */
        return (uint8_t)(10U + (((uint32_t)(vbat_mv - 3100U) * 10U) / 100U));
    } else if (vbat_mv >= 3000U) {
        /* 3000 mV - 3099 mV -> 5% - 9% (Slope: 5% / 100 mV) */
        return (uint8_t)(5U + (((uint32_t)(vbat_mv - 3000U) * 5U) / 100U));
    } else if (vbat_mv >= 2500U) {
        /* 2500 mV - 2999 mV -> 0% - 4% (Slope: 5% / 500 mV) */
        return (uint8_t)(((uint32_t)(vbat_mv - 2500U) * 5U) / 500U);
    } else {
        return 0U;
    }
}

status_t power_mgr_battery_update(uint16_t ambient_lux) {
    uint16_t vbat_mv = 0;
    status_t status = bsp_adc_read_vbat_mv(&vbat_mv);
    if (status != STATUS_OK) {
        return status;
    }

    s_battery_status.vbat_mv = vbat_mv;
    s_battery_status.soc_percent = power_mgr_battery_calc_soc(vbat_mv);

    /* Determine battery health state and throttling */
    if (vbat_mv >= POWER_BATTERY_OPTIMAL_THRESHOLD_MV) {
        s_battery_status.health = POWER_BATTERY_HEALTH_OPTIMAL;
        s_battery_status.throttling_active = false;
    } else if (vbat_mv >= POWER_BATTERY_LOW_THRESHOLD_MV) {
        s_battery_status.health = POWER_BATTERY_HEALTH_LOW;
        s_battery_status.throttling_active = true;
    } else {
        s_battery_status.health = POWER_BATTERY_HEALTH_CRITICAL;
        s_battery_status.throttling_active = true;
    }

    /* Calculate rate of change delta */
    int16_t diff_mv = (int16_t)vbat_mv - (int16_t)s_prev_vbat_mv;
    s_battery_status.delta_vbat_mv_per_hr = diff_mv;
    s_prev_vbat_mv = vbat_mv;

    /* Classify solar harvesting status */
    if (ambient_lux < POWER_BATTERY_NIGHT_LUX_THRESHOLD) {
        s_battery_status.solar_status = SOLAR_STATUS_NIGHT;
    } else if (vbat_mv >= POWER_BATTERY_FLOAT_THRESHOLD_MV) {
        s_battery_status.solar_status = SOLAR_STATUS_FLOAT_CHARGED;
    } else if (diff_mv > 0 && ambient_lux >= POWER_BATTERY_HARVEST_LUX_THRESHOLD) {
        s_battery_status.solar_status = SOLAR_STATUS_ACTIVE_HARVEST;
    } else {
        s_battery_status.solar_status = SOLAR_STATUS_DISCHARGING;
    }

    return STATUS_OK;
}

const power_battery_status_t* power_mgr_battery_get_status(void) {
    return &s_battery_status;
}

power_battery_health_t power_mgr_battery_get_health(void) {
    return s_battery_status.health;
}

bool power_mgr_battery_is_throttling_required(void) {
    return s_battery_status.throttling_active;
}

uint32_t power_mgr_battery_get_recommended_sleep_sec(uint32_t nominal_sleep_sec) {
    if (s_battery_status.health == POWER_BATTERY_HEALTH_CRITICAL) {
        return POWER_MGR_CRITICAL_BAT_SLEEP_SEC;
    } else if (s_battery_status.health == POWER_BATTERY_HEALTH_LOW) {
        return (nominal_sleep_sec < POWER_MGR_LOW_BAT_SLEEP_SEC) ? POWER_MGR_LOW_BAT_SLEEP_SEC : nominal_sleep_sec;
    }
    return nominal_sleep_sec;
}

uint8_t power_mgr_battery_encode_payload_byte(bool sensor_error, bool unexpected_reset) {
    uint16_t vbat_mv = s_battery_status.vbat_mv;
    uint8_t raw_vbat_6bit = 0;

    if (vbat_mv <= 2500U) {
        raw_vbat_6bit = 0U;
    } else if (vbat_mv >= 3760U) {
        raw_vbat_6bit = 63U;
    } else {
        raw_vbat_6bit = (uint8_t)((vbat_mv - 2500U) / 20U);
        if (raw_vbat_6bit > 63U) {
            raw_vbat_6bit = 63U;
        }
    }

    uint8_t byte11 = raw_vbat_6bit & 0x3FU;
    if (sensor_error) {
        byte11 |= (uint8_t)(1U << 6);
    }
    if (unexpected_reset) {
        byte11 |= (uint8_t)(1U << 7);
    }

    return byte11;
}

