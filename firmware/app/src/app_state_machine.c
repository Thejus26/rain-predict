/**
 * @file    app_state_machine.c
 * @brief   Top-level application state machine and operational lifecycle implementation.
 * @details Implements the 8-state cyclic sequence: WAKE -> POWER_ON -> SAMPLE ->
 *          FILTER -> PREDICT -> TRANSMIT -> ALERT -> SLEEP for STM32WLE5 SoC.
 */

#include "app_state_machine.h"

#include <string.h>
#include <math.h>

#include "bsp_power_rails.h"
#include "bsp_indicators.h"
#include "bsp_adc.h"
#include "bme280_driver.h"
#include "opt3001_driver.h"
#include "rain_gauge_driver.h"
#include "dew_point.h"
#include "trend_detector.h"
#include "zambretti.h"
#include "telemetry_codec.h"
#include "flash_storage.h"
#include "lorawan_service.h"
#include "watchdog.h"
#include "app_fault_handler.h"

/* ========================================================================== */
/* Forward Declarations of Static Functions                                   */
/* ========================================================================== */

static status_t app_exec_wake(void);
static status_t app_exec_power_on(void);
static status_t app_exec_sample(void);
static status_t app_exec_filter(void);
static status_t app_exec_predict(void);
static status_t app_exec_transmit(void);
static status_t app_exec_alert(void);
static status_t app_exec_sleep(void);
static void     app_history_push(const env_sample_t *p_sample);
static void     app_watchdog_checkpoint(app_state_t state);

/* ========================================================================== */
/* Internal State Context & Drivers                                           */
/* ========================================================================== */

static app_context_t  s_app_ctx;
static bool           s_initialized         = false;
static bool           s_boot_watchdog_reset = false;
static uint32_t       s_watchdog_kick_count = 0U;
static uint32_t       s_state_entry_tick_ms = 0U;
static bool           s_waking_from_sleep   = false;
static bme280_dev_t   s_bme280_dev;
static opt3001_dev_t  s_opt3001_dev;

static env_sample_t   s_history_samples[TREND_SAMPLES_3HOUR + 1U];
static uint32_t       s_history_count = 0;

static const char * const s_state_names[STATE_MAX] = {
    [STATE_WAKE]       = "STATE_WAKE",
    [STATE_POWER_ON]   = "STATE_POWER_ON",
    [STATE_SAMPLE]     = "STATE_SAMPLE",
    [STATE_FILTER]     = "STATE_FILTER",
    [STATE_PREDICT]    = "STATE_PREDICT",
    [STATE_TRANSMIT]   = "STATE_TRANSMIT",
    [STATE_ALERT]      = "STATE_ALERT",
    [STATE_SLEEP]      = "STATE_SLEEP"
};

/* ========================================================================== */
/* Helper Functions                                                           */
/* ========================================================================== */

static void app_watchdog_checkpoint(app_state_t state) {
    /* Anti-Masking Invariant: Verify state validity and execution time */
    if (state < STATE_MAX) {
        uint32_t current_tick = power_mgr_get_tick_ms();
        uint32_t elapsed      = current_tick - s_state_entry_tick_ms;

        /* Maximum permissible single-state duration before considering it a stall */
        if (elapsed < 200U) {
            watchdog_refresh();
            s_watchdog_kick_count++;
            s_state_entry_tick_ms = current_tick;
        }
    }
}

static void app_history_push(const env_sample_t *p_sample) {
    if (p_sample == NULL) {
        return;
    }
    if (s_history_count < (TREND_SAMPLES_3HOUR + 1U)) {
        s_history_samples[s_history_count] = *p_sample;
        s_history_count++;
    } else {
        for (uint32_t i = 0; i < TREND_SAMPLES_3HOUR; i++) {
            s_history_samples[i] = s_history_samples[i + 1U];
        }
        s_history_samples[TREND_SAMPLES_3HOUR] = *p_sample;
    }
}

/* ========================================================================== */
/* State Execution Handlers                                                   */
/* ========================================================================== */

static status_t app_exec_wake(void) {
    /* Re-baseline state entry timestamp when waking from Stop 2 deep sleep */
    if (s_waking_from_sleep) {
        s_state_entry_tick_ms = power_mgr_get_tick_ms();
        s_waking_from_sleep   = false;
    }

    /* Checkpoint 1: WAKE */
    app_watchdog_checkpoint(STATE_WAKE);

    s_app_ctx.active_start_tick_ms = power_mgr_get_tick_ms();

    /* 1. Restore clocks to MSI 48 MHz and GPIO pin mux */
    (void)power_mgr_wake_restore();

    /* 2. Identify wakeup source */
    s_app_ctx.wake_reason = power_mgr_get_wake_reason();

    /* Transition to STATE_POWER_ON */
    s_app_ctx.previous_state = STATE_WAKE;
    s_app_ctx.current_state  = STATE_POWER_ON;
    return STATUS_OK;
}

static status_t app_exec_power_on(void) {
    /* Checkpoint 2: POWER_ON */
    app_watchdog_checkpoint(STATE_POWER_ON);

    /* 1. Energize switched sensor power rail (PA4) */
    status_t rc = bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, true);
    if (rc != STATUS_OK) {
        s_app_ctx.sensor_fault = true;
    }

    /* 2. Enforce 20ms RC stabilization guard delay */
    (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_SENSORS);

    /* Transition to STATE_SAMPLE */
    s_app_ctx.previous_state = STATE_POWER_ON;
    s_app_ctx.current_state  = STATE_SAMPLE;
    return STATUS_OK;
}

static status_t app_exec_sample(void) {
    /* Checkpoint 3: SAMPLE */
    app_watchdog_checkpoint(STATE_SAMPLE);

    /* 1. Read BME280 forced mode burst with autonomous recovery & fallback */
    bme280_data_t bme_data;
    memset(&bme_data, 0, sizeof(bme_data));
    status_t rc_bme = bme280_read_data(&s_bme280_dev, &bme_data);
    if ((rc_bme == STATUS_OK) && bme_data.is_valid) {
        s_app_ctx.temperature_c = bme_data.temperature_c;
        s_app_ctx.humidity_pct  = bme_data.humidity_percent;
        s_app_ctx.pressure_hpa  = bme_data.pressure_hpa;
        app_fault_handler_set_last_valid_bme280(bme_data.temperature_c,
                                                bme_data.humidity_percent,
                                                bme_data.pressure_hpa);
        app_fault_handler_report(FAULT_MASK_BME280_COMM, true);
    } else {
        /* Attempt autonomous I2C bus recovery */
        (void)app_fault_handler_recover_i2c_bus();
        app_fault_handler_report(FAULT_MASK_BME280_COMM, false);
        (void)app_fault_handler_get_bme280_fallback(&s_app_ctx.temperature_c,
                                                   &s_app_ctx.humidity_pct,
                                                   &s_app_ctx.pressure_hpa);
    }

    /* 2. Read OPT3001 single-shot illuminance with solar fallback */
    opt3001_reading_t opt_reading;
    memset(&opt_reading, 0, sizeof(opt_reading));
    status_t rc_opt = opt3001_read_lux(&s_opt3001_dev, &opt_reading);
    if ((rc_opt == STATUS_OK) && opt_reading.is_valid) {
        s_app_ctx.solar_lux = opt_reading.lux;
        app_fault_handler_report(FAULT_MASK_OPT3001_COMM, true);
    } else {
        app_fault_handler_report(FAULT_MASK_OPT3001_COMM, false);
        app_fault_handler_get_opt3001_fallback(power_mgr_get_rtc_hour(), &s_app_ctx.solar_lux);
    }

    /* 3. Read and clear tipping-bucket rain gauge pulses (clamped against contact chatter) */
    uint32_t raw_pulses = (uint32_t)rain_gauge_read_and_clear_interval(NULL);
    if (raw_pulses > 40U) {
        /* Clamp to maximum physical rate (40 tips/interval) and report chatter */
        s_app_ctx.rain_pulses_cycle = 40U;
        app_fault_handler_report(FAULT_MASK_RAIN_GAUGE_CHATTER, false);
    } else {
        s_app_ctx.rain_pulses_cycle = raw_pulses;
        app_fault_handler_report(FAULT_MASK_RAIN_GAUGE_CHATTER, true);
    }

    /* 4. Read battery voltage from ADC */
    uint16_t vbat_mv = 0;
    status_t rc_adc = bsp_adc_read_vbat_mv(&vbat_mv);
    if (rc_adc == STATUS_OK) {
        s_app_ctx.battery_volts = (float)vbat_mv / 1000.0f;
        (void)measurement_scheduler_set_battery_voltage(s_app_ctx.battery_volts);
        (void)power_mgr_battery_update((uint16_t)s_app_ctx.solar_lux);

        scheduler_status_t sched_stat;
        memset(&sched_stat, 0, sizeof(sched_stat));
        if (measurement_scheduler_get_status(&sched_stat) == STATUS_OK) {
            s_app_ctx.sched_decision.battery_tier      = sched_stat.battery_tier;
            s_app_ctx.sched_decision.actuation_allowed = sched_stat.actuation_allowed;
        }

        if (s_app_ctx.battery_volts < 2.90f) {
            app_fault_handler_report(FAULT_MASK_BATTERY_CRITICAL, false);
            app_fault_handler_report(FAULT_MASK_BATTERY_LOW, false);
        } else if (s_app_ctx.battery_volts < 3.10f) {
            app_fault_handler_report(FAULT_MASK_BATTERY_LOW, false);
            app_fault_handler_report(FAULT_MASK_BATTERY_CRITICAL, true);
        } else {
            app_fault_handler_report(FAULT_MASK_BATTERY_LOW, true);
            app_fault_handler_report(FAULT_MASK_BATTERY_CRITICAL, true);
        }
    }

    /* 5. Update master system sensor fault status */
    s_app_ctx.sensor_fault = app_fault_handler_is_system_fault_active();

    /* Transition to STATE_FILTER without stalling */
    s_app_ctx.previous_state = STATE_SAMPLE;
    s_app_ctx.current_state  = STATE_FILTER;
    return STATUS_OK;
}

static status_t app_exec_filter(void) {
    /* Checkpoint 4: FILTER */
    app_watchdog_checkpoint(STATE_FILTER);

    /* 1. Calculate Magnus-Tetens dew point and dew point depression */
    float tdew = 0.0f;
    float dpd  = 0.0f;
    (void)dew_point_calc_tdew(s_app_ctx.temperature_c, s_app_ctx.humidity_pct, &tdew);
    (void)dew_point_calc_depression(s_app_ctx.temperature_c, s_app_ctx.humidity_pct, &dpd);
    s_app_ctx.dew_point_c = tdew;
    s_app_ctx.dpd_c       = dpd;

    /* 2. Append sample to historical sliding window */
    env_sample_t new_sample;
    new_sample.temp_c      = s_app_ctx.temperature_c;
    new_sample.rh_pct      = s_app_ctx.humidity_pct;
    new_sample.p0_hpa      = s_app_ctx.pressure_hpa;
    new_sample.lux         = s_app_ctx.solar_lux;
    new_sample.timestamp_s = power_mgr_get_tick_ms() / 1000U;
    app_history_push(&new_sample);

    /* 3. Compute multi-variable gradient differentials */
    multi_gradient_t gradients;
    memset(&gradients, 0, sizeof(gradients));
    if (trend_detector_compute_gradients(s_history_samples, s_history_count, &gradients) == 0) {
        s_app_ctx.delta_p_1h_hpa = gradients.delta_p_1h_hpa;
    } else {
        s_app_ctx.delta_p_1h_hpa = 0.0f;
    }

    /* Transition to STATE_PREDICT */
    s_app_ctx.previous_state = STATE_FILTER;
    s_app_ctx.current_state  = STATE_PREDICT;
    return STATUS_OK;
}

static status_t app_exec_predict(void) {
    /* Checkpoint 5: PREDICT */
    app_watchdog_checkpoint(STATE_PREDICT);

    /* 1. Calculate Zambretti 26-rule forecast */
    uint8_t z_idx = 1U;
    rain_forecast_state_t z_state = RAIN_STATE_UNLIKELY;
    (void)zambretti_calculate(s_app_ctx.pressure_hpa, s_app_ctx.delta_p_1h_hpa * 3.0f, &z_idx, &z_state);
    s_app_ctx.zambretti_code = z_idx;

    /* 2. Run Composite Precipitation Index (CPI) scoring & alert classification */
    rain_forecast_t forecast;
    memset(&forecast, 0, sizeof(forecast));
    status_t rc_rain = rain_algo_evaluate(s_history_samples,
                                          s_history_count,
                                          100.0f,
                                          6U,
                                          WIND_DIR_CALM,
                                          0.0f,
                                          &forecast);
    if (rc_rain == STATUS_OK) {
        s_app_ctx.cpi_pct    = forecast.cpi_score_pct;
        s_app_ctx.rain_state = forecast.forecast_state;
    } else {
        s_app_ctx.cpi_pct    = 0.0f;
        s_app_ctx.rain_state = RAIN_ALERT_UNLIKELY;
    }

    /* Transition to STATE_TRANSMIT */
    s_app_ctx.previous_state = STATE_PREDICT;
    s_app_ctx.current_state  = STATE_TRANSMIT;
    return STATUS_OK;
}

static status_t app_exec_transmit(void) {
    /* Checkpoint 6: TRANSMIT */
    app_watchdog_checkpoint(STATE_TRANSMIT);

    /* Map internal rain alert and physical gauge state to over-the-air telemetry state */
    telemetry_rain_state_t telem_rain;
    if (s_app_ctx.rain_pulses_cycle > 0U || rain_gauge_is_rain_active()) {
        telem_rain = TELEMETRY_RAIN_STATE_ACTIVE_RAIN;
    } else if (s_app_ctx.rain_state >= RAIN_ALERT_LIKELY || s_app_ctx.cpi_pct >= 60.0f) {
        telem_rain = TELEMETRY_RAIN_STATE_IMMINENT;
    } else if (s_app_ctx.rain_state == RAIN_ALERT_POSSIBLE || s_app_ctx.cpi_pct >= 30.0f) {
        telem_rain = TELEMETRY_RAIN_STATE_POSSIBLE;
    } else {
        telem_rain = TELEMETRY_RAIN_STATE_UNLIKELY;
    }

    /* 1. Serialize periodic telemetry packet */
    telemetry_periodic_data_t telem = {
        .temperature_c          = s_app_ctx.temperature_c,
        .humidity_pct           = s_app_ctx.humidity_pct,
        .pressure_hpa           = s_app_ctx.pressure_hpa,
        .ambient_lux            = s_app_ctx.solar_lux,
        .rain_interval_mm       = (float)s_app_ctx.rain_pulses_cycle * RAIN_GAUGE_CALIB_MM_PER_TIP,
        .forecast_state         = telem_rain,
        .zambretti_index        = s_app_ctx.zambretti_code,
        .cpi_prob_pct           = (uint8_t)s_app_ctx.cpi_pct,
        .solar_cloud_drop_alarm = (s_app_ctx.solar_lux < 3000.0f && s_app_ctx.solar_lux > 50.0f),
        .battery_voltage_v      = s_app_ctx.battery_volts,
        .sensor_fault           = s_app_ctx.sensor_fault,
        .unexpected_reset       = s_boot_watchdog_reset
    };

    uint8_t payload[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    memset(payload, 0, sizeof(payload));
    size_t encoded_len = 0;
    (void)telemetry_encode_periodic(&telem, payload, sizeof(payload), &encoded_len);

    /* 2. Log to on-chip Flash ring buffer */
    status_t rc_flash = flash_ring_push(payload, (uint16_t)s_app_ctx.cycle_count);
    app_fault_handler_report(FAULT_MASK_FLASH_WRITE, (rc_flash == STATUS_OK));

    /* 3. Dispatch unconfirmed uplink via LoRaWAN Class A stack */
    status_t rc_lora = lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC, payload, (uint8_t)sizeof(payload));
    (void)lorawan_service_process_step();
    app_fault_handler_report(FAULT_MASK_LORA_TX_TIMEOUT, (rc_lora == STATUS_OK));

    /* Transition to STATE_ALERT */
    s_app_ctx.previous_state = STATE_TRANSMIT;
    s_app_ctx.current_state  = STATE_ALERT;
    return STATUS_OK;
}

static status_t app_exec_alert(void) {
    /* Checkpoint 7: ALERT */
    app_watchdog_checkpoint(STATE_ALERT);

    /* 1. Update Alert Manager inputs */
    alert_input_t alert_in = {
        .rain_state         = s_app_ctx.rain_state,
        .cpi_pct            = s_app_ctx.cpi_pct,
        .rain_pulses_recent = s_app_ctx.rain_pulses_cycle,
        .battery_tier       = s_app_ctx.sched_decision.battery_tier,
        .sensor_fault       = s_app_ctx.sensor_fault,
        .is_sleeping        = false,
        .rtc_hour_0_to_23   = power_mgr_get_rtc_hour()
    };
    (void)alert_manager_update(&alert_in);

    /* 2. Animate indicators for active display window (50 ms) */
    (void)alert_manager_process_step(50U);

    /* Transition to STATE_SLEEP */
    s_app_ctx.previous_state = STATE_ALERT;
    s_app_ctx.current_state  = STATE_SLEEP;
    return STATUS_OK;
}

static status_t app_exec_sleep(void) {
    /* Checkpoint 8: SLEEP */
    app_watchdog_checkpoint(STATE_SLEEP);

    /* 1. Evaluate Measurement Scheduler interval with active duration compensation */
    uint32_t active_elapsed_ms = power_mgr_get_tick_ms() - s_app_ctx.active_start_tick_ms;
    bool solar_drop = (s_app_ctx.solar_lux < 3000.0f && s_app_ctx.solar_lux > 50.0f);

    (void)measurement_scheduler_evaluate((uint8_t)s_app_ctx.cpi_pct,
                                         s_app_ctx.delta_p_1h_hpa,
                                         solar_drop,
                                         s_app_ctx.rain_pulses_cycle,
                                         active_elapsed_ms,
                                         &s_app_ctx.sched_decision);

    s_app_ctx.configured_sleep_sec = s_app_ctx.sched_decision.computed_sleep_sec;
    s_app_ctx.active_duration_ms   = active_elapsed_ms;

    /* 2. Force all indicators OFF before deep sleep */
    (void)alert_manager_force_all_off();

    /* 3. De-energize switched sensor power rails */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, false);

    /* 4. Prepare GPIOs for low-leakage Stop 2 */
    (void)power_mgr_gpio_sleep_prepare();

    /* 5. Advance cycle counter and update state for next cycle */
    s_app_ctx.cycle_count++;
    s_app_ctx.previous_state = STATE_SLEEP;
    s_app_ctx.current_state  = STATE_WAKE;

    /* 6. Enter Stop 2 deep sleep (< 3.0 uA) */
    (void)power_mgr_enter_stop2(s_app_ctx.configured_sleep_sec);
    s_waking_from_sleep   = true;
    s_state_entry_tick_ms = power_mgr_get_tick_ms();
    return STATUS_OK;
}

/* ========================================================================== */
/* Public State Machine API Implementations                                   */
/* ========================================================================== */

status_t app_state_machine_init(void) {
    memset(&s_app_ctx, 0, sizeof(s_app_ctx));

    /* 1. Evaluate Boot Reset Reason */
    s_boot_watchdog_reset = watchdog_was_reset_by_watchdog();
    watchdog_clear_reset_flags();

    /* 2. Initialize Watchdog with 8.0s timeout */
    (void)watchdog_init(WATCHDOG_TIMEOUT_MS_DEFAULT);

    /* 3. Initialize metrics */
    s_watchdog_kick_count = 0U;
    s_state_entry_tick_ms = power_mgr_get_tick_ms();
    s_waking_from_sleep   = false;

    s_app_ctx.current_state  = STATE_WAKE;
    s_app_ctx.previous_state = STATE_SLEEP;

    status_t rc = STATUS_OK;
    rc |= bsp_power_rails_init();
    rc |= bsp_indicators_init();
    rc |= bsp_adc_init();
    rc |= rain_gauge_init();
    rc |= power_mgr_init();
    rc |= alert_manager_init(NULL);
    rc |= measurement_scheduler_init(NULL);
    rc |= app_fault_handler_init();
    rc |= flash_storage_init();
    rc |= flash_ring_init();
    rc |= lorawan_service_init(NULL);

    (void)bme280_init(&s_bme280_dev, BME280_I2C_ADDR_PRIMARY);
    bme280_config_t bme_cfg = {
        .osrs_t = BME280_OVERSAMPLING_2X,
        .osrs_p = BME280_OVERSAMPLING_16X,
        .osrs_h = BME280_OVERSAMPLING_1X,
        .filter = BME280_FILTER_COEFF_4
    };
    (void)bme280_configure(&s_bme280_dev, &bme_cfg);
    (void)opt3001_init(&s_opt3001_dev, (uint8_t)OPT3001_I2C_ADDR_GND);

    s_history_count = 0;
    memset(s_history_samples, 0, sizeof(s_history_samples));

    s_initialized = true;
    return rc;
}

status_t app_state_machine_step(void) {
    if (!s_initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    switch (s_app_ctx.current_state) {
        case STATE_WAKE:       return app_exec_wake();
        case STATE_POWER_ON:   return app_exec_power_on();
        case STATE_SAMPLE:     return app_exec_sample();
        case STATE_FILTER:     return app_exec_filter();
        case STATE_PREDICT:    return app_exec_predict();
        case STATE_TRANSMIT:   return app_exec_transmit();
        case STATE_ALERT:      return app_exec_alert();
        case STATE_SLEEP:      return app_exec_sleep();
        default:
            s_app_ctx.current_state = STATE_WAKE;
            return STATUS_ERR_INVALID_STATE;
    }
}

status_t app_state_machine_run_cycle(void) {
    if (!s_initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    /* Execute all 8 states sequentially */
    for (uint8_t s = 0; s < (uint8_t)STATE_MAX; s++) {
        status_t rc = app_state_machine_step();
        if (rc != STATUS_OK) {
            return rc;
        }
    }
    return STATUS_OK;
}

app_state_t app_state_machine_get_current_state(void) {
    return s_app_ctx.current_state;
}

const app_context_t *app_state_machine_get_context(void) {
    return &s_app_ctx;
}

const char *app_state_machine_get_state_name(app_state_t state) {
    if (state >= STATE_MAX) {
        return "STATE_UNKNOWN";
    }
    return s_state_names[state];
}

void app_state_machine_reset(void) {
    memset(&s_app_ctx, 0, sizeof(s_app_ctx));
    s_app_ctx.current_state  = STATE_WAKE;
    s_app_ctx.previous_state = STATE_SLEEP;
    s_history_count = 0;
    memset(s_history_samples, 0, sizeof(s_history_samples));
    app_fault_handler_reset();
    s_boot_watchdog_reset = false;
    s_watchdog_kick_count = 0U;
    s_state_entry_tick_ms = 0U;
    s_waking_from_sleep   = false;
    s_initialized         = false;
}

uint32_t app_state_machine_get_watchdog_kick_count(void) {
    return s_watchdog_kick_count;
}

bool app_state_machine_was_boot_watchdog_reset(void) {
    return s_boot_watchdog_reset;
}

uint32_t app_state_machine_get_state_entry_tick_ms(void) {
    return s_state_entry_tick_ms;
}
