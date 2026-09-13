/**
 * @file    system_clock.h
 * @brief   System clock tree and low-power oscillator manager for STM32WLE5 SoC.
 * @details Configures MSI @ 48 MHz, LSE @ 32.768 kHz, HSE/TCXO @ 32 MHz, and peripheral routing.
 */

#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Clock Frequency Constants
 * ============================================================================ */

#define SYSTEM_CLOCK_SYSCLK_FREQ_HZ     48000000UL  /**< Active SYSCLK frequency: 48 MHz */
#define SYSTEM_CLOCK_HCLK_FREQ_HZ       48000000UL  /**< AHB HCLK frequency: 48 MHz */
#define SYSTEM_CLOCK_PCLK1_FREQ_HZ      48000000UL  /**< APB1 PCLK1 frequency: 48 MHz */
#define SYSTEM_CLOCK_PCLK2_FREQ_HZ      48000000UL  /**< APB2 PCLK2 frequency: 48 MHz */
#define SYSTEM_CLOCK_LSE_FREQ_HZ        32768UL     /**< LSE Quartz frequency: 32.768 kHz */
#define SYSTEM_CLOCK_HSE_RADIO_FREQ_HZ  32000000UL  /**< HSE TCXO Radio frequency: 32 MHz */
#define SYSTEM_CLOCK_LSI_FREQ_HZ        32000UL     /**< LSI Watchdog frequency: 32 kHz */

#define SYSTEM_CLOCK_LSE_TIMEOUT_MS     1000U       /**< Maximum LSE startup timeout in ms */
#define SYSTEM_CLOCK_HSE_TIMEOUT_MS     200U        /**< Maximum HSE/TCXO startup timeout in ms */

/* ============================================================================
 * Public Clock Management API Prototypes
 * ============================================================================ */

/**
 * @brief  Configures the primary system clock tree (MSI @ 48 MHz, LSE @ 32.768 kHz).
 * @return status_t STATUS_OK on success, error code on oscillator fault or timeout.
 */
status_t system_clock_init(void);

/**
 * @brief  Initializes the Low-Speed External (LSE) 32.768 kHz crystal oscillator.
 * @return status_t STATUS_OK on success, STATUS_ERR_TIMEOUT if crystal fails to start.
 */
status_t system_clock_lse_init(void);

/**
 * @brief  Prepares clock tree for Stop 2 deep sleep entry (scales clocks, isolates buses).
 * @return status_t STATUS_OK on success.
 */
status_t system_clock_sleep_prepare(void);

/**
 * @brief  Restores 48 MHz MSI system clocks and Flash latency upon wakeup from Stop 2.
 * @return status_t STATUS_OK on success.
 */
status_t system_clock_wake_restore(void);

/**
 * @brief  Enables or disables the 32 MHz HSE/TCXO oscillator for Sub-GHz LoRa radio operations.
 * @param[in] enable true to start and stabilize HSE TCXO; false to shut down.
 * @return status_t STATUS_OK on success, STATUS_ERR_TIMEOUT if oscillator fails to lock.
 */
status_t system_clock_hse_radio_enable(bool enable);

/**
 * @brief  Returns active SYSCLK frequency in Hz.
 * @return uint32_t frequency in Hz.
 */
uint32_t system_clock_get_sysclk(void);

/**
 * @brief  Returns active AHB HCLK frequency in Hz.
 * @return uint32_t frequency in Hz.
 */
uint32_t system_clock_get_hclk(void);

/**
 * @brief  Returns active APB1 PCLK1 frequency in Hz.
 * @return uint32_t frequency in Hz.
 */
uint32_t system_clock_get_pclk1(void);

/**
 * @brief  Returns active APB2 PCLK2 frequency in Hz.
 * @return uint32_t frequency in Hz.
 */
uint32_t system_clock_get_pclk2(void);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated clock tree state for unit test isolation.
 */
void system_clock_test_reset(void);

/**
 * @brief Injects or clears simulated LSE crystal fault for timeout testing.
 * @param[in] fault true to simulate failed LSE oscillator; false for normal operation.
 */
void system_clock_test_set_lse_fault(bool fault);

/**
 * @brief Injects or clears simulated HSE TCXO oscillator fault for timeout testing.
 * @param[in] fault true to simulate failed HSE oscillator; false for normal operation.
 */
void system_clock_test_set_hse_fault(bool fault);

/**
 * @brief Checks if simulated LSE oscillator is currently ready and stable.
 * @return bool true if LSE is running.
 */
bool system_clock_test_is_lse_ready(void);

/**
 * @brief Checks if simulated HSE TCXO radio oscillator is currently active.
 * @return bool true if HSE radio clock is ON.
 */
bool system_clock_test_is_hse_radio_ready(void);

/**
 * @brief Checks if MSI hardware PLL auto-trim locked to LSE is enabled.
 * @return bool true if PLL auto-calibration active.
 */
bool system_clock_test_is_msi_pll_enabled(void);

/**
 * @brief Gets simulated Flash access latency in wait states.
 * @return uint32_t wait states (e.g. 2 for 48 MHz).
 */
uint32_t system_clock_test_get_flash_latency(void);

/**
 * @brief Checks if Instruction Cache (ICache) is enabled.
 * @return bool true if enabled.
 */
bool system_clock_test_is_icache_enabled(void);

/**
 * @brief Checks if Data Cache (DCache) is enabled.
 * @return bool true if enabled.
 */
bool system_clock_test_is_dcache_enabled(void);

/**
 * @brief Checks if Flash Prefetch buffer is enabled.
 * @return bool true if enabled.
 */
bool system_clock_test_is_prefetch_enabled(void);

/**
 * @brief Gets simulated core power voltage regulator scaling mode.
 * @return uint32_t voltage scale (1 = High performance 1.2V core).
 */
uint32_t system_clock_test_get_voltage_scale(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CLOCK_H */
