/**
 * @file    power_mgr.h
 * @brief   Power management, sleep mode controller, and pre-sleep GPIO conditioning for STM32WLE5.
 * @details Manages pre-sleep GPIO leakage elimination, bus isolation, Stop 2 deep sleep entry,
 *          and post-wake restoration routines for the Tea Plantation Rain Prediction System.
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
 * Power State Definitions
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

/* ============================================================================
 * Public Power Management API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes the power management subsystem, low-power regulator configurations, and backup domain access.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t power_mgr_init(void);

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

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* POWER_MGR_H */
