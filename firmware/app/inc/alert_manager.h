/**
 * @file    alert_manager.h
 * @brief   Application alert coordinator and visual status LED engine.
 * @details Manages multi-pattern visual indicators, audible alert coordination,
 *          and low-battery preservation overrides for tea plantation monitoring.
 *
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "bsp_indicators.h"
#include "rain_algo.h"
#include "measurement_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility definitions if not provided by status.h */
#ifndef STATUS_ERR_INVALID_ARG
#define STATUS_ERR_INVALID_ARG STATUS_ERR_INVALID_PARAM
#endif

/* ========================================================================== */
/* Constants & Timing Definitions                                             */
/* ========================================================================== */

#define ALERT_LED_HEALTHY_ON_MS         50U     /**< Green pulse ON time (ms) */
#define ALERT_LED_HEALTHY_PERIOD_MS     1000U   /**< Green pulse period (ms) */

#define ALERT_LED_WATCH_ON_MS           250U    /**< Amber blink ON time (ms) */
#define ALERT_LED_WATCH_PERIOD_MS       500U    /**< Amber blink period (ms) */

#define ALERT_LED_WARNING_ON_MS         200U    /**< Red warning ON time (ms) */
#define ALERT_LED_WARNING_PERIOD_MS     400U    /**< Red warning period (ms) */

#define ALERT_LED_IMMINENT_ON_MS        50U     /**< Red strobe ON time (ms) */
#define ALERT_LED_IMMINENT_PERIOD_MS    100U    /**< Red strobe period (ms) */

#define ALERT_LED_RAIN_PULSE_MS         50U     /**< Rain double pulse ON (ms) */
#define ALERT_LED_RAIN_GAP_MS           50U     /**< Rain double pulse gap (ms) */
#define ALERT_LED_RAIN_PERIOD_MS        1000U   /**< Rain double pulse period */

#define ALERT_LED_FAULT_PHASE_MS        100U    /**< Fault phase duration (ms) */
#define ALERT_LED_FAULT_PERIOD_MS       1000U   /**< Fault cycle period (ms) */

#define ALERT_LED_CONSERVE_ON_MS        10U     /**< Conservation micro-pulse ON */
#define ALERT_LED_CONSERVE_PERIOD_MS    5000U   /**< Conservation cycle period */

/* ========================================================================== */
/* Enumerations & Type Definitions                                            */
/* ========================================================================== */

/**
 * @brief  Visual status LED flash pattern identifiers.
 */
typedef enum {
    ALERT_LED_PATTERN_OFF = 0,          /**< All LEDs disabled / dark */
    ALERT_LED_PATTERN_HEALTHY_PULSE,    /**< Green pulse (50ms ON / 950ms OFF) */
    ALERT_LED_PATTERN_WATCH_AMBER,      /**< Amber blink (250ms ON / 250ms OFF) */
    ALERT_LED_PATTERN_WARNING_RED,      /**< Red warning blink (200ms ON / 200ms OFF) */
    ALERT_LED_PATTERN_IMMINENT_STROBE,  /**< Red rapid strobe (50ms ON / 50ms OFF) */
    ALERT_LED_PATTERN_ACTIVE_RAIN,      /**< Red double flash (50ms/50ms/50ms/850ms) */
    ALERT_LED_PATTERN_SYSTEM_FAULT,     /**< Alternating Green/Red error beacon */
    ALERT_LED_PATTERN_CONSERVATION,     /**< Green micro-pulse (10ms ON / 4990ms OFF) */
    ALERT_LED_PATTERN_MAX
} alert_led_pattern_t;

/**
 * @brief  Input operational state evaluated by alert manager.
 */
typedef struct {
    rain_alert_state_t      rain_state;         /**< Classified meteorological alert state */
    float                   cpi_pct;            /**< Composite Precipitation Index (0..100%) */
    uint32_t                rain_pulses_recent; /**< Rain gauge tips in last sampling window */
    battery_throttle_tier_t battery_tier;       /**< Active battery preservation tier */
    bool                    sensor_fault;       /**< True if sensor bus / hardware error */
    bool                    is_sleeping;        /**< True if entering/in low-power sleep */
} alert_input_t;

/**
 * @brief  Alert manager runtime configuration.
 */
typedef struct {
    bool     enable_visual_leds;                /**< Master enable for status LEDs */
    bool     enable_audible_buzzer;             /**< Master enable for piezo buzzer */
    bool     enable_siren_relay;                /**< Master enable for siren relay */
    uint32_t active_display_duration_ms;        /**< Max duration to animate LEDs per active cycle */
} alert_manager_config_t;

/**
 * @brief  Diagnostic status snapshot of the alert coordinator.
 */
typedef struct {
    alert_led_pattern_t active_pattern;         /**< Currently active LED flash pattern */
    bool                green_led_state;        /**< Real-time physical state of Green LED */
    bool                red_led_state;          /**< Real-time physical state of Red LED */
    uint32_t            pattern_elapsed_ms;     /**< Milliseconds into current pattern cycle */
    uint32_t            total_alerts_imminent;  /**< Lifetime count of storm alerts triggered */
    uint32_t            total_alerts_warning;   /**< Lifetime count of warning alerts triggered */
    bool                is_throttled;           /**< True if visual alerts throttled by battery */
} alert_manager_status_t;

/* ========================================================================== */
/* Public Function Prototypes                                                 */
/* ========================================================================== */

/**
 * @brief  Initializes the Alert Manager and configures default operational parameters.
 * @param[in] p_config  Pointer to configuration structure (or NULL for defaults).
 * @return status_t     STATUS_OK on success, or error code.
 */
status_t alert_manager_init(const alert_manager_config_t *p_config);

/**
 * @brief  Updates the alert manager state based on latest meteorological and system inputs.
 * @param[in] p_input   Pointer to current operational inputs.
 * @return status_t     STATUS_OK on success, or error code.
 */
status_t alert_manager_update(const alert_input_t *p_input);

/**
 * @brief  Advances the non-blocking pattern animation timing by elapsed milliseconds.
 * @param[in] delta_ms  Milliseconds elapsed since last execution step.
 * @return status_t     STATUS_OK on success, or error code.
 */
status_t alert_manager_process_step(uint32_t delta_ms);

/**
 * @brief  Directly forces a specific visual LED pattern (manual test or override).
 * @param[in] pattern   Target LED pattern to apply.
 * @return status_t     STATUS_OK on success, or STATUS_ERR_INVALID_PARAM / STATUS_ERR_INVALID_ARG.
 */
status_t alert_manager_set_led_pattern(alert_led_pattern_t pattern);

/**
 * @brief  Retrieves the currently active visual LED pattern.
 * @return alert_led_pattern_t Active pattern enum identifier.
 */
alert_led_pattern_t alert_manager_get_active_led_pattern(void);

/**
 * @brief  Forces all LEDs and alert actuators completely OFF immediately.
 * @return status_t     STATUS_OK on success.
 */
status_t alert_manager_force_all_off(void);

/**
 * @brief  Retrieves human-readable English descriptor for an LED pattern.
 * @param[in] pattern   Pattern enum identifier.
 * @return const char*  Pointer to static flash string.
 */
const char *alert_manager_get_pattern_name(alert_led_pattern_t pattern);

/**
 * @brief  Retrieves real-time diagnostics and status telemetry from the alert coordinator.
 * @param[out] p_status Pointer to destination status structure.
 * @return status_t     STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t alert_manager_get_status(alert_manager_status_t *p_status);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_MANAGER_H */
