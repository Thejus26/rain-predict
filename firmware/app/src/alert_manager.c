/**
 * @file    alert_manager.c
 * @brief   Application alert coordinator and visual status LED engine implementation.
 * @details Implements non-blocking state evaluation, priority cascade, and LED flash patterns.
 *
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#include "alert_manager.h"
#include <string.h>

/* ========================================================================== */
/* Internal State Structure                                                   */
/* ========================================================================== */

typedef struct {
    bool                   initialized;
    alert_manager_config_t config;
    alert_led_pattern_t    active_pattern;
    uint32_t               pattern_timer_ms;
    bool                   green_state;
    bool                   red_state;
    uint32_t               total_alerts_imminent;
    uint32_t               total_alerts_warning;
    bool                   is_throttled;
} alert_mgr_ctx_t;

static alert_mgr_ctx_t s_ctx;

/* Static Pattern Name String Lookup */
static const char * const s_pattern_names[ALERT_LED_PATTERN_MAX] = {
    [ALERT_LED_PATTERN_OFF]             = "OFF",
    [ALERT_LED_PATTERN_HEALTHY_PULSE]   = "HEALTHY_GREEN_PULSE",
    [ALERT_LED_PATTERN_WATCH_AMBER]     = "WATCH_AMBER_BLINK",
    [ALERT_LED_PATTERN_WARNING_RED]     = "WARNING_RED_BLINK",
    [ALERT_LED_PATTERN_IMMINENT_STROBE] = "IMMINENT_RED_STROBE",
    [ALERT_LED_PATTERN_ACTIVE_RAIN]     = "ACTIVE_RAIN_DOUBLE_FLASH",
    [ALERT_LED_PATTERN_SYSTEM_FAULT]    = "SYSTEM_FAULT_BEACON",
    [ALERT_LED_PATTERN_CONSERVATION]    = "CONSERVATION_MICRO_PULSE"
};

/* ========================================================================== */
/* Private Helper Functions                                                   */
/* ========================================================================== */

static void alert_mgr_apply_leds(bool green, bool red) {
    s_ctx.green_state = green;
    s_ctx.red_state   = red;

    if (s_ctx.config.enable_visual_leds) {
        bsp_led_set(BSP_LED_GREEN, green);
        bsp_led_set(BSP_LED_RED, red);
    } else {
        bsp_led_set(BSP_LED_GREEN, false);
        bsp_led_set(BSP_LED_RED, false);
    }
}

static alert_led_pattern_t alert_mgr_resolve_pattern(const alert_input_t *p_in) {
    /* Priority 1: Sleep mode requested */
    if (p_in->is_sleeping) {
        return ALERT_LED_PATTERN_OFF;
    }

    /* Priority 2: Battery Critical Tier 3 (< 2.90V) */
    if (p_in->battery_tier == BATTERY_TIER_CRITICAL) {
        return ALERT_LED_PATTERN_OFF;
    }

    /* Priority 3: System hardware fault */
    if (p_in->sensor_fault) {
        return ALERT_LED_PATTERN_SYSTEM_FAULT;
    }

    /* Priority 4: Rain Imminent / Severe Storm Override */
    if (p_in->rain_state == RAIN_ALERT_IMMINENT || p_in->cpi_pct >= 80.0f) {
        return ALERT_LED_PATTERN_IMMINENT_STROBE;
    }

    /* Priority 5: Active rainfall detected by tipping bucket */
    if (p_in->rain_pulses_recent > 0U) {
        return ALERT_LED_PATTERN_ACTIVE_RAIN;
    }

    /* Priority 6: Rain Likely */
    if (p_in->rain_state == RAIN_ALERT_LIKELY || p_in->cpi_pct >= 60.0f) {
        return ALERT_LED_PATTERN_WARNING_RED;
    }

    /* Priority 7: Rain Possible (Amber) */
    if (p_in->rain_state == RAIN_ALERT_POSSIBLE || p_in->cpi_pct >= 30.0f) {
        return ALERT_LED_PATTERN_WATCH_AMBER;
    }

    /* Priority 8: Battery Conservation Tier 2 (2.90V <= Vbat < 3.10V) */
    if (p_in->battery_tier == BATTERY_TIER_CONSERVATION) {
        return ALERT_LED_PATTERN_CONSERVATION;
    }

    /* Priority 9: Quiescent Healthy / Fair Weather */
    return ALERT_LED_PATTERN_HEALTHY_PULSE;
}

/* ========================================================================== */
/* Public API Implementations                                                 */
/* ========================================================================== */

status_t alert_manager_init(const alert_manager_config_t *p_config) {
    (void)memset(&s_ctx, 0, sizeof(s_ctx));

    if (p_config != NULL) {
        s_ctx.config = *p_config;
    } else {
        /* Safe Agronomic Defaults */
        s_ctx.config.enable_visual_leds         = true;
        s_ctx.config.enable_audible_buzzer      = true;
        s_ctx.config.enable_siren_relay         = true;
        s_ctx.config.active_display_duration_ms = 30000U; /* 30 seconds */
    }

    /* Initialize underlying BSP LED and actuator hardware */
    status_t bsp_rc = bsp_indicators_init();
    if (bsp_rc != STATUS_OK) {
        return bsp_rc;
    }

    s_ctx.active_pattern   = ALERT_LED_PATTERN_OFF;
    s_ctx.pattern_timer_ms = 0U;
    s_ctx.green_state      = false;
    s_ctx.red_state        = false;
    s_ctx.initialized      = true;

    return STATUS_OK;
}

status_t alert_manager_update(const alert_input_t *p_input) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    if (p_input == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    alert_led_pattern_t resolved = alert_mgr_resolve_pattern(p_input);

    if (resolved != s_ctx.active_pattern) {
        s_ctx.active_pattern   = resolved;
        s_ctx.pattern_timer_ms = 0U; /* Reset pattern phase */

        if (resolved == ALERT_LED_PATTERN_IMMINENT_STROBE) {
            s_ctx.total_alerts_imminent++;
        } else if (resolved == ALERT_LED_PATTERN_WARNING_RED) {
            s_ctx.total_alerts_warning++;
        }
    }

    s_ctx.is_throttled = (p_input->battery_tier != BATTERY_TIER_NORMAL);

    return STATUS_OK;
}

status_t alert_manager_process_step(uint32_t delta_ms) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    s_ctx.pattern_timer_ms += delta_ms;

    bool green = false;
    bool red   = false;

    switch (s_ctx.active_pattern) {
        case ALERT_LED_PATTERN_OFF:
            green = false;
            red   = false;
            s_ctx.pattern_timer_ms = 0U;
            break;

        case ALERT_LED_PATTERN_HEALTHY_PULSE: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_HEALTHY_PERIOD_MS;
            green = (phase < ALERT_LED_HEALTHY_ON_MS);
            red   = false;
            break;
        }

        case ALERT_LED_PATTERN_WATCH_AMBER: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_WATCH_PERIOD_MS;
            bool on = (phase < ALERT_LED_WATCH_ON_MS);
            green = on;
            red   = on; /* Amber combination */
            break;
        }

        case ALERT_LED_PATTERN_WARNING_RED: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_WARNING_PERIOD_MS;
            green = false;
            red   = (phase < ALERT_LED_WARNING_ON_MS);
            break;
        }

        case ALERT_LED_PATTERN_IMMINENT_STROBE: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_IMMINENT_PERIOD_MS;
            green = false;
            red   = (phase < ALERT_LED_IMMINENT_ON_MS);
            break;
        }

        case ALERT_LED_PATTERN_ACTIVE_RAIN: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_RAIN_PERIOD_MS;
            green = false;
            /* Double-flash: Pulse 1 [0..50ms], Gap [50..100ms], Pulse 2 [100..150ms] */
            if (phase < ALERT_LED_RAIN_PULSE_MS) {
                red = true;
            } else if ((phase >= (ALERT_LED_RAIN_PULSE_MS + ALERT_LED_RAIN_GAP_MS)) &&
                       (phase <  (2U * ALERT_LED_RAIN_PULSE_MS + ALERT_LED_RAIN_GAP_MS))) {
                red = true;
            } else {
                red = false;
            }
            break;
        }

        case ALERT_LED_PATTERN_SYSTEM_FAULT: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_FAULT_PERIOD_MS;
            /* Phases: Green (0..100ms), Red (100..200ms), Green (200..300ms), Red (300..400ms), Off (400..1000ms) */
            if (phase < 100U) {
                green = true;
                red   = false;
            } else if (phase < 200U) {
                green = false;
                red   = true;
            } else if (phase < 300U) {
                green = true;
                red   = false;
            } else if (phase < 400U) {
                green = false;
                red   = true;
            } else {
                green = false;
                red   = false;
            }
            break;
        }

        case ALERT_LED_PATTERN_CONSERVATION: {
            uint32_t phase = s_ctx.pattern_timer_ms % ALERT_LED_CONSERVE_PERIOD_MS;
            green = (phase < ALERT_LED_CONSERVE_ON_MS);
            red   = false;
            break;
        }

        default:
            green = false;
            red   = false;
            break;
    }

    alert_mgr_apply_leds(green, red);
    return STATUS_OK;
}

status_t alert_manager_set_led_pattern(alert_led_pattern_t pattern) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    if (pattern >= ALERT_LED_PATTERN_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }

    s_ctx.active_pattern   = pattern;
    s_ctx.pattern_timer_ms = 0U;

    return alert_manager_process_step(0U);
}

alert_led_pattern_t alert_manager_get_active_led_pattern(void) {
    return s_ctx.active_pattern;
}

status_t alert_manager_force_all_off(void) {
    s_ctx.active_pattern   = ALERT_LED_PATTERN_OFF;
    s_ctx.pattern_timer_ms = 0U;
    alert_mgr_apply_leds(false, false);
    return bsp_indicators_all_off();
}

const char *alert_manager_get_pattern_name(alert_led_pattern_t pattern) {
    if (pattern >= ALERT_LED_PATTERN_MAX) {
        return "UNKNOWN";
    }
    return s_pattern_names[pattern];
}

status_t alert_manager_get_status(alert_manager_status_t *p_status) {
    if (p_status == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    p_status->active_pattern        = s_ctx.active_pattern;
    p_status->green_led_state       = s_ctx.green_state;
    p_status->red_led_state         = s_ctx.red_state;
    p_status->pattern_elapsed_ms    = s_ctx.pattern_timer_ms;
    p_status->total_alerts_imminent = s_ctx.total_alerts_imminent;
    p_status->total_alerts_warning  = s_ctx.total_alerts_warning;
    p_status->is_throttled          = s_ctx.is_throttled;

    return STATUS_OK;
}
