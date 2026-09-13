/**
 * @file    bsp_power_rails.h
 * @brief   Switched sensor power rail driver for STM32WLE5 SoC.
 * @details High-side P-MOSFET load switch management with calibrated RC stabilization guard timing.
 */

#ifndef BSP_POWER_RAILS_H
#define BSP_POWER_RAILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Power Rail Identifiers & Timing Constants
 * ============================================================================ */

/**
 * @brief Switched power rail identifiers.
 */
typedef enum {
    BSP_POWER_RAIL_SENSORS    = 0,  /**< Switched 3.3V Sensor Rail (PA4 / VSENS_SW) */
    BSP_POWER_RAIL_VBAT_SENSE = 1,  /**< Battery Divider Sense Gate (PB1 / VBAT_DIV_EN) */
    BSP_POWER_RAIL_MAX
} bsp_power_rail_t;

/** Sensor rail RC stabilization delay in milliseconds */
#define BSP_POWER_RAIL_SENSORS_STABILIZE_MS     20U

/** Battery voltage divider RC stabilization delay in milliseconds */
#define BSP_POWER_RAIL_VBAT_STABILIZE_MS        2U

/** Rail bleeder capacitor discharge delay in milliseconds */
#define BSP_POWER_RAIL_DISCHARGE_DELAY_MS       10U

/* ============================================================================
 * Public Driver API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes power rail control GPIOs to default safe power-off states.
 * @details Sets PA4 LOW (Sensor rail OFF) and PB1 HIGH (Divider OFF).
 * @return status_t STATUS_OK on success.
 */
status_t bsp_power_rails_init(void);

/**
 * @brief  Energizes or de-energizes the specified hardware power rail.
 * @param[in] rail   Power rail identifier (BSP_POWER_RAIL_SENSORS or BSP_POWER_RAIL_VBAT_SENSE).
 * @param[in] enable true to energize rail with stabilization delay; false to de-energize.
 * @return status_t  STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid rail ID.
 */
status_t bsp_power_rail_enable(bsp_power_rail_t rail, bool enable);

/**
 * @brief  Checks whether a specified power rail is currently energized.
 * @param[in] rail Power rail identifier.
 * @return bool    true if rail is energized, false if disabled or invalid rail.
 */
bool bsp_power_rail_is_enabled(bsp_power_rail_t rail);

/**
 * @brief  De-energizes all switched power rails prior to Stop 2 deep sleep entry.
 * @return status_t STATUS_OK on success.
 */
status_t bsp_power_rails_all_off(void);

/**
 * @brief  Executes calibrated hardware stabilization guard delay for specified rail.
 * @param[in] rail Power rail identifier.
 * @return status_t STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid rail.
 */
status_t bsp_power_rail_stabilize(bsp_power_rail_t rail);

/**
 * @brief  Returns required stabilization guard delay in milliseconds.
 * @param[in] rail Power rail identifier.
 * @return uint32_t Delay in milliseconds (20 ms for sensors, 2 ms for vbat, 0 on invalid).
 */
uint32_t bsp_power_rail_get_stabilization_ms(bsp_power_rail_t rail);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated power rail driver states and delay metrics for unit testing.
 */
void bsp_power_rails_test_reset(void);

/**
 * @brief Returns total number of times hardware stabilization delay was invoked.
 * @return uint32_t Delay invocation count.
 */
uint32_t bsp_power_rails_test_get_delay_calls(void);

/**
 * @brief Returns duration in milliseconds of most recent stabilization delay call.
 * @return uint32_t Last delay duration in ms.
 */
uint32_t bsp_power_rails_test_get_last_delay_ms(void);

/**
 * @brief Returns raw simulated GPIO pin output level for the specified rail.
 * @param[in] rail Power rail identifier.
 * @return bool Raw pin logic level (true = HIGH, false = LOW).
 */
bool bsp_power_rails_test_get_raw_pin_state(bsp_power_rail_t rail);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* BSP_POWER_RAILS_H */
