/**
 * @file    bsp_indicators.h
 * @brief   Board indicator and alarm actuator driver for STM32WLE5 SoC.
 * @details Non-blocking controller for Status LEDs (PB8/PB9), Piezo Buzzer (PB2), and Siren Relay (PB4).
 */

#ifndef BSP_INDICATORS_H
#define BSP_INDICATORS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "board_config.h"

/* ============================================================================
 * Indicator Types & Pattern Definitions
 * ============================================================================ */

/**
 * @brief Status LED channel identifiers.
 */
typedef enum {
    BSP_LED_GREEN = 0,   /**< Status LED Green (PB8) */
    BSP_LED_RED   = 1,   /**< Warning LED Red (PB9) */
    BSP_LED_MAX
} bsp_led_t;

/**
 * @brief Autonomous visual and audible indicator pattern presets.
 */
typedef enum {
    BSP_INDICATOR_PATTERN_OFF = 0,      /**< All indicators silenced and dark */
    BSP_INDICATOR_PATTERN_HEARTBEAT,    /**< Green pulse (50ms) every 5 seconds */
    BSP_INDICATOR_PATTERN_WATCH,        /**< Amber slow blink (500ms ON / 500ms OFF) + chirp */
    BSP_INDICATOR_PATTERN_WARNING,      /**< Red medium blink (200ms ON / 200ms OFF) + double chirp */
    BSP_INDICATOR_PATTERN_STORM_ALERT   /**< Red rapid strobe (100ms ON/OFF) + buzzer strobe */
} bsp_indicator_pattern_t;

/** Maximum allowed siren relay on-time: 10,000 ms (10 seconds) */
#define BSP_RELAY_MAX_PULSE_DURATION_MS     10000U

/** Default alert siren pulse duration: 5,000 ms (5 seconds) */
#define BSP_RELAY_DEFAULT_PULSE_MS          5000U

/* ============================================================================
 * Public Driver API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes indicator GPIOs (PB8, PB9, PB2, PB4) and clears all active states.
 * @return status_t STATUS_OK on success.
 */
status_t bsp_indicators_init(void);

/**
 * @brief  Directly controls the state of a status LED.
 * @param[in] led   LED identifier (BSP_LED_GREEN or BSP_LED_RED).
 * @param[in] state true for ON, false for OFF.
 */
void bsp_led_set(bsp_led_t led, bool state);

/**
 * @brief  Toggles the state of a status LED.
 * @param[in] led LED identifier (BSP_LED_GREEN or BSP_LED_RED).
 */
void bsp_led_toggle(bsp_led_t led);

/**
 * @brief  Reads the current state of a status LED.
 * @param[in] led LED identifier.
 * @return bool   true if illuminated, false if dark or invalid.
 */
bool bsp_led_get(bsp_led_t led);

/**
 * @brief  Directly drives the piezoelectric buzzer gate.
 * @param[in] state true to sound buzzer, false to silence.
 */
void bsp_buzzer_set(bool state);

/**
 * @brief  Reads the current state of the piezoelectric buzzer gate.
 * @return bool true if buzzer is active.
 */
bool bsp_buzzer_get(void);

/**
 * @brief  Directly controls the alert relay gate.
 * @param[in] state true to energize relay, false to de-energize.
 */
void bsp_relay_set(bool state);

/**
 * @brief  Reads the current state of the alert relay gate.
 * @return bool true if relay is energized.
 */
bool bsp_relay_get(void);

/**
 * @brief  Energizes alert relay for a timed duration with automatic safety shutoff.
 * @param[in] duration_ms Pulse duration in ms (clamped to BSP_RELAY_MAX_PULSE_DURATION_MS).
 * @return status_t STATUS_OK on success.
 */
status_t bsp_relay_trigger_timed(uint32_t duration_ms);

/**
 * @brief  Configures the active non-blocking indicator pattern.
 * @param[in] pattern Desired indicator operational pattern.
 * @return status_t STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid pattern.
 */
status_t bsp_indicator_set_pattern(bsp_indicator_pattern_t pattern);

/**
 * @brief  Queries the currently active indicator pattern.
 * @return bsp_indicator_pattern_t Active pattern.
 */
bsp_indicator_pattern_t bsp_indicator_get_pattern(void);

/**
 * @brief  Non-blocking periodic update routine for pattern animation and relay timeout.
 * @param[in] delta_ms Time elapsed in milliseconds since last update call.
 */
void bsp_indicators_process(uint32_t delta_ms);

/**
 * @brief  De-energizes all indicators and alert actuators prior to deep sleep entry.
 * @return status_t STATUS_OK on success.
 */
status_t bsp_indicators_all_off(void);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated actuator states, timers, and active patterns.
 */
void bsp_indicators_test_reset(void);

/**
 * @brief Gets remaining time on the simulated timed relay auto-cutoff timer.
 * @return uint32_t Remaining milliseconds.
 */
uint32_t bsp_indicators_test_get_relay_timer_ms(void);

/**
 * @brief Gets accumulated pattern animation timer.
 * @return uint32_t Accumulated milliseconds.
 */
uint32_t bsp_indicators_test_get_pattern_timer_ms(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* BSP_INDICATORS_H */
