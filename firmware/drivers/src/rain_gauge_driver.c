/**
 * @file    rain_gauge_driver.c
 * @brief   Tipping-bucket rain gauge driver implementation for STM32WLE5 SoC.
 * @details Handles EXTI0 GPIO falling-edge interrupts, dual-stage chatter rejection,
 *          and low-power Stop 2 deep sleep wakeup support.
 */

#include "rain_gauge_driver.h"
#include "board_config.h"

#if defined(HAVE_STM32WLXX_HAL)
#include "stm32wlxx_hal.h"
#endif

/* ========================================================================== */
/* Private Static Driver State                                                */
/* ========================================================================== */

static volatile uint32_t s_last_pulse_timestamp_ms = 0U;
static volatile uint32_t s_rejected_bounce_count    = 0U;
static volatile bool     s_rain_active_flag         = false;
static volatile bool     s_has_first_pulse          = false;
static bool              s_is_initialized           = false;
static bool              s_irq_enabled              = false;

/* ========================================================================== */
/* Driver Lifecycle & Configuration                                           */
/* ========================================================================== */

status_t rain_gauge_init(void) {
#if defined(HAVE_STM32WLXX_HAL)
    /* 1. Enable GPIOA Peripheral Clock */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2. Configure PA0 as Falling-Edge EXTI Input with Pull-Up */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin   = PIN_RAIN_GAUGE_PIN;
    gpio_init.Mode  = GPIO_MODE_IT_FALLING;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_RAIN_GAUGE_PORT, &gpio_init);

    /* 3. Configure NVIC for EXTI0 */
    HAL_NVIC_SetPriority((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn,
                         RAIN_GAUGE_NVIC_PREEMPT_PRIO,
                         RAIN_GAUGE_NVIC_SUB_PRIO);
    HAL_NVIC_EnableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);
#endif

    s_last_pulse_timestamp_ms = 0U;
    s_rejected_bounce_count    = 0U;
    s_rain_active_flag         = false;
    s_has_first_pulse          = false;
    s_is_initialized           = true;
    s_irq_enabled              = true;

    return STATUS_OK;
}

status_t rain_gauge_deinit(void) {
#if defined(HAVE_STM32WLXX_HAL)
    /* 1. Disable NVIC Interrupt */
    HAL_NVIC_DisableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);

    /* 2. Tri-state PA0 to Analog mode to eliminate deep sleep leakage */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin   = PIN_RAIN_GAUGE_PIN;
    gpio_init.Mode  = GPIO_MODE_ANALOG;
    gpio_init.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(PIN_RAIN_GAUGE_PORT, &gpio_init);
#endif

    s_is_initialized = false;
    s_irq_enabled    = false;

    return STATUS_OK;
}

status_t rain_gauge_enable_irq(bool enable) {
    if (!s_is_initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

#if defined(HAVE_STM32WLXX_HAL)
    if (enable) {
        HAL_NVIC_EnableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);
    } else {
        HAL_NVIC_DisableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);
    }
#endif

    s_irq_enabled = enable;
    return STATUS_OK;
}

/* ========================================================================== */
/* Interrupt Service Routine & Debounce Filter                                */
/* ========================================================================== */

void rain_gauge_exti_isr(uint32_t current_tick_ms) {
    if (!s_is_initialized) {
        return;
    }

    if (!s_has_first_pulse) {
        /* First registered physical bucket tip */
        s_last_pulse_timestamp_ms = current_tick_ms;
        s_rain_active_flag         = true;
        s_has_first_pulse          = true;
        return;
    }

    /* 32-bit unsigned arithmetic handles SysTick timer rollover seamlessly */
    uint32_t elapsed_ms = current_tick_ms - s_last_pulse_timestamp_ms;

    if (elapsed_ms >= RAIN_GAUGE_DEBOUNCE_MS) {
        /* Valid physical bucket tip (>= 50ms lockout interval) */
        s_last_pulse_timestamp_ms = current_tick_ms;
        s_rain_active_flag         = true;
    } else {
        /* Spurious contact bounce / chatter spike (< 50ms) rejected */
        s_rejected_bounce_count++;
    }
}

/* ========================================================================== */
/* Diagnostics & Telemetry Accessors                                          */
/* ========================================================================== */

bool rain_gauge_is_rain_active(void) {
    return s_rain_active_flag;
}

uint32_t rain_gauge_get_last_pulse_timestamp(void) {
    return s_last_pulse_timestamp_ms;
}

uint32_t rain_gauge_get_rejected_bounce_count(void) {
    return s_rejected_bounce_count;
}

void rain_gauge_reset_diagnostics(void) {
    s_rejected_bounce_count = 0U;
    s_rain_active_flag         = false;
}
