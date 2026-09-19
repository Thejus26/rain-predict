/**
 * @file    alert_manager.h
 * @brief   Application alert coordinator, visual LED patterns, and acoustic actuation.
 * @details Unified manager for status LEDs, audible piezo buzzer bursts, estate siren
 *          relay triggers, battery preservation overrides, and anti-chatter cooldowns.
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
/* Visual LED Timing Constants                                                */
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
/* Acoustic & Siren Timing Constants                                          */
/* ========================================================================== */

#define ALERT_SIREN_MAX_DURATION_MS     10000U  /**< Maximum siren pulse: 10 seconds */
#define ALERT_SIREN_DEFAULT_DURATION_MS 10000U  /**< Default storm siren pulse: 10s */
#define ALERT_SIREN_COOLDOWN_SEC        1800U   /**< Siren cooldown window: 30 minutes */

#define ALERT_BUZZER_CHIRP_MS           50U     /**< Short chirp duration (ms) */
#define ALERT_BUZZER_DOUBLE_PULSE_MS    100U    /**< Double chirp pulse width (ms) */
#define ALERT_BUZZER_DOUBLE_GAP_MS      100U    /**< Double chirp pulse gap (ms) */
#define ALERT_BUZZER_BURST_CADENCE_MS   200U    /**< Storm burst toggle rate (ms) */
#define ALERT_BUZZER_FAULT_ON_MS        500U    /**< Fault beep ON time (ms) */
#define ALERT_BUZZER_FAULT_PERIOD_MS    2000U   /**< Fault beep cycle period (ms) */

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
 * @brief  Audible piezoelectric buzzer sound pattern identifiers.
 */
typedef enum {
    ALERT_BUZZER_PATTERN_OFF = 0,       /**< Buzzer silent */
    ALERT_BUZZER_PATTERN_SHORT_CHIRP,   /**< Single 50ms chirp */
    ALERT_BUZZER_PATTERN_DOUBLE_CHIRP,  /**< Double 100ms chirp */
    ALERT_BUZZER_PATTERN_STORM_BURST,   /**< Continuous 200ms toggle burst */
    ALERT_BUZZER_PATTERN_FAULT_BEEP,    /**< Periodic 500ms warning beep */
    ALERT_BUZZER_PATTERN_MAX
} alert_buzzer_pattern_t;

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
    uint8_t                 rtc_hour_0_to_23;   /**< Current 24-hour clock for quiet hours */
} alert_input_t;

/**
 * @brief  Alert manager runtime configuration.
 */
typedef struct {
    bool     enable_visual_leds;                /**< Master enable for status LEDs */
    bool     enable_audible_buzzer;             /**< Master enable for piezo buzzer */
    bool     enable_siren_relay;                /**< Master enable for siren relay */
    bool     enable_night_quiet_hours;          /**< Mute external siren at night */
    uint8_t  quiet_hours_start_hour;            /**< Quiet hours start (default: 20 -> 8 PM) */
    uint8_t  quiet_hours_end_hour;              /**< Quiet hours end (default: 6 -> 6 AM) */
    uint32_t active_display_duration_ms;        /**< Max duration to animate indicators per cycle */
} alert_manager_config_t;

/**
 * @brief  Diagnostic status snapshot of the alert coordinator.
 */
typedef struct {
    alert_led_pattern_t    active_led_pattern;      /**< Currently active LED flash pattern */
    alert_buzzer_pattern_t active_buzzer_pattern;   /**< Currently active buzzer pattern */
    bool                   green_led_state;         /**< Real-time physical state of Green LED */
    bool                   red_led_state;           /**< Real-time physical state of Red LED */
    bool                   buzzer_state;            /**< Real-time physical state of Buzzer */
    bool                   siren_relay_active;      /**< Real-time physical state of Siren Relay */
    uint32_t               siren_remaining_ms;      /**< Milliseconds remaining on active siren pulse */
    uint32_t               siren_cooldown_sec;      /**< Seconds remaining on siren cooldown timer */
    uint32_t               total_siren_activations; /**< Lifetime count of siren relay triggers */
    uint32_t               total_alerts_imminent;   /**< Lifetime count of storm alerts triggered */
    uint32_t               total_alerts_warning;    /**< Lifetime count of warning alerts triggered */
    bool                   is_throttled;            /**< True if acoustic alerts throttled by battery */
} alert_manager_status_t;

/* ========================================================================== */
/* Public Function Prototypes                                                 */
/* ========================================================================== */

/**
 * @brief  Initializes the Alert Manager, clears actuators, and arms default configs.
 * @param[in] p_config  Pointer to configuration structure (or NULL for defaults).
 * @return status_t     STATUS_OK on success, or error code.
 */
status_t alert_manager_init(const alert_manager_config_t *p_config);

/**
 * @brief  Updates alert evaluation based on latest meteorological, battery, and system inputs.
 * @param[in] p_input   Pointer to current operational inputs.
 * @return status_t     STATUS_OK on success, or error code.
 */
status_t alert_manager_update(const alert_input_t *p_input);

/**
 * @brief  Advances non-blocking pattern animation, buzzer cadence, and siren timers by delta_ms.
 * @param[in] delta_ms  Milliseconds elapsed since last execution step.
 * @return status_t     STATUS_OK on success.
 */
status_t alert_manager_process_step(uint32_t delta_ms);

/**
 * @brief  Directly triggers the estate siren relay with hardware/software auto-shutoff.
 * @param[in] duration_ms Pulse duration in ms (clamped to ALERT_SIREN_MAX_DURATION_MS).
 * @return status_t       STATUS_OK on success, STATUS_ERR_BUSY if in cooldown, or error code.
 */
status_t alert_manager_trigger_siren(uint32_t duration_ms);

/**
 * @brief  Directly forces a specific visual LED pattern.
 * @param[in] pattern     Target LED pattern.
 * @return status_t       STATUS_OK on success, or STATUS_ERR_INVALID_ARG / STATUS_ERR_INVALID_PARAM.
 */
status_t alert_manager_set_led_pattern(alert_led_pattern_t pattern);

/**
 * @brief  Directly forces a specific audible buzzer pattern.
 * @param[in] pattern     Target buzzer pattern.
 * @return status_t       STATUS_OK on success, or STATUS_ERR_INVALID_ARG / STATUS_ERR_INVALID_PARAM.
 */
status_t alert_manager_set_buzzer_pattern(alert_buzzer_pattern_t pattern);

/**
 * @brief  Retrieves the currently active visual LED pattern.
 * @return alert_led_pattern_t Active pattern enum.
 */
alert_led_pattern_t alert_manager_get_active_led_pattern(void);

/**
 * @brief  Retrieves the currently active audible buzzer pattern.
 * @return alert_buzzer_pattern_t Active buzzer pattern enum.
 */
alert_buzzer_pattern_t alert_manager_get_active_buzzer_pattern(void);

/**
 * @brief  Checks whether the estate siren relay is currently energized.
 * @return bool True if siren relay is ON.
 */
bool alert_manager_is_siren_active(void);

/**
 * @brief  Retrieves the remaining siren anti-chatter cooldown duration in seconds.
 * @return uint32_t Seconds remaining (0 if ready to fire).
 */
uint32_t alert_manager_get_siren_cooldown_remaining_sec(void);

/**
 * @brief  Forces all visual LEDs, piezo buzzer, and siren relay completely OFF immediately.
 * @return status_t STATUS_OK on success.
 */
status_t alert_manager_force_all_off(void);

/**
 * @brief  Retrieves human-readable English descriptor for an LED pattern.
 * @param[in] pattern   Pattern enum.
 * @return const char*  Static string descriptor.
 */
const char *alert_manager_get_pattern_name(alert_led_pattern_t pattern);

/**
 * @brief  Retrieves real-time diagnostics snapshot from the alert manager.
 * @param[out] p_status Destination status struct pointer.
 * @return status_t     STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t alert_manager_get_status(alert_manager_status_t *p_status);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_MANAGER_H */
