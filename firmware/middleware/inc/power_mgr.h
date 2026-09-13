/**
 * @file    power_mgr.h
 * @brief   Ultra-low-power Stop 2 sleep manager and RTC wake controller for STM32WLE5 SoC.
 * @details Manages pre-sleep GPIO leakage elimination, bus isolation, Stop 2 deep sleep entry (< 3.0 uA),
 *          hardware RTC periodic wakeup timing (EXTI19), multi-source wake detection,
 *          sub-5 us clock restoration, cumulative sleep tracking, and shelf-storage Standby mode.
 */

#ifndef POWER_MGR_H
#define POWER_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Power State & Wakeup Source Definitions
 * ============================================================================ */

/**
 * @brief System Power Management States.
 */
typedef enum {
    POWER_STATE_RUN = 0,    /**< Active 48 MHz execution mode (sensors sampling, nowcasting) */
    POWER_STATE_LP_RUN,     /**< Low-power run mode (clock throttled to 2 MHz) */
    POWER_STATE_STOP2,      /**< Stop 2 deep sleep mode (< 3.0 uA, SRAM retained, RTC armed) */
    POWER_STATE_STANDBY     /**< Standby shelf-storage mode (< 0.8 uA) */
} power_state_t;

/**
 * @brief Stop 2 Deep Sleep Wakeup Source Triggers.
 */
typedef enum {
    POWER_WAKE_REASON_UNKNOWN = 0,  /**< Reset, power-on, or unidentified wake trigger */
    POWER_WAKE_REASON_RTC,          /**< RTC Periodic Measurement Wakeup Timer (EXTI line 19) */
    POWER_WAKE_REASON_RAIN_EXTI,    /**< Tipping-Bucket Rain Gauge Pulse on PA0 (EXTI line 0) */
    POWER_WAKE_REASON_BUTTON        /**< User Field Diagnostic Push-Button on PC13 (EXTI line 13) */
} power_wake_reason_t;

/* ============================================================================
 * Sleep Interval Timing Constants (in seconds)
 * ============================================================================ */

#define POWER_MGR_DEFAULT_SLEEP_SEC     600U    /**< Default normal fair-weather sampling: 10 min (600s) */
#define POWER_MGR_WATCH_SLEEP_SEC       300U    /**< Unsettled weather watch sampling: 5 min (300s) */
#define POWER_MGR_STORM_SLEEP_SEC       120U    /**< Active convective storm alert sampling: 2 min (120s) */
#define POWER_MGR_LOW_BAT_SLEEP_SEC     1800U   /**< Low battery preservation sampling: 30 min (1800s) */
#define POWER_MGR_MIN_SLEEP_SEC         1U      /**< Minimum configurable RTC sleep interval: 1s */
#define POWER_MGR_MAX_SLEEP_SEC         65535U  /**< Maximum 16-bit RTC WUT interval: 65535s (~18.2 hours) */

/* ============================================================================
 * Public Power Management API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes power manager, low-power voltage regulator, backup domain access, and retention.
 * @details Configures Ultra-Low-Power mode, backup domain write access, full SRAM1/SRAM2 retention,
 *          and Flash deep power-down during Stop 2 sleep.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t power_mgr_init(void);

/**
 * @brief  Enters Stop 2 ultra-low-power deep sleep mode with automatic RTC timer wakeup.
 * @details Sequentially isolates all GPIOs, configures clock tree for wake, arms RTC periodic wakeup timer,
 *          clears pending interrupt flags, halts core via WFI, restores clocks and GPIO mux upon wake,
 *          and logs cumulative sleep metrics.
 * @param[in] sleep_duration_sec Sleep interval in seconds (1 to 65535).
 * @return status_t STATUS_OK upon waking from sleep, STATUS_ERR_INVALID_PARAM if duration is out of range.
 */
status_t power_mgr_enter_stop2(uint32_t sleep_duration_sec);

/**
 * @brief  Arms the hardware RTC Periodic Wakeup Timer counter with a specified second interval.
 * @details Configures RTC_WUTR on EXTI Line 19 with 1 Hz ck_spre clock source.
 * @param[in] interval_sec Wakeup period in seconds (1 to 65535).
 * @return status_t STATUS_OK on success, STATUS_ERR_INVALID_PARAM if interval is 0 or > 65535.
 */
status_t power_mgr_set_rtc_wakeup(uint32_t interval_sec);

/**
 * @brief  Disables and cancels any pending RTC periodic wakeup timer.
 * @return status_t STATUS_OK on success.
 */
status_t power_mgr_cancel_rtc_wakeup(void);

/**
 * @brief  Identifies and returns the event source that caused the last wakeup from Stop 2 deep sleep.
 * @return power_wake_reason_t Wakeup event trigger (RTC, RAIN_EXTI, BUTTON, UNKNOWN).
 */
power_wake_reason_t power_mgr_get_wake_reason(void);

/**
 * @brief  Restores 48 MHz MSI clocks, Flash latency (2 wait states), and peripheral GPIO multiplexing post-wake.
 * @details Restores MSI clock tree, re-enables LSE auto-calibration, restores GPIO AF multiplexing,
 *          and identifies/clears wakeup interrupt flags.
 * @return status_t STATUS_OK on success.
 */
status_t power_mgr_wake_restore(void);

/**
 * @brief  Returns cumulative seconds the system has spent in Stop 2 deep sleep since boot.
 * @return uint32_t Cumulative sleep duration in seconds.
 */
uint32_t power_mgr_get_total_sleep_time_sec(void);

/**
 * @brief  Resets cumulative deep sleep duration counter to zero.
 */
void power_mgr_reset_total_sleep_time(void);

/**
 * @brief  Enters ultra-low-leakage Standby mode (< 0.8 uA) for long-term shelf storage.
 * @details Tri-states all GPIOs and shuts down core/SRAM retention. Requires NRST pin reset to wake.
 * @return status_t Does not return on embedded target if successful; STATUS_OK on simulation.
 */
status_t power_mgr_enter_standby(void);

/**
 * @brief  Conditions all GPIO ports for ultra-low-leakage Stop 2 deep sleep (< 3.0 uA).
 * @details De-energizes switched rails, silences actuators, isolates digital sensor communication
 *          buses, and switches unrouted pins to Analog No-Pull while preserving wakeup sources.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t power_mgr_gpio_sleep_prepare(void);

/**
 * @brief  Restores operational GPIO pin multiplexing and active states upon wake from Stop 2.
 * @details Restores diagnostic inputs and peripheral communication pin alternate function assignments.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t power_mgr_gpio_wake_restore(void);

/**
 * @brief  Isolates I2C1, USART1, LPUART1, and SPI1 sensor communication pins to GPIO_MODE_ANALOG (No-Pull).
 * @details Eliminates parasitic back-powering into unpowered sensor ICs via ESD protection diodes when
 *          switched power rail VSENS_SW is de-energized (0V).
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t power_mgr_isolate_sensor_buses(void);

/**
 * @brief  Restores I2C1, USART1, LPUART1, and SPI1 communication pins to Alternate Function modes.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t power_mgr_restore_sensor_buses(void);

/**
 * @brief  Diagnostic helper verifying no unexempt pins remain floating and power rail switches are safely de-energized.
 * @return status_t STATUS_OK if all pins comply with leakage rules, STATUS_ERR_INVALID_STATE otherwise.
 */
status_t power_mgr_verify_leakage_state(void);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated power manager state for unit test isolation.
 */
void power_mgr_test_reset(void);

/**
 * @brief Gets current simulated power manager state.
 * @return power_state_t Current power state.
 */
power_state_t power_mgr_test_get_state(void);

/**
 * @brief Sets simulated power state.
 * @param[in] state Power state to assign.
 */
void power_mgr_test_set_state(power_state_t state);

/**
 * @brief Overrides simulated last wakeup reason.
 * @param[in] reason Wake reason to assign.
 */
void power_mgr_test_set_wake_reason(power_wake_reason_t reason);

/**
 * @brief Injects the next wakeup event to be returned upon wake restoration.
 * @param[in] reason Wake reason trigger to inject.
 */
void power_mgr_test_inject_wake_event(power_wake_reason_t reason);

/**
 * @brief Gets the configured simulated RTC wakeup interval in seconds.
 * @return uint32_t RTC interval in seconds.
 */
uint32_t power_mgr_test_get_rtc_wakeup_interval(void);

/**
 * @brief Checks if the simulated RTC wakeup timer is currently armed.
 * @return bool true if RTC wakeup timer is active.
 */
bool power_mgr_test_is_rtc_wakeup_armed(void);

/**
 * @brief Gets total number of Stop 2 sleep cycles executed during test simulation.
 * @return uint32_t Sleep cycle count.
 */
uint32_t power_mgr_test_get_sleep_cycle_count(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* POWER_MGR_H */
