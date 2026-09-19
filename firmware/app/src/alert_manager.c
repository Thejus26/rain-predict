/**
 * @file    alert_manager.c
 * @brief   Application alert coordinator, visual LED patterns, and acoustic actuation implementation.
 * @details Implements non-blocking state evaluation, priority cascade, buzzer bursts, and siren relay logic.
 *
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#include "alert_manager.h"
#include <string.h>
#include <math.h>

/* ========================================================================== */
/* Internal State Context                                                     */
/* ========================================================================== */

typedef struct {
    bool                   initialized;
    alert_manager_config_t config;
    alert_led_pattern_t    active_led_pattern;
    alert_buzzer_pattern_t active_buzzer_pattern;
    uint32_t               led_timer_ms;
    uint32_t               buzzer_timer_ms;
    uint32_t               siren_pulse_remaining_ms;
    uint32_t               siren_cooldown_ms;
    bool                   green_state;
    bool                   red_state;
    bool                   buzzer_state;
    bool                   siren_relay_state;
    uint32_t               total_siren_activations;
    uint32_t               total_alerts_imminent;
    uint32_t               total_alerts_warning;
    bool                   is_throttled;
    bool                   actuation_allowed;
} alert_mgr_ctx_t;

static alert_mgr_ctx_t s_ctx;

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

static void alert_mgr_apply_outputs(bool green, bool red, bool buzzer) {
    s_ctx.green_state  = green;
    s_ctx.red_state    = red;
    s_ctx.buzzer_state = buzzer;

    if (s_ctx.config.enable_visual_leds) {
        bsp_led_set(BSP_LED_GREEN, green);
        bsp_led_set(BSP_LED_RED, red);
    } else {
        bsp_led_set(BSP_LED_GREEN, false);
        bsp_led_set(BSP_LED_RED, false);
    }

    if (s_ctx.config.enable_audible_buzzer && s_ctx.actuation_allowed) {
        bsp_buzzer_set(buzzer);
    } else {
        bsp_buzzer_set(false);
    }
}

static bool alert_mgr_is_night_quiet(uint8_t hour) {
    if (!s_ctx.config.enable_night_quiet_hours) {
        return false;
    }
    uint8_t start = s_ctx.config.quiet_hours_start_hour;
    uint8_t end   = s_ctx.config.quiet_hours_end_hour;

    if (start > end) {
        /* Wraps midnight: e.g. 20:00 to 06:00 */
        return (hour >= start || hour < end);
    } else {
        return (hour >= start && hour < end);
    }
}

static alert_led_pattern_t alert_mgr_resolve_led_pattern(const alert_input_t *p_in) {
    if (p_in->is_sleeping || p_in->battery_tier == BATTERY_TIER_CRITICAL) {
        return ALERT_LED_PATTERN_OFF;
    }
    if (p_in->sensor_fault) {
        return ALERT_LED_PATTERN_SYSTEM_FAULT;
    }
    if (p_in->rain_state == RAIN_ALERT_IMMINENT || p_in->cpi_pct >= 80.0f) {
        return ALERT_LED_PATTERN_IMMINENT_STROBE;
    }
    if (p_in->rain_pulses_recent > 0U) {
        return ALERT_LED_PATTERN_ACTIVE_RAIN;
    }
    if (p_in->rain_state == RAIN_ALERT_LIKELY || p_in->cpi_pct >= 60.0f) {
        return ALERT_LED_PATTERN_WARNING_RED;
    }
    if (p_in->rain_state == RAIN_ALERT_POSSIBLE || p_in->cpi_pct >= 30.0f) {
        return ALERT_LED_PATTERN_WATCH_AMBER;
    }
    if (p_in->battery_tier == BATTERY_TIER_CONSERVATION) {
        return ALERT_LED_PATTERN_CONSERVATION;
    }
    return ALERT_LED_PATTERN_HEALTHY_PULSE;
}

static alert_buzzer_pattern_t alert_mgr_resolve_buzzer_pattern(const alert_input_t *p_in) {
    if (p_in->is_sleeping || p_in->battery_tier != BATTERY_TIER_NORMAL) {
        return ALERT_BUZZER_PATTERN_OFF;
    }
    if (p_in->sensor_fault) {
        return ALERT_BUZZER_PATTERN_FAULT_BEEP;
    }
    if (p_in->rain_state == RAIN_ALERT_IMMINENT || p_in->cpi_pct >= 80.0f) {
        return ALERT_BUZZER_PATTERN_STORM_BURST;
    }
    if (p_in->rain_state == RAIN_ALERT_LIKELY || p_in->cpi_pct >= 60.0f) {
        return ALERT_BUZZER_PATTERN_DOUBLE_CHIRP;
    }
    if (p_in->rain_state == RAIN_ALERT_POSSIBLE || p_in->cpi_pct >= 30.0f) {
        return ALERT_BUZZER_PATTERN_SHORT_CHIRP;
    }
    return ALERT_BUZZER_PATTERN_OFF;
}

/* ========================================================================== */
/* Public API Implementations                                                 */
/* ========================================================================== */

status_t alert_manager_init(const alert_manager_config_t *p_config) {
    (void)memset(&s_ctx, 0, sizeof(s_ctx));

    if (p_config != NULL) {
        s_ctx.config = *p_config;
    } else {
        /* Safe Agronomic Plantation Defaults */
        s_ctx.config.enable_visual_leds         = true;
        s_ctx.config.enable_audible_buzzer      = true;
        s_ctx.config.enable_siren_relay         = true;
        s_ctx.config.enable_night_quiet_hours   = true;
        s_ctx.config.quiet_hours_start_hour     = 20U; /* 8 PM */
        s_ctx.config.quiet_hours_end_hour       = 6U;  /* 6 AM */
        s_ctx.config.active_display_duration_ms = 30000U;
    }

    status_t rc = bsp_indicators_init();
    if (rc != STATUS_OK) {
        return rc;
    }

    s_ctx.active_led_pattern    = ALERT_LED_PATTERN_OFF;
    s_ctx.active_buzzer_pattern = ALERT_BUZZER_PATTERN_OFF;
    s_ctx.actuation_allowed     = true;
    s_ctx.initialized           = true;

    return STATUS_OK;
}

status_t alert_manager_trigger_siren(uint32_t duration_ms) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    if (!s_ctx.config.enable_siren_relay || !s_ctx.actuation_allowed) {
        return STATUS_ERR_INVALID_STATE;
    }
    if (s_ctx.siren_cooldown_ms > 0U) {
        return STATUS_ERR_BUSY; /* In cooldown hold-down window */
    }

    /* Clamp pulse duration to 10 seconds maximum */
    uint32_t pulse_ms = (duration_ms > ALERT_SIREN_MAX_DURATION_MS) ?
                         ALERT_SIREN_MAX_DURATION_MS : (duration_ms == 0U ? ALERT_SIREN_DEFAULT_DURATION_MS : duration_ms);

    status_t rc = bsp_relay_trigger_timed(pulse_ms);
    if (rc == STATUS_OK) {
        s_ctx.siren_pulse_remaining_ms = pulse_ms;
        s_ctx.siren_cooldown_ms        = ALERT_SIREN_COOLDOWN_SEC * 1000U;
        s_ctx.siren_relay_state        = true;
        s_ctx.total_siren_activations++;
    }
    return rc;
}

status_t alert_manager_update(const alert_input_t *p_input) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    if (p_input == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    s_ctx.actuation_allowed = (p_input->battery_tier == BATTERY_TIER_NORMAL);
    s_ctx.is_throttled      = !s_ctx.actuation_allowed;

    /* 1. Evaluate Visual LED Pattern */
    alert_led_pattern_t new_led = alert_mgr_resolve_led_pattern(p_input);
    if (new_led != s_ctx.active_led_pattern) {
        s_ctx.active_led_pattern = new_led;
        s_ctx.led_timer_ms       = 0U;

        if (new_led == ALERT_LED_PATTERN_IMMINENT_STROBE) {
            s_ctx.total_alerts_imminent++;
        } else if (new_led == ALERT_LED_PATTERN_WARNING_RED) {
            s_ctx.total_alerts_warning++;
        }
    }

    /* 2. Evaluate Audible Buzzer Pattern */
    alert_buzzer_pattern_t new_buzz = alert_mgr_resolve_buzzer_pattern(p_input);
    if (new_buzz != s_ctx.active_buzzer_pattern) {
        s_ctx.active_buzzer_pattern = new_buzz;
        s_ctx.buzzer_timer_ms       = 0U;
    }

    /* 3. Automatic Siren Triggering on Imminent Storm */
    if (new_led == ALERT_LED_PATTERN_IMMINENT_STROBE &&
        s_ctx.actuation_allowed &&
        !alert_mgr_is_night_quiet(p_input->rtc_hour_0_to_23) &&
        s_ctx.siren_cooldown_ms == 0U) {
        (void)alert_manager_trigger_siren(ALERT_SIREN_DEFAULT_DURATION_MS);
    }

    return STATUS_OK;
}

status_t alert_manager_process_step(uint32_t delta_ms) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    /* 1. Advance Timers */
    s_ctx.led_timer_ms    += delta_ms;
    s_ctx.buzzer_timer_ms += delta_ms;

    if (s_ctx.siren_pulse_remaining_ms > delta_ms) {
        s_ctx.siren_pulse_remaining_ms -= delta_ms;
    } else {
        s_ctx.siren_pulse_remaining_ms = 0U;
        s_ctx.siren_relay_state        = false;
    }

    if (s_ctx.siren_cooldown_ms > delta_ms) {
        s_ctx.siren_cooldown_ms -= delta_ms;
    } else {
        s_ctx.siren_cooldown_ms = 0U;
    }

    /* Also step underlying BSP indicator driver */
    bsp_indicators_process(delta_ms);

    /* 2. Evaluate LED Animation States */
    bool green = false;
    bool red   = false;

    switch (s_ctx.active_led_pattern) {
        case ALERT_LED_PATTERN_OFF:
            green = false;
            red   = false;
            break;
        case ALERT_LED_PATTERN_HEALTHY_PULSE: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_HEALTHY_PERIOD_MS;
            green = (phase < ALERT_LED_HEALTHY_ON_MS);
            red   = false;
            break;
        }
        case ALERT_LED_PATTERN_WATCH_AMBER: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_WATCH_PERIOD_MS;
            bool on = (phase < ALERT_LED_WATCH_ON_MS);
            green = on;
            red   = on;
            break;
        }
        case ALERT_LED_PATTERN_WARNING_RED: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_WARNING_PERIOD_MS;
            green = false;
            red   = (phase < ALERT_LED_WARNING_ON_MS);
            break;
        }
        case ALERT_LED_PATTERN_IMMINENT_STROBE: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_IMMINENT_PERIOD_MS;
            green = false;
            red   = (phase < ALERT_LED_IMMINENT_ON_MS);
            break;
        }
        case ALERT_LED_PATTERN_ACTIVE_RAIN: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_RAIN_PERIOD_MS;
            green = false;
            if (phase < ALERT_LED_RAIN_PULSE_MS ||
                (phase >= (ALERT_LED_RAIN_PULSE_MS + ALERT_LED_RAIN_GAP_MS) &&
                 phase <  (2U * ALERT_LED_RAIN_PULSE_MS + ALERT_LED_RAIN_GAP_MS))) {
                red = true;
            } else {
                red = false;
            }
            break;
        }
        case ALERT_LED_PATTERN_SYSTEM_FAULT: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_FAULT_PERIOD_MS;
            green = (phase < 100U) || (phase >= 200U && phase < 300U);
            red   = (phase >= 100U && phase < 200U) || (phase >= 300U && phase < 400U);
            break;
        }
        case ALERT_LED_PATTERN_CONSERVATION: {
            uint32_t phase = s_ctx.led_timer_ms % ALERT_LED_CONSERVE_PERIOD_MS;
            green = (phase < ALERT_LED_CONSERVE_ON_MS);
            red   = false;
            break;
        }
        default:
            green = false;
            red   = false;
            break;
    }

    /* 3. Evaluate Buzzer Animation States */
    bool buzzer = false;

    if (s_ctx.actuation_allowed && s_ctx.config.enable_audible_buzzer) {
        switch (s_ctx.active_buzzer_pattern) {
            case ALERT_BUZZER_PATTERN_OFF:
                buzzer = false;
                break;
            case ALERT_BUZZER_PATTERN_SHORT_CHIRP:
                buzzer = (s_ctx.buzzer_timer_ms < ALERT_BUZZER_CHIRP_MS);
                break;
            case ALERT_BUZZER_PATTERN_DOUBLE_CHIRP:
                if (s_ctx.buzzer_timer_ms < ALERT_BUZZER_DOUBLE_PULSE_MS ||
                    (s_ctx.buzzer_timer_ms >= (ALERT_BUZZER_DOUBLE_PULSE_MS + ALERT_BUZZER_DOUBLE_GAP_MS) &&
                     s_ctx.buzzer_timer_ms <  (2U * ALERT_BUZZER_DOUBLE_PULSE_MS + ALERT_BUZZER_DOUBLE_GAP_MS))) {
                    buzzer = true;
                } else {
                    buzzer = false;
                }
                break;
            case ALERT_BUZZER_PATTERN_STORM_BURST: {
                uint32_t phase = s_ctx.buzzer_timer_ms % (2U * ALERT_BUZZER_BURST_CADENCE_MS);
                buzzer = (phase < ALERT_BUZZER_BURST_CADENCE_MS);
                break;
            }
            case ALERT_BUZZER_PATTERN_FAULT_BEEP: {
                uint32_t phase = s_ctx.buzzer_timer_ms % ALERT_BUZZER_FAULT_PERIOD_MS;
                buzzer = (phase < ALERT_BUZZER_FAULT_ON_MS);
                break;
            }
            default:
                buzzer = false;
                break;
        }
    }

    alert_mgr_apply_outputs(green, red, buzzer);
    return STATUS_OK;
}

status_t alert_manager_set_led_pattern(alert_led_pattern_t pattern) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    if (pattern >= ALERT_LED_PATTERN_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_ctx.active_led_pattern = pattern;
    s_ctx.led_timer_ms       = 0U;
    return alert_manager_process_step(0U);
}

status_t alert_manager_set_buzzer_pattern(alert_buzzer_pattern_t pattern) {
    if (!s_ctx.initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    if (pattern >= ALERT_BUZZER_PATTERN_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_ctx.active_buzzer_pattern = pattern;
    s_ctx.buzzer_timer_ms       = 0U;
    return alert_manager_process_step(0U);
}

alert_led_pattern_t alert_manager_get_active_led_pattern(void) {
    return s_ctx.active_led_pattern;
}

alert_buzzer_pattern_t alert_manager_get_active_buzzer_pattern(void) {
    return s_ctx.active_buzzer_pattern;
}

bool alert_manager_is_siren_active(void) {
    return s_ctx.siren_relay_state;
}

uint32_t alert_manager_get_siren_cooldown_remaining_sec(void) {
    return (s_ctx.siren_cooldown_ms + 999U) / 1000U;
}

status_t alert_manager_force_all_off(void) {
    s_ctx.active_led_pattern    = ALERT_LED_PATTERN_OFF;
    s_ctx.active_buzzer_pattern = ALERT_BUZZER_PATTERN_OFF;
    s_ctx.led_timer_ms          = 0U;
    s_ctx.buzzer_timer_ms       = 0U;
    s_ctx.siren_pulse_remaining_ms = 0U;
    s_ctx.siren_relay_state     = false;

    alert_mgr_apply_outputs(false, false, false);
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

    p_status->active_led_pattern      = s_ctx.active_led_pattern;
    p_status->active_buzzer_pattern   = s_ctx.active_buzzer_pattern;
    p_status->green_led_state         = s_ctx.green_state;
    p_status->red_led_state           = s_ctx.red_state;
    p_status->buzzer_state            = s_ctx.buzzer_state;
    p_status->siren_relay_active      = s_ctx.siren_relay_state;
    p_status->siren_remaining_ms      = s_ctx.siren_pulse_remaining_ms;
    p_status->siren_cooldown_sec      = alert_manager_get_siren_cooldown_remaining_sec();
    p_status->total_siren_activations = s_ctx.total_siren_activations;
    p_status->total_alerts_imminent   = s_ctx.total_alerts_imminent;
    p_status->total_alerts_warning    = s_ctx.total_alerts_warning;
    p_status->is_throttled            = s_ctx.is_throttled;

    return STATUS_OK;
}
