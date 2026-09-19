/**
 * @file    measurement_scheduler.c
 * @brief   Adaptive multi-rate measurement scheduler implementation.
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
    .hold_down_sec             = SCHEDULER_HOLD_DOWN_DURATION_SEC,
    .cpi_trigger_pct           = SCHEDULER_CPI_STORM_TRIGGER_PCT,
    .cpi_calm_pct              = SCHEDULER_CPI_CALM_RELEASE_PCT,
    .pressure_drop_trigger_hpa = SCHEDULER_PRESSURE_DROP_TRIGGER_HPA
};

static scheduler_mode_t s_current_mode            = SCHEDULER_MODE_NOMINAL;
static uint32_t         s_hold_down_timer_sec     = 0U;
static uint32_t         s_rain_calm_timer_sec     = 0U;
static uint16_t         s_override_interval_sec   = 0U;
static bool             s_is_override_active      = false;
static uint32_t         s_last_sleep_duration_sec = SCHEDULER_INTERVAL_NOMINAL_SEC;

static uint32_t s_total_wakeups_nominal = 0U;
static uint32_t s_total_wakeups_storm   = 0U;
static uint32_t s_total_wakeups_rain    = 0U;
static bool     s_is_initialized        = false;

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
        if (s_config.hold_down_sec == 0U) {
            s_config.hold_down_sec = SCHEDULER_HOLD_DOWN_DURATION_SEC;
        }
    } else {
        s_config.nominal_interval_sec      = SCHEDULER_INTERVAL_NOMINAL_SEC;
        s_config.storm_interval_sec        = SCHEDULER_INTERVAL_STORM_SEC;
        s_config.rain_interval_sec         = SCHEDULER_INTERVAL_RAIN_SEC;
        s_config.hold_down_sec             = SCHEDULER_HOLD_DOWN_DURATION_SEC;
        s_config.cpi_trigger_pct           = SCHEDULER_CPI_STORM_TRIGGER_PCT;
        s_config.cpi_calm_pct              = SCHEDULER_CPI_CALM_RELEASE_PCT;
        s_config.pressure_drop_trigger_hpa = SCHEDULER_PRESSURE_DROP_TRIGGER_HPA;
    }

    s_current_mode            = SCHEDULER_MODE_NOMINAL;
    s_hold_down_timer_sec     = 0U;
    s_rain_calm_timer_sec     = 0U;
    s_override_interval_sec   = 0U;
    s_is_override_active      = false;
    s_last_sleep_duration_sec = s_config.nominal_interval_sec;

    s_total_wakeups_nominal = 0U;
    s_total_wakeups_storm   = 0U;
    s_total_wakeups_rain    = 0U;
    s_is_initialized        = true;

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
    } else {
        /* 2. Autonomous Multi-Rate State Machine */
        bool is_storm_condition = (cpi_pct >= s_config.cpi_trigger_pct) ||
                                  (pressure_rate_hpa <= s_config.pressure_drop_trigger_hpa) ||
                                  solar_cloud_alarm;

        bool is_rain_condition = (active_rain_pulses > 0U);

        if (is_rain_condition) {
            s_current_mode = SCHEDULER_MODE_ACTIVE_RAIN;
            s_rain_calm_timer_sec = SCHEDULER_RAIN_CALM_DURATION_SEC; /* 10-minute rain calm hold */
            s_hold_down_timer_sec = s_config.hold_down_sec;           /* Arm storm hold-down as well */
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

        /* 3. Map Selected Mode to Target Interval */
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

    /* 4. Active Execution Time Compensation */
    uint32_t active_sec = (active_duration_ms + 999U) / 1000U; /* Ceil to seconds */
    uint32_t computed_sleep_sec;
    if (target_interval_sec > active_sec) {
        computed_sleep_sec = target_interval_sec - active_sec;
    } else {
        computed_sleep_sec = 1U; /* Minimum 1-second sleep safeguard */
    }

    s_last_sleep_duration_sec = computed_sleep_sec;

    /* 5. Populate Output Decision Structure */
    out_decision->active_mode             = s_current_mode;
    out_decision->target_interval_sec     = target_interval_sec;
    out_decision->computed_sleep_sec      = computed_sleep_sec;
    out_decision->mode_changed            = (s_current_mode != prev_mode);
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

status_t measurement_scheduler_get_status(scheduler_status_t *out_status) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (out_status == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    out_status->current_mode = s_current_mode;
    switch (s_current_mode) {
        case SCHEDULER_MODE_ACTIVE_RAIN:
            out_status->current_interval_sec = s_config.rain_interval_sec;
            break;
        case SCHEDULER_MODE_STORM_WATCH:
            out_status->current_interval_sec = s_config.storm_interval_sec;
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
    out_status->total_wakeups_nominal   = s_total_wakeups_nominal;
    out_status->total_wakeups_storm     = s_total_wakeups_storm;
    out_status->total_wakeups_rain      = s_total_wakeups_rain;

    return STATUS_OK;
}

status_t measurement_scheduler_reset(void) {
    s_current_mode            = SCHEDULER_MODE_NOMINAL;
    s_hold_down_timer_sec     = 0U;
    s_rain_calm_timer_sec     = 0U;
    s_override_interval_sec   = 0U;
    s_is_override_active      = false;
    s_last_sleep_duration_sec = s_config.nominal_interval_sec;
    return STATUS_OK;
}
