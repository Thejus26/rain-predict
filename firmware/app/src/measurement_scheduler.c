/**
 * @file    measurement_scheduler.c
 * @brief   Adaptive multi-rate measurement scheduler and battery preservation throttling implementation.
 */

#include "measurement_scheduler.h"
#include <string.h>

/* ========================================================================== */
/* Static Module State Variables                                              */
/* ========================================================================== */

static scheduler_config_t s_config = {
    .nominal_interval_sec      = SCHEDULER_INTERVAL_NOMINAL_SEC,
    .storm_interval_sec        = SCHEDULER_INTERVAL_STORM_SEC,
    .rain_interval_sec         = SCHEDULER_INTERVAL_RAIN_SEC,
    .conserve_interval_sec     = SCHEDULER_INTERVAL_CONSERVE_SEC,
    .critical_interval_sec     = SCHEDULER_INTERVAL_CRITICAL_SEC,
    .hold_down_sec             = SCHEDULER_HOLD_DOWN_DURATION_SEC,
    .cpi_trigger_pct           = SCHEDULER_CPI_STORM_TRIGGER_PCT,
    .cpi_calm_pct              = SCHEDULER_CPI_CALM_RELEASE_PCT,
    .pressure_drop_trigger_hpa = SCHEDULER_PRESSURE_DROP_TRIGGER_HPA,
    .vbat_conserve_enter_v     = SCHEDULER_VBAT_CONSERVE_ENTER_V,
    .vbat_conserve_recover_v   = SCHEDULER_VBAT_CONSERVE_RECOVER_V,
    .vbat_critical_enter_v     = SCHEDULER_VBAT_CRITICAL_ENTER_V,
    .vbat_critical_recover_v   = SCHEDULER_VBAT_CRITICAL_RECOVER_V
};

static scheduler_mode_t        s_current_mode            = SCHEDULER_MODE_NOMINAL;
static battery_throttle_tier_t s_battery_tier            = BATTERY_TIER_NORMAL;
static float                   s_last_vbat_volts         = 3.30f;
static uint8_t                 s_vbat_recover_counter    = 0U;

static uint32_t s_hold_down_timer_sec     = 0U;
static uint32_t s_rain_calm_timer_sec     = 0U;
static uint16_t s_override_interval_sec   = 0U;
static bool     s_is_override_active      = false;
static uint32_t s_last_sleep_duration_sec = SCHEDULER_INTERVAL_NOMINAL_SEC;

static uint32_t s_total_wakeups_nominal   = 0U;
static uint32_t s_total_wakeups_storm     = 0U;
static uint32_t s_total_wakeups_rain      = 0U;
static uint32_t s_total_wakeups_throttled = 0U;
static bool     s_is_initialized          = false;

/* ========================================================================== */
/* Public API Implementation                                                  */
/* ========================================================================== */

status_t measurement_scheduler_init(const scheduler_config_t *config) {
    if (config != NULL) {
        s_config = *config;
        if (s_config.nominal_interval_sec == 0U) {
            s_config.nominal_interval_sec = SCHEDULER_INTERVAL_NOMINAL_SEC;
        }
        if (s_config.storm_interval_sec == 0U) {
            s_config.storm_interval_sec = SCHEDULER_INTERVAL_STORM_SEC;
        }
        if (s_config.rain_interval_sec == 0U) {
            s_config.rain_interval_sec = SCHEDULER_INTERVAL_RAIN_SEC;
        }
        if (s_config.conserve_interval_sec == 0U) {
            s_config.conserve_interval_sec = SCHEDULER_INTERVAL_CONSERVE_SEC;
        }
        if (s_config.critical_interval_sec == 0U) {
            s_config.critical_interval_sec = SCHEDULER_INTERVAL_CRITICAL_SEC;
        }
        if (s_config.hold_down_sec == 0U) {
            s_config.hold_down_sec = SCHEDULER_HOLD_DOWN_DURATION_SEC;
        }
    } else {
        s_config.nominal_interval_sec      = SCHEDULER_INTERVAL_NOMINAL_SEC;
        s_config.storm_interval_sec        = SCHEDULER_INTERVAL_STORM_SEC;
        s_config.rain_interval_sec         = SCHEDULER_INTERVAL_RAIN_SEC;
        s_config.conserve_interval_sec     = SCHEDULER_INTERVAL_CONSERVE_SEC;
        s_config.critical_interval_sec     = SCHEDULER_INTERVAL_CRITICAL_SEC;
        s_config.hold_down_sec             = SCHEDULER_HOLD_DOWN_DURATION_SEC;
        s_config.cpi_trigger_pct           = SCHEDULER_CPI_STORM_TRIGGER_PCT;
        s_config.cpi_calm_pct              = SCHEDULER_CPI_CALM_RELEASE_PCT;
        s_config.pressure_drop_trigger_hpa = SCHEDULER_PRESSURE_DROP_TRIGGER_HPA;
        s_config.vbat_conserve_enter_v     = SCHEDULER_VBAT_CONSERVE_ENTER_V;
        s_config.vbat_conserve_recover_v   = SCHEDULER_VBAT_CONSERVE_RECOVER_V;
        s_config.vbat_critical_enter_v     = SCHEDULER_VBAT_CRITICAL_ENTER_V;
        s_config.vbat_critical_recover_v   = SCHEDULER_VBAT_CRITICAL_RECOVER_V;
    }

    s_current_mode            = SCHEDULER_MODE_NOMINAL;
    s_battery_tier            = BATTERY_TIER_NORMAL;
    s_last_vbat_volts         = 3.30f;
    s_vbat_recover_counter    = 0U;
    s_hold_down_timer_sec     = 0U;
    s_rain_calm_timer_sec     = 0U;
    s_override_interval_sec   = 0U;
    s_is_override_active      = false;
    s_last_sleep_duration_sec = s_config.nominal_interval_sec;

    s_total_wakeups_nominal   = 0U;
    s_total_wakeups_storm     = 0U;
    s_total_wakeups_rain      = 0U;
    s_total_wakeups_throttled = 0U;
    s_is_initialized          = true;

    return STATUS_OK;
}

status_t measurement_scheduler_set_battery_voltage(float vbat_volts) {
    if (!s_is_initialized) {
        measurement_scheduler_init(NULL);
    }
    if (vbat_volts < 1.0f || vbat_volts > 5.0f) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    s_last_vbat_volts = vbat_volts;

    switch (s_battery_tier) {
        case BATTERY_TIER_NORMAL:
            if (vbat_volts < s_config.vbat_critical_enter_v) {
                /* Skip Conservation — voltage fell through both thresholds */
                s_battery_tier = BATTERY_TIER_CRITICAL;
                s_vbat_recover_counter = 0U;
            } else if (vbat_volts < s_config.vbat_conserve_enter_v) {
                s_battery_tier = BATTERY_TIER_CONSERVATION;
                s_vbat_recover_counter = 0U;
            }
            break;

        case BATTERY_TIER_CONSERVATION:
            if (vbat_volts < s_config.vbat_critical_enter_v) {
                /* Degrade further into Critical tier */
                s_battery_tier = BATTERY_TIER_CRITICAL;
                s_vbat_recover_counter = 0U;
            } else if (vbat_volts >= s_config.vbat_conserve_recover_v) {
                /* Solar recovery candidate — require 2 consecutive readings */
                s_vbat_recover_counter++;
                if (s_vbat_recover_counter >= 2U) {
                    s_battery_tier = BATTERY_TIER_NORMAL;
                    s_vbat_recover_counter = 0U;
                }
            } else {
                s_vbat_recover_counter = 0U;
            }
            break;

        case BATTERY_TIER_CRITICAL:
            if (vbat_volts >= s_config.vbat_critical_recover_v) {
                /* Solar recovery candidate — require 2 consecutive readings */
                s_vbat_recover_counter++;
                if (s_vbat_recover_counter >= 2U) {
                    s_battery_tier = BATTERY_TIER_CONSERVATION;
                    s_vbat_recover_counter = 0U;
                }
            } else {
                s_vbat_recover_counter = 0U;
            }
            break;

        default:
            s_battery_tier = BATTERY_TIER_NORMAL;
            break;
    }

    return STATUS_OK;
}

status_t measurement_scheduler_evaluate(uint8_t cpi_pct,
                                        float pressure_rate_hpa,
                                        bool solar_cloud_alarm,
                                        uint32_t active_rain_pulses,
                                        uint32_t active_duration_ms,
                                        scheduler_decision_t *out_decision) {
    if (!s_is_initialized) {
        status_t st = measurement_scheduler_init(NULL);
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (out_decision == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    scheduler_mode_t prev_mode = s_current_mode;
    uint32_t target_interval_sec;

    /* 1. Handle Remote Downlink Custom Override */
    if (s_is_override_active && s_override_interval_sec > 0U) {
        s_current_mode = SCHEDULER_MODE_OVERRIDE;
        target_interval_sec = s_override_interval_sec;
    } else if (s_battery_tier == BATTERY_TIER_CRITICAL) {
        /* 2. Critical Battery Tier Enforcement (60 minutes baseline) */
        s_current_mode = SCHEDULER_MODE_CRITICAL;
        if (active_rain_pulses > 0U) {
            /* Allow limited rain acceleration: cap at nominal interval (15 min) */
            target_interval_sec = s_config.nominal_interval_sec;
        } else {
            target_interval_sec = s_config.critical_interval_sec;
        }
        s_total_wakeups_throttled++;
    } else if (s_battery_tier == BATTERY_TIER_CONSERVATION) {
        /* 3. Conservation Battery Tier Enforcement (30 minutes baseline) */
        s_current_mode = SCHEDULER_MODE_CONSERVATION;
        if (active_rain_pulses > 0U) {
            /* Allow limited rain acceleration: cap at storm interval (5 min) */
            target_interval_sec = s_config.storm_interval_sec;
        } else {
            /* Storm CPI acceleration suppressed to conserve energy */
            target_interval_sec = s_config.conserve_interval_sec;
        }
        s_total_wakeups_throttled++;
    } else {
        /* 4. Normal Battery Tier: Autonomous Multi-Rate State Machine */
        bool is_storm_condition = (cpi_pct >= s_config.cpi_trigger_pct) ||
                                  (pressure_rate_hpa <= s_config.pressure_drop_trigger_hpa) ||
                                  solar_cloud_alarm;

        bool is_rain_condition = (active_rain_pulses > 0U);

        if (is_rain_condition) {
            s_current_mode = SCHEDULER_MODE_ACTIVE_RAIN;
            s_rain_calm_timer_sec = SCHEDULER_RAIN_CALM_DURATION_SEC; /* 10-minute rain calm hold */
            s_hold_down_timer_sec = s_config.hold_down_sec;           /* Arm storm hold-down */
        } else if (s_current_mode == SCHEDULER_MODE_ACTIVE_RAIN) {
            /* Check if rain calm timer has expired */
            if (s_rain_calm_timer_sec > s_config.rain_interval_sec) {
                s_rain_calm_timer_sec -= s_config.rain_interval_sec;
            } else {
                s_rain_calm_timer_sec = 0U;
                s_current_mode = SCHEDULER_MODE_STORM_WATCH;
            }
        }

        if (s_current_mode != SCHEDULER_MODE_ACTIVE_RAIN) {
            if (is_storm_condition) {
                s_current_mode = SCHEDULER_MODE_STORM_WATCH;
                s_hold_down_timer_sec = s_config.hold_down_sec; /* Reset 30-min hold-down timer */
            } else if (s_current_mode == SCHEDULER_MODE_STORM_WATCH) {
                /* Calm conditions detected while in Storm Watch mode */
                if (cpi_pct <= s_config.cpi_calm_pct) {
                    if (s_hold_down_timer_sec > s_config.storm_interval_sec) {
                        s_hold_down_timer_sec -= s_config.storm_interval_sec;
                    } else {
                        s_hold_down_timer_sec = 0U;
                        s_current_mode = SCHEDULER_MODE_NOMINAL;
                    }
                } else {
                    /* CPI between calm and trigger: keep hold-down armed */
                    s_hold_down_timer_sec = s_config.hold_down_sec;
                }
            } else {
                s_current_mode = SCHEDULER_MODE_NOMINAL;
                s_hold_down_timer_sec = 0U;
            }
        }

        /* 5. Map Selected Mode to Target Interval */
        switch (s_current_mode) {
            case SCHEDULER_MODE_ACTIVE_RAIN:
                target_interval_sec = s_config.rain_interval_sec;
                s_total_wakeups_rain++;
                break;
            case SCHEDULER_MODE_STORM_WATCH:
                target_interval_sec = s_config.storm_interval_sec;
                s_total_wakeups_storm++;
                break;
            case SCHEDULER_MODE_NOMINAL:
            default:
                target_interval_sec = s_config.nominal_interval_sec;
                s_total_wakeups_nominal++;
                break;
        }
    }

    /* 6. Active Execution Time Compensation */
    uint32_t active_sec = (active_duration_ms + 999U) / 1000U; /* Ceil to seconds */
    uint32_t computed_sleep_sec;
    if (target_interval_sec > active_sec) {
        computed_sleep_sec = target_interval_sec - active_sec;
    } else {
        computed_sleep_sec = 1U; /* Minimum 1-second sleep safeguard */
    }

    s_last_sleep_duration_sec = computed_sleep_sec;

    /* 7. Populate Output Decision Structure */
    out_decision->active_mode             = s_current_mode;
    out_decision->battery_tier            = s_battery_tier;
    out_decision->target_interval_sec     = target_interval_sec;
    out_decision->computed_sleep_sec      = computed_sleep_sec;
    out_decision->mode_changed            = (s_current_mode != prev_mode);
    out_decision->actuation_allowed       = measurement_scheduler_is_actuation_allowed();
    out_decision->aux_buses_allowed       = measurement_scheduler_is_aux_sensor_allowed();
    out_decision->max_tx_power_dbm        = measurement_scheduler_get_max_tx_power();
    out_decision->hold_down_remaining_sec = s_hold_down_timer_sec;

    return STATUS_OK;
}

status_t measurement_scheduler_set_override_interval(uint16_t interval_seconds) {
    if (!s_is_initialized) {
        status_t st = measurement_scheduler_init(NULL);
        if (st != STATUS_OK) {
            return st;
        }
    }

    if (interval_seconds == 0U) {
        /* Disable override */
        s_is_override_active    = false;
        s_override_interval_sec = 0U;
        s_current_mode          = SCHEDULER_MODE_NOMINAL;
        return STATUS_OK;
    }

    if (interval_seconds < SCHEDULER_INTERVAL_MIN_OVERRIDE_SEC ||
        interval_seconds > SCHEDULER_INTERVAL_MAX_OVERRIDE_SEC) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    s_override_interval_sec = interval_seconds;
    s_is_override_active    = true;
    s_current_mode          = SCHEDULER_MODE_OVERRIDE;

    return STATUS_OK;
}

status_t measurement_scheduler_clear_override(void) {
    return measurement_scheduler_set_override_interval(0U);
}

bool measurement_scheduler_is_actuation_allowed(void) {
    /* Acoustic buzzer & siren relay only allowed in healthy Tier 1 */
    return (s_battery_tier == BATTERY_TIER_NORMAL);
}

bool measurement_scheduler_is_aux_sensor_allowed(void) {
    /* Modbus & SDI-12 auxiliary buses allowed in Tiers 1 and 2; off in Tier 3 */
    return (s_battery_tier != BATTERY_TIER_CRITICAL);
}

int8_t measurement_scheduler_get_max_tx_power(void) {
    /* +22 dBm HP PA in Tiers 1 & 2; +14 dBm LP in Tier 3 to suppress current spike */
    return (s_battery_tier == BATTERY_TIER_CRITICAL) ? 14 : 22;
}

status_t measurement_scheduler_get_status(scheduler_status_t *out_status) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (out_status == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    out_status->current_mode     = s_current_mode;
    out_status->battery_tier     = s_battery_tier;
    out_status->last_vbat_volts  = s_last_vbat_volts;

    switch (s_current_mode) {
        case SCHEDULER_MODE_ACTIVE_RAIN:
            out_status->current_interval_sec = s_config.rain_interval_sec;
            break;
        case SCHEDULER_MODE_STORM_WATCH:
            out_status->current_interval_sec = s_config.storm_interval_sec;
            break;
        case SCHEDULER_MODE_CONSERVATION:
            out_status->current_interval_sec = s_config.conserve_interval_sec;
            break;
        case SCHEDULER_MODE_CRITICAL:
            out_status->current_interval_sec = s_config.critical_interval_sec;
            break;
        case SCHEDULER_MODE_OVERRIDE:
            out_status->current_interval_sec = s_override_interval_sec;
            break;
        case SCHEDULER_MODE_NOMINAL:
        default:
            out_status->current_interval_sec = s_config.nominal_interval_sec;
            break;
    }

    out_status->last_sleep_duration_sec = s_last_sleep_duration_sec;
    out_status->hold_down_timer_sec     = s_hold_down_timer_sec;
    out_status->override_interval_sec   = s_override_interval_sec;
    out_status->is_override_active      = s_is_override_active;
    out_status->actuation_allowed       = measurement_scheduler_is_actuation_allowed();
    out_status->aux_buses_allowed       = measurement_scheduler_is_aux_sensor_allowed();
    out_status->max_tx_power_dbm        = measurement_scheduler_get_max_tx_power();
    out_status->total_wakeups_nominal   = s_total_wakeups_nominal;
    out_status->total_wakeups_storm     = s_total_wakeups_storm;
    out_status->total_wakeups_rain      = s_total_wakeups_rain;
    out_status->total_wakeups_throttled = s_total_wakeups_throttled;

    return STATUS_OK;
}

status_t measurement_scheduler_reset(void) {
    s_current_mode            = SCHEDULER_MODE_NOMINAL;
    s_battery_tier            = BATTERY_TIER_NORMAL;
    s_last_vbat_volts         = 3.30f;
    s_vbat_recover_counter    = 0U;
    s_hold_down_timer_sec     = 0U;
    s_rain_calm_timer_sec     = 0U;
    s_override_interval_sec   = 0U;
    s_is_override_active      = false;
    s_last_sleep_duration_sec = s_config.nominal_interval_sec;
    return STATUS_OK;
}
