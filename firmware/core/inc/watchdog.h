/**
 * @file    watchdog.h
 * @brief   Independent Watchdog (IWDG) driver and system supervisor for STM32WLE5 SoC.
 * @details Manages hardware watchdog initialization (8.0s timeout), safe anti-masking refresh logic,
 *          boot reset reason diagnostics (RCC_CSR), and automatic Stop 2 deep sleep counter freeze.
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Watchdog Timing Constants
 * ============================================================================ */

#define WATCHDOG_TIMEOUT_MS_DEFAULT     8000UL      /**< Default hardware timeout: 8.0 seconds */
#define WATCHDOG_TIMEOUT_MS_MIN         100UL       /**< Minimum configurable timeout: 100 ms */
#define WATCHDOG_TIMEOUT_MS_MAX         32768UL     /**< Maximum timeout with /64 prescaler: ~32.7s */
#define WATCHDOG_LSI_FREQ_HZ            32000UL     /**< LSI nominal frequency: 32 kHz */
#define WATCHDOG_PRESCALER_DIV          64UL        /**< Prescaler divider: 64 (500 Hz tick / 2.0 ms) */
#define WATCHDOG_RELOAD_VALUE           4000UL      /**< Reload register value for 8.0s (4000 * 2.0 ms) */

/* ============================================================================
 * Public Watchdog API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes the Independent Watchdog (IWDG) with an 8.0-second hardware timeout.
 * @details Enables LSI oscillator, configures /64 prescaler and 4000 reload counts, captures boot
 *          reset reason flags, and enables Stop 2 / Standby deep sleep counter freeze via DBGMCU.
 * @param[in] timeout_ms Desired timeout in ms (default 8000 ms; 0 defaults to WATCHDOG_TIMEOUT_MS_DEFAULT).
 * @return status_t      STATUS_OK on success, STATUS_ERR_INVALID_PARAM if timeout_ms exceeds maximum.
 */
status_t watchdog_init(uint32_t timeout_ms);

/**
 * @brief  Reloads the IWDG down-counter to prevent system reset (kicks the watchdog).
 * @details Writes key 0xAAAA to IWDG_KR register. Must only be called from defined application
 *          checkpoints, never inside Interrupt Service Routines (ISRs).
 */
void watchdog_refresh(void);

/**
 * @brief  Checks whether the last system reset was caused by an IWDG timeout.
 * @details Inspects the RCC_CSR_IWDGRSTF flag captured at system boot.
 * @return bool true if previous reset was triggered by watchdog expiration.
 */
bool watchdog_was_reset_by_watchdog(void);

/**
 * @brief  Clears hardware reset status flags in RCC_CSR.
 */
void watchdog_clear_reset_flags(void);

/**
 * @brief  Returns active configured watchdog timeout in milliseconds.
 * @return uint32_t Timeout in milliseconds (nominally 8000 ms).
 */
uint32_t watchdog_get_timeout_ms(void);

/**
 * @brief  Returns whether the watchdog is active and supervising the system.
 * @return bool true if watchdog is armed and running.
 */
bool watchdog_is_enabled(void);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated watchdog state for unit test isolation.
 */
void watchdog_test_reset(void);

/**
 * @brief Gets total number of watchdog refreshes executed.
 * @return uint32_t Refresh count.
 */
uint32_t watchdog_test_get_refresh_count(void);

/**
 * @brief Sets simulated boot reset reason for test verification.
 * @param[in] was_reset_by_watchdog true to simulate an IWDG timeout reboot.
 */
void watchdog_test_set_reset_reason(bool was_reset_by_watchdog);

/**
 * @brief Sets simulated enabled state.
 * @param[in] enabled true to mark watchdog as armed.
 */
void watchdog_test_set_enabled(bool enabled);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_H */
