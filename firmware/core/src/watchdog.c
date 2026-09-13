/**
 * @file    watchdog.c
 * @brief   Implementation of STM32WLE5 Independent Watchdog (IWDG) Controller.
 * @details Configures dedicated 32 kHz LSI clocking, /64 prescaler, 8.0s timeout down-counter,
 *          anti-masking refresh logic, boot reset reason inspection, and Stop 2 sleep freeze.
 */

#include "watchdog.h"

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

static IWDG_HandleTypeDef s_hiwdg;
static bool               s_is_enabled     = false;
static bool               s_reset_by_iwdg  = false;
static uint32_t           s_timeout_ms     = WATCHDOG_TIMEOUT_MS_DEFAULT;

status_t watchdog_init(uint32_t timeout_ms) {
    if (timeout_ms == 0U) {
        timeout_ms = WATCHDOG_TIMEOUT_MS_DEFAULT;
    }

    if (timeout_ms < WATCHDOG_TIMEOUT_MS_MIN || timeout_ms > WATCHDOG_TIMEOUT_MS_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* 1. Inspect and capture hardware reset status flags before clearing */
    s_reset_by_iwdg = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET);

    /* 2. Freeze IWDG down-counter during Stop 2 and Standby low-power sleep modes */
    __HAL_DBGMCU_FREEZE_IWDG();

    /* 3. Calculate reload value based on 32 kHz LSI and /64 prescaler (500 Hz / 2.0 ms tick) */
    uint32_t reload = (timeout_ms * (WATCHDOG_LSI_FREQ_HZ / WATCHDOG_PRESCALER_DIV)) / 1000UL;
    if (reload > 4095UL) {
        reload = 4095UL;
    }

    /* 4. Configure STM32CubeWL IWDG peripheral */
    s_hiwdg.Instance       = IWDG;
    s_hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    s_hiwdg.Init.Reload    = reload;
    s_hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;

    if (HAL_IWDG_Init(&s_hiwdg) != HAL_OK) {
        return STATUS_ERR_GENERIC;
    }

    s_is_enabled = true;
    s_timeout_ms = timeout_ms;

    return STATUS_OK;
}

void watchdog_refresh(void) {
    if (s_is_enabled) {
        HAL_IWDG_Refresh(&s_hiwdg);
    }
}

bool watchdog_was_reset_by_watchdog(void) {
    return s_reset_by_iwdg;
}

void watchdog_clear_reset_flags(void) {
    __HAL_RCC_CLEAR_RESET_FLAGS();
    s_reset_by_iwdg = false;
}

uint32_t watchdog_get_timeout_ms(void) {
    return s_timeout_ms;
}

bool watchdog_is_enabled(void) {
    return s_is_enabled;
}

#else

/* ============================================================================
 * Host Simulation & Unit Testing Backend
 * ============================================================================ */

static bool     s_sim_enabled        = false;
static bool     s_sim_reset_by_iwdg  = false;
static uint32_t s_sim_refresh_count  = 0;
static uint32_t s_sim_timeout_ms     = WATCHDOG_TIMEOUT_MS_DEFAULT;

void watchdog_test_reset(void) {
    s_sim_enabled        = false;
    s_sim_reset_by_iwdg  = false;
    s_sim_refresh_count  = 0;
    s_sim_timeout_ms     = WATCHDOG_TIMEOUT_MS_DEFAULT;
}

uint32_t watchdog_test_get_refresh_count(void) {
    return s_sim_refresh_count;
}

void watchdog_test_set_reset_reason(bool was_reset_by_watchdog) {
    s_sim_reset_by_iwdg = was_reset_by_watchdog;
}

void watchdog_test_set_enabled(bool enabled) {
    s_sim_enabled = enabled;
}

status_t watchdog_init(uint32_t timeout_ms) {
    if (timeout_ms == 0U) {
        timeout_ms = WATCHDOG_TIMEOUT_MS_DEFAULT;
    }

    if (timeout_ms < WATCHDOG_TIMEOUT_MS_MIN || timeout_ms > WATCHDOG_TIMEOUT_MS_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }

    s_sim_timeout_ms = timeout_ms;
    s_sim_enabled = true;

    return STATUS_OK;
}

void watchdog_refresh(void) {
    if (s_sim_enabled) {
        s_sim_refresh_count++;
    }
}

bool watchdog_was_reset_by_watchdog(void) {
    return s_sim_reset_by_iwdg;
}

void watchdog_clear_reset_flags(void) {
    s_sim_reset_by_iwdg = false;
}

uint32_t watchdog_get_timeout_ms(void) {
    return s_sim_timeout_ms;
}

bool watchdog_is_enabled(void) {
    return s_sim_enabled;
}

#endif /* HAVE_STM32WLXX_HAL */
