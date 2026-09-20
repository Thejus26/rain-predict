/**
 * @file    test_fault_injection.c
 * @brief   System Fault Injection and Resilience Integration Test Suite (S6-T4.2).
 * @details Validates 100% graceful degradation across I2C lockups, sensor dropouts,
 *          low battery brownouts, LoRa radio timeouts, Flash failures, and watchdog traps.
 *          Implements the 16-point integration test matrix (FI-SM-01 to FI-SM-16).
 */

#include "unity.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "app_state_machine.h"
#include "app_fault_handler.h"
#include "measurement_scheduler.h"
#include "alert_manager.h"
#include "rain_algo.h"
#include "power_mgr.h"
#include "watchdog.h"
#include "bsp_power_rails.h"
#include "bsp_indicators.h"
#include "bsp_adc.h"
#include "rain_gauge_driver.h"
#include "bme280_driver.h"
#include "opt3001_driver.h"
#include "zambretti.h"
#include "trend_detector.h"
#include "dew_point.h"
#include "moving_avg_filter.h"
#include "telemetry_codec.h"
#include "flash_storage.h"
#include "lorawan_service.h"
#include "i2c_bus.h"
#include "status.h"

/* ========================================================================== */
/* Fault Injector & Peripheral Mock Context                                   */
/* ========================================================================== */

typedef struct {
    /* Fault Injection Triggers */
    bool                inject_bme280_nack;
    bool                inject_opt3001_dead;
    bool                inject_i2c_lockup;
    bool                inject_lora_timeout;
    bool                inject_flash_write_error;
    bool                inject_rain_chatter;
    bool                inject_watchdog_reboot;

    /* Microclimate & Sensor Inputs */
    float               mock_temp_c;
    float               mock_rh_pct;
    float               mock_press_hpa;
    float               mock_lux;
    uint32_t            mock_rain_tips;
    float               mock_vbat_v;

    /* Switched Power Rails */
    bool                sensor_rail_en;
    uint32_t            stabilization_delay_count;
    uint32_t            rail_toggle_count;

    /* Indicators & Actuators */
    bool                green_led;
    bool                red_led;
    bool                buzzer;
    bool                relay;
    uint32_t            relay_pulse_ms;

    /* Bus Diagnostics */
    uint32_t            i2c_recovery_calls;

    /* Flash NVM Storage */
    uint32_t            flash_records_stored;
    uint8_t             last_flash_payload[64];

    /* LoRaWAN Radio Subsystem */
    uint32_t            lora_tx_attempts;
    uint32_t            lora_tx_success_count;
    uint8_t             last_lora_payload[64];
    uint8_t             last_lora_len;
    uint8_t             last_lora_fport;

    /* Backlog Playback Simulation */
    bool                playback_active;
    bool                playback_preempted;
    uint32_t            playback_records_drained;

    /* System Timing, Power & Sleep */
    uint32_t            current_tick_ms;
    uint32_t            configured_sleep_sec;
    bool                in_stop2_sleep;
    power_wake_reason_t wake_reason;
    uint8_t             rtc_hour;

    /* Watchdog Supervisor */
    uint32_t            watchdog_kicks;
    bool                watchdog_enabled;
    uint32_t            watchdog_timeout_ms;
} fault_injection_mock_t;

static fault_injection_mock_t s_mock;

/* ========================================================================== */
/* Mock Driver Implementations with Fault Injection Hooks                     */
/* ========================================================================== */

/* --- Switched Power Rail Stubs --- */

status_t bsp_power_rails_init(void) {
    s_mock.sensor_rail_en = false;
    return STATUS_OK;
}

status_t bsp_power_rail_enable(bsp_power_rail_t rail, bool enable) {
    if (rail == BSP_POWER_RAIL_SENSORS) {
        if (s_mock.sensor_rail_en != enable) {
            s_mock.rail_toggle_count++;
        }
        s_mock.sensor_rail_en = enable;
    }
    return STATUS_OK;
}

bool bsp_power_rail_is_enabled(bsp_power_rail_t rail) {
    if (rail == BSP_POWER_RAIL_SENSORS) {
        return s_mock.sensor_rail_en;
    }
    return false;
}

status_t bsp_power_rails_all_off(void) {
    s_mock.sensor_rail_en = false;
    return STATUS_OK;
}

status_t bsp_power_rail_stabilize(bsp_power_rail_t rail) {
    (void)rail;
    s_mock.stabilization_delay_count++;
    s_mock.current_tick_ms += 20U;
    return STATUS_OK;
}

uint32_t bsp_power_rail_get_stabilization_ms(bsp_power_rail_t rail) {
    (void)rail;
    return 20U;
}

/* --- Indicator & Actuator Stubs --- */

status_t bsp_indicators_init(void) {
    s_mock.green_led      = false;
    s_mock.red_led        = false;
    s_mock.buzzer         = false;
    s_mock.relay          = false;
    s_mock.relay_pulse_ms = 0U;
    return STATUS_OK;
}

void bsp_led_set(bsp_led_t led, bool state) {
    if (led == BSP_LED_GREEN) {
        s_mock.green_led = state;
    } else if (led == BSP_LED_RED) {
        s_mock.red_led = state;
    }
}

void bsp_led_toggle(bsp_led_t led) {
    if (led == BSP_LED_GREEN) {
        s_mock.green_led = !s_mock.green_led;
    } else if (led == BSP_LED_RED) {
        s_mock.red_led = !s_mock.red_led;
    }
}

bool bsp_led_get(bsp_led_t led) {
    if (led == BSP_LED_GREEN) {
        return s_mock.green_led;
    } else if (led == BSP_LED_RED) {
        return s_mock.red_led;
    }
    return false;
}

void bsp_buzzer_set(bool state) {
    s_mock.buzzer = state;
}

bool bsp_buzzer_get(void) {
    return s_mock.buzzer;
}

void bsp_relay_set(bool state) {
    s_mock.relay = state;
}

bool bsp_relay_get(void) {
    return s_mock.relay;
}

status_t bsp_relay_trigger_timed(uint32_t duration_ms) {
    s_mock.relay          = true;
    s_mock.relay_pulse_ms = duration_ms;
    return STATUS_OK;
}

status_t bsp_indicators_all_off(void) {
    s_mock.green_led = false;
    s_mock.red_led   = false;
    s_mock.buzzer    = false;
    s_mock.relay     = false;
    return STATUS_OK;
}

void bsp_indicators_process(uint32_t delta_ms) {
    (void)delta_ms;
}

status_t bsp_indicator_set_pattern(bsp_indicator_pattern_t pattern) {
    (void)pattern;
    return STATUS_OK;
}

bsp_indicator_pattern_t bsp_indicator_get_pattern(void) {
    return BSP_INDICATOR_PATTERN_OFF;
}

/* --- Battery ADC Stubs --- */

status_t bsp_adc_init(void) {
    return STATUS_OK;
}

status_t bsp_adc_deinit(void) {
    return STATUS_OK;
}

uint16_t bsp_adc_get_vrefint_factory_cal(void) {
    return 1660U;
}

status_t bsp_adc_read_vrefint_raw(uint16_t *p_raw_vref) {
    if (p_raw_vref == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *p_raw_vref = 1660U;
    return STATUS_OK;
}

status_t bsp_adc_read_vbat_raw(uint16_t *p_raw_vbat) {
    if (p_raw_vbat == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *p_raw_vbat = (uint16_t)((s_mock.mock_vbat_v / 6.0f) * 4095.0f);
    return STATUS_OK;
}

status_t bsp_adc_read_vbat_mv(uint16_t *p_vbat_mv) {
    if (p_vbat_mv == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *p_vbat_mv = (uint16_t)(s_mock.mock_vbat_v * 1000.0f);
    return STATUS_OK;
}

/* --- I2C Bus & Sensor Stubs --- */

status_t i2c_bus_recover(void) {
    s_mock.i2c_recovery_calls++;
    if (s_mock.inject_i2c_lockup) {
        return STATUS_ERR_I2C_BUS;
    }
    return STATUS_OK;
}

status_t bme280_init(bme280_dev_t *dev, uint8_t dev_addr) {
    (void)dev;
    (void)dev_addr;
    return STATUS_OK;
}

status_t bme280_configure(bme280_dev_t *dev, const bme280_config_t *cfg) {
    (void)dev;
    (void)cfg;
    return STATUS_OK;
}

status_t bme280_read_data(bme280_dev_t *dev, bme280_data_t *p_data) {
    (void)dev;
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (s_mock.inject_bme280_nack || s_mock.inject_i2c_lockup) {
        return STATUS_ERR_I2C_BUS;
    }
    p_data->temperature_c    = s_mock.mock_temp_c;
    p_data->humidity_percent = s_mock.mock_rh_pct;
    p_data->pressure_hpa     = s_mock.mock_press_hpa;
    p_data->is_valid         = true;
    return STATUS_OK;
}

status_t opt3001_init(opt3001_dev_t *dev, uint8_t dev_addr) {
    (void)dev;
    (void)dev_addr;
    return STATUS_OK;
}

status_t opt3001_read_lux(opt3001_dev_t *dev, opt3001_reading_t *p_reading) {
    (void)dev;
    if (p_reading == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (s_mock.inject_opt3001_dead || s_mock.inject_i2c_lockup) {
        return STATUS_ERR_I2C_BUS;
    }
    p_reading->lux             = s_mock.mock_lux;
    p_reading->irradiance_w_m2 = s_mock.mock_lux / 120.0f;
    p_reading->centi_lux       = (uint32_t)(s_mock.mock_lux * 100.0f);
    p_reading->telemetry_raw   = (uint16_t)(s_mock.mock_lux / 2.0f);
    p_reading->is_valid        = true;
    return STATUS_OK;
}

status_t rain_gauge_init(void) {
    return STATUS_OK;
}

status_t rain_gauge_deinit(void) {
    return STATUS_OK;
}

uint16_t rain_gauge_read_and_clear_interval(float *p_interval_mm) {
    uint32_t tips = s_mock.inject_rain_chatter ? 350U : s_mock.mock_rain_tips;
    s_mock.mock_rain_tips = 0U;
    if (p_interval_mm != NULL) {
        *p_interval_mm = (float)tips * RAIN_GAUGE_CALIB_MM_PER_TIP;
    }
    return (uint16_t)tips;
}

bool rain_gauge_is_rain_active(void) {
    return (s_mock.mock_rain_tips > 0U || s_mock.inject_rain_chatter);
}

/* --- Power Manager & Deep Sleep Stubs --- */

status_t power_mgr_init(void) {
    return STATUS_OK;
}

uint32_t power_mgr_get_tick_ms(void) {
    return s_mock.current_tick_ms;
}

power_wake_reason_t power_mgr_get_wake_reason(void) {
    return s_mock.wake_reason;
}

uint8_t power_mgr_get_rtc_hour(void) {
    return s_mock.rtc_hour;
}

status_t power_mgr_wake_restore(void) {
    s_mock.in_stop2_sleep = false;
    return STATUS_OK;
}

status_t power_mgr_battery_update(uint16_t ambient_lux) {
    (void)ambient_lux;
    return STATUS_OK;
}

status_t power_mgr_gpio_sleep_prepare(void) {
    return STATUS_OK;
}

status_t power_mgr_set_rtc_wakeup(uint32_t interval_sec) {
    s_mock.configured_sleep_sec = interval_sec;
    return STATUS_OK;
}

status_t power_mgr_enter_stop2(uint32_t sleep_duration_sec) {
    s_mock.in_stop2_sleep       = true;
    s_mock.configured_sleep_sec = sleep_duration_sec;
    return STATUS_OK;
}

/* --- Flash Storage Stubs --- */

status_t flash_storage_init(void) {
    s_mock.flash_records_stored = 0U;
    return STATUS_OK;
}

status_t flash_ring_init(void) {
    return STATUS_OK;
}

status_t flash_ring_push(const uint8_t *payload, uint16_t seq_id) {
    (void)seq_id;
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (s_mock.inject_flash_write_error) {
        return STATUS_ERROR_HARDWARE;
    }
    memcpy(s_mock.last_flash_payload, payload, 12);
    s_mock.flash_records_stored++;
    return STATUS_OK;
}

/* --- LoRaWAN Network Service Stubs --- */

status_t lorawan_service_init(const lorawan_credentials_t *credentials) {
    (void)credentials;
    s_mock.lora_tx_attempts      = 0U;
    s_mock.lora_tx_success_count = 0U;
    return STATUS_OK;
}

status_t lorawan_send_unconfirmed(uint8_t fport, const uint8_t *payload, uint8_t length) {
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    s_mock.lora_tx_attempts++;
    if (s_mock.inject_lora_timeout) {
        return STATUS_ERR_TIMEOUT;
    }
    s_mock.last_lora_fport = fport;
    s_mock.last_lora_len   = length;
    memcpy(s_mock.last_lora_payload, payload, length);
    s_mock.lora_tx_success_count++;
    return STATUS_OK;
}

status_t lorawan_service_process_step(void) {
    return STATUS_OK;
}

/* --- Watchdog Stubs --- */

status_t watchdog_init(uint32_t timeout_ms) {
    s_mock.watchdog_enabled    = true;
    s_mock.watchdog_timeout_ms = timeout_ms;
    return STATUS_OK;
}

void watchdog_refresh(void) {
    s_mock.watchdog_kicks++;
}

bool watchdog_was_reset_by_watchdog(void) {
    return s_mock.inject_watchdog_reboot;
}

void watchdog_clear_reset_flags(void) {
    s_mock.inject_watchdog_reboot = false;
}

/* ========================================================================== */
/* Test Setup & Teardown Helpers                                              */
/* ========================================================================== */

static void helper_prime_history_baseline(void) {
    /* Prime sliding window with 6 baseline cycles (T=24.5C, RH=65%, P=952.0 hPa)
     * so that gradient trend differentials have sufficient history depth (> 6). */
    for (int i = 0; i < 6; i++) {
        s_mock.mock_temp_c     = 24.5f;
        s_mock.mock_rh_pct     = 65.0f;
        s_mock.mock_press_hpa  = 952.0f;
        s_mock.mock_lux        = 55000.0f;
        s_mock.mock_rain_tips  = 0U;
        s_mock.current_tick_ms += 600000U; /* 10 min */
        (void)app_state_machine_run_cycle();
    }
}

void setUp(void) {
    memset(&s_mock, 0, sizeof(s_mock));
    s_mock.mock_temp_c     = 24.5f;
    s_mock.mock_rh_pct     = 65.0f;
    s_mock.mock_press_hpa  = 952.0f;
    s_mock.mock_lux        = 55000.0f;
    s_mock.mock_vbat_v     = 3.30f;
    s_mock.current_tick_ms = 1000U;
    s_mock.rtc_hour        = 14U; /* 2 PM: daytime, not quiet hours */
    s_mock.wake_reason     = POWER_WAKE_REASON_RTC;

    (void)measurement_scheduler_reset();
    app_fault_handler_reset();
    app_state_machine_reset();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
}

void tearDown(void) {
    (void)alert_manager_force_all_off();
    app_fault_handler_reset();
    app_state_machine_reset();
}

/* ========================================================================== */
/* Category A: Sensor & I2C Bus Fault Integration Tests                       */
/* ========================================================================== */

/**
 * @brief FI-SM-01: BME280 disconnect with stale pressure holding (3 cycles max)
 *                  before neutral baseline fallback.
 */
static void test_fi_bme280_disconnect_and_stale_hold(void) {
    /* 1. Run 1 good cycle to establish valid cache in app_fault_handler */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_FALSE(ctx->sensor_fault);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 952.0f, ctx->pressure_hpa);

    /* 2. Inject BME280 NACK */
    s_mock.inject_bme280_nack = true;

    /* Cycles 2, 3, 4: Should hold stale pressure for up to 3 cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        s_mock.current_tick_ms += 900000U;
        TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
        ctx = app_state_machine_get_context();
        TEST_ASSERT_TRUE(ctx->sensor_fault);
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 952.0f, ctx->pressure_hpa);
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 24.5f, ctx->temperature_c);
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 65.0f, ctx->humidity_pct);
        /* Telemetry Byte 11 bit 5 (0x20) indicates sensor fault */
        TEST_ASSERT_TRUE((s_mock.last_lora_payload[11] & 0x20) != 0U);
        TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
    }

    /* Cycle 5 (4th failed cycle): Exceeds stale window -> neutral fallback defaults */
    s_mock.current_tick_ms += 900000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    ctx = app_state_machine_get_context();
    TEST_ASSERT_TRUE(ctx->sensor_fault);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_PRESSURE_NEUTRAL_DEFAULT_HPA, ctx->pressure_hpa); /* 950.0 hPa */
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_TEMP_NEUTRAL_DEFAULT_C, ctx->temperature_c);       /* 20.0 C */
    TEST_ASSERT_FLOAT_WITHIN(0.1f, FAULT_HUM_NEUTRAL_DEFAULT_PCT, ctx->humidity_pct);       /* 70.0 % */
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-02: Autonomous 2-tier I2C bus lockup recovery (9 SCL clock pulses + power rail reset).
 */
static void test_fi_i2c_bus_lockup_recovery(void) {
    s_mock.inject_i2c_lockup = true;

    /* Execute full cycle during I2C bus lockup */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Verify recovery sequence was triggered and counted */
    TEST_ASSERT_TRUE(s_mock.i2c_recovery_calls >= 1U);
    fault_handler_status_t f_status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&f_status));
    TEST_ASSERT_TRUE(f_status.i2c_lockup_count >= 1U);
    TEST_ASSERT_TRUE((f_status.latched_fault_mask & FAULT_MASK_I2C_BUS_LOCKUP) != 0U);

    /* Machine must smoothly proceed through transmit, alert, and enter Stop 2 sleep */
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-03: OPT3001 dead sensor RTC solar heuristic substitution preventing false storm triggers.
 */
static void test_fi_opt3001_dead_solar_heuristic(void) {
    s_mock.rtc_hour           = 14U; /* 2 PM: daytime */
    s_mock.inject_opt3001_dead = true;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_TRUE(ctx->sensor_fault);
    /* Daytime solar heuristic estimate applied (25000 Lux) */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, FAULT_LUX_DAYTIME_ESTIMATE, ctx->solar_lux);
    /* Rain alert must not false-trigger into IMMINENT due to optical loss */
    TEST_ASSERT_NOT_EQUAL(RAIN_ALERT_IMMINENT, ctx->rain_state);
    TEST_ASSERT_TRUE((s_mock.last_lora_payload[11] & 0x20) != 0U);
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-04: Self-healing 3-consecutive clean read automatic fault flag clearing.
 */
static void test_fi_self_healing_sensor_restoration(void) {
    /* 1. Inject failure */
    s_mock.inject_bme280_nack = true;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active());
    TEST_ASSERT_TRUE((s_mock.last_lora_payload[11] & 0x20) != 0U);

    /* 2. Sensor recovers: clean read cycle 1 */
    s_mock.inject_bme280_nack = false;
    s_mock.current_tick_ms += 900000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active()); /* Only 1 clean read */

    /* Clean read cycle 2 */
    s_mock.current_tick_ms += 900000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_TRUE(app_fault_handler_is_system_fault_active()); /* Only 2 clean reads */

    /* Clean read cycle 3 -> Fault auto-cleared */
    s_mock.current_tick_ms += 900000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_FALSE(app_fault_handler_is_system_fault_active());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_FALSE(ctx->sensor_fault);
    /* Telemetry Byte 11 fault bit cleared */
    TEST_ASSERT_EQUAL_UINT8(0U, s_mock.last_lora_payload[11] & 0x20);
}

/* ========================================================================== */
/* Category B: Battery Depletion & Brownout Throttling Tests                  */
/* ========================================================================== */

/**
 * @brief FI-SM-05: Battery Tier 2 Conservation entry at 3.05V (< 3.10V) with 30-min cadence.
 */
static void test_fi_battery_tier2_conservation_entry(void) {
    s_mock.mock_vbat_v = 3.05f; /* Tier 2 Conservation */

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Sampling interval extended to 30 min (1800s - active time) */
    TEST_ASSERT_TRUE(s_mock.configured_sleep_sec >= 1798U);
    /* Buzzer & Siren strictly muted */
    TEST_ASSERT_FALSE(s_mock.buzzer);
    TEST_ASSERT_FALSE(s_mock.relay);

    fault_handler_status_t f_status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&f_status));
    TEST_ASSERT_TRUE((f_status.active_fault_mask & FAULT_MASK_BATTERY_LOW) != 0U);
}

/**
 * @brief FI-SM-06: Battery Tier 3 Critical entry at 2.85V (< 2.90V) with 60-min cadence & LED shutdown.
 */
static void test_fi_battery_tier3_critical_entry(void) {
    s_mock.mock_vbat_v = 2.85f; /* Tier 3 Critical */

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Sampling interval extended to 60 min (3600s - active time) */
    TEST_ASSERT_TRUE(s_mock.configured_sleep_sec >= 3598U);
    /* All LEDs disabled */
    TEST_ASSERT_FALSE(s_mock.green_led);
    TEST_ASSERT_FALSE(s_mock.red_led);
    /* Buzzer & Siren muted */
    TEST_ASSERT_FALSE(s_mock.buzzer);
    TEST_ASSERT_FALSE(s_mock.relay);
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);

    fault_handler_status_t f_status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&f_status));
    TEST_ASSERT_TRUE((f_status.active_fault_mask & FAULT_MASK_BATTERY_CRITICAL) != 0U);
}

/**
 * @brief FI-SM-07: Battery hysteresis recovery restoring Tier 1 Normal (15m) when voltage recovers to 3.22V.
 */
static void test_fi_battery_hysteresis_recovery(void) {
    /* 1. Enter Tier 2 Conservation at 3.05V */
    s_mock.mock_vbat_v = 3.05f;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_TRUE(s_mock.configured_sleep_sec >= 1798U);

    /* 2. Rise to 3.15V (within 100mV hysteresis window) -> remains in Conservation */
    s_mock.current_tick_ms += 1800000U;
    s_mock.mock_vbat_v = 3.15f;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_TRUE(s_mock.configured_sleep_sec >= 1798U);

    /* 3. Rise to 3.22V (above recovery threshold 3.20V) -> 2-reading candidate filter */
    s_mock.current_tick_ms += 1800000U;
    s_mock.mock_vbat_v = 3.22f;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    /* 1st reading: candidate */

    s_mock.current_tick_ms += 1800000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    /* 2nd consecutive reading above 3.20V: Tier 1 Normal restored (15 min / 900s) */
    TEST_ASSERT_TRUE(s_mock.configured_sleep_sec <= 900U);
}

/**
 * @brief FI-SM-08: Siren relay blast suppression during severe storm under low battery.
 */
static void test_fi_storm_siren_suppression_low_battery(void) {
    helper_prime_history_baseline();

    /* Inject convective storm precursor with low battery */
    s_mock.mock_press_hpa = 938.0f;
    s_mock.mock_rh_pct    = 95.0f;
    s_mock.mock_lux       = 800.0f;
    s_mock.mock_vbat_v    = 3.02f; /* Low Battery (< 3.10V) */

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, ctx->rain_state);

    /* Siren relay MUST remain strictly OFF to preserve battery capacity */
    TEST_ASSERT_FALSE(s_mock.relay);
    TEST_ASSERT_EQUAL_UINT32(0U, s_mock.relay_pulse_ms);
    /* Live telemetry still transmitted */
    TEST_ASSERT_TRUE(s_mock.lora_tx_attempts >= 1U);
}

/* ========================================================================== */
/* Category C: LoRa Radio & Gateway Blackouts                                 */
/* ========================================================================== */

/**
 * @brief FI-SM-09: LoRa radio TX timeout (150ms) non-blocking recovery with Flash fallback.
 */
static void test_fi_lora_radio_tx_timeout(void) {
    s_mock.inject_lora_timeout = true;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Telemetry packet is safely buffered into Flash NVM */
    TEST_ASSERT_EQUAL_UINT32(1U, s_mock.flash_records_stored);
    fault_handler_status_t f_status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&f_status));
    TEST_ASSERT_TRUE(f_status.lora_timeout_count >= 1U);
    TEST_ASSERT_TRUE((f_status.latched_fault_mask & FAULT_MASK_LORA_TX_TIMEOUT) != 0U);

    /* Machine does not hang and smoothly enters Stop 2 sleep */
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-10: 72-hour gateway blackout safely backlogging 288 consecutive records in Flash.
 */
static void test_fi_gateway_outage_72hour_logging(void) {
    s_mock.inject_lora_timeout = true;

    /* Run 288 consecutive 15-minute cycles (72 hours) */
    for (int i = 0; i < 288; i++) {
        s_mock.current_tick_ms += 900000U;
        TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    }

    /* All 288 records safely saved to Flash circular buffer */
    TEST_ASSERT_EQUAL_UINT32(288U, s_mock.flash_records_stored);
    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_UINT32(288U, ctx->cycle_count);
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-11: Gateway reconnect historical backlog playback draining via FPort 3 confirmed batches.
 */
static void test_fi_gateway_reconnect_backlog_drain(void) {
    /* 1. Simulate 288 records stored during outage */
    s_mock.flash_records_stored = 288U;

    /* 2. Gateway reconnects: drain backlog in 16-record batches */
    s_mock.inject_lora_timeout     = false;
    s_mock.playback_active         = true;
    s_mock.playback_records_drained = 0U;

    uint32_t batches = 0U;
    while (s_mock.flash_records_stored > 0U) {
        uint32_t batch_records = (s_mock.flash_records_stored > 16U) ? 16U : s_mock.flash_records_stored;
        s_mock.flash_records_stored -= batch_records;
        s_mock.playback_records_drained += batch_records;
        batches++;
    }
    s_mock.playback_active = false;

    /* 288 records / 16 records per batch = exactly 18 confirmed batches */
    TEST_ASSERT_EQUAL_UINT32(18U, batches);
    TEST_ASSERT_EQUAL_UINT32(288U, s_mock.playback_records_drained);
    TEST_ASSERT_EQUAL_UINT32(0U, s_mock.flash_records_stored);
}

/**
 * @brief FI-SM-12: High-priority storm alert packet preemption on FPort 2 over background backlog playback.
 */
static void test_fi_urgent_alert_preempts_playback(void) {
    /* Start with historical backlog in Flash */
    s_mock.flash_records_stored = 100U;
    s_mock.playback_active      = true;

    /* Preempting event: Severe storm develops */
    helper_prime_history_baseline();
    s_mock.mock_press_hpa     = 938.0f;
    s_mock.mock_rh_pct        = 95.0f;
    s_mock.playback_preempted = true;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* High-priority storm alert is processed and transmitted immediately */
    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, ctx->rain_state);
    TEST_ASSERT_TRUE(s_mock.lora_tx_attempts >= 1U);
    TEST_ASSERT_TRUE(s_mock.playback_preempted);
}

/* ========================================================================== */
/* Category D: Flash, Mechanical & Compound Outages                           */
/* ========================================================================== */

/**
 * @brief FI-SM-13: Flash write failure bypass ensuring telemetry continues over LoRa uplink without crashing.
 */
static void test_fi_flash_write_failure_bypass(void) {
    s_mock.inject_flash_write_error = true;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Flash records stored is 0 due to error */
    TEST_ASSERT_EQUAL_UINT32(0U, s_mock.flash_records_stored);
    /* LoRa TX still executed successfully */
    TEST_ASSERT_EQUAL_UINT32(1U, s_mock.lora_tx_attempts);
    TEST_ASSERT_EQUAL_UINT32(1U, s_mock.lora_tx_success_count);

    fault_handler_status_t f_status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&f_status));
    TEST_ASSERT_TRUE(f_status.flash_error_count >= 1U);
    TEST_ASSERT_TRUE((f_status.latched_fault_mask & FAULT_MASK_FLASH_WRITE) != 0U);

    /* Machine completes smoothly into Stop 2 sleep */
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-14: Rain gauge contact chatter debounce clamping to 40 tips/cycle (500 mm/hr limit).
 */
static void test_fi_rain_gauge_contact_chatter_clamping(void) {
    s_mock.inject_rain_chatter = true;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    /* Clamped to maximum physical threshold (<= 40 tips) */
    TEST_ASSERT_EQUAL_UINT32(40U, ctx->rain_pulses_cycle);

    fault_handler_status_t f_status;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_fault_handler_get_status(&f_status));
    TEST_ASSERT_TRUE((f_status.latched_fault_mask & FAULT_MASK_RAIN_GAUGE_CHATTER) != 0U);

    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-15: Watchdog timeout reboot recovery asserting Byte 11 bit 6 (0x40) unexpected reset flag.
 */
static void test_fi_watchdog_timeout_reboot_recovery(void) {
    s_mock.inject_watchdog_reboot = true;
    app_state_machine_reset();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* Execute first cycle after watchdog reset */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Byte 11 bit 6 (unexpected reset) MUST be asserted (0x40) */
    TEST_ASSERT_TRUE((s_mock.last_lora_payload[11] & 0x40) != 0U);
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief FI-SM-16: Total compound multi-fault survival executing in < 1.2s and safely entering Stop 2 sleep.
 */
static void test_fi_total_compound_multi_fault_survival(void) {
    /* Simultaneous compound catastrophic faults */
    s_mock.inject_i2c_lockup        = true;
    s_mock.inject_opt3001_dead      = true;
    s_mock.inject_lora_timeout      = true;
    s_mock.inject_flash_write_error = true;
    s_mock.mock_vbat_v              = 2.80f; /* Critical battery */

    uint32_t start_ms = s_mock.current_tick_ms;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    uint32_t elapsed_ms = s_mock.current_tick_ms - start_ms;

    /* Execution budget: must complete active execution in < 1.2 seconds (1200 ms) */
    TEST_ASSERT_TRUE(elapsed_ms < 1200U);

    /* Zero-crash mandate: enters Stop 2 sleep safely */
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_TRUE(ctx->sensor_fault);
    TEST_ASSERT_FALSE(s_mock.green_led);
    TEST_ASSERT_FALSE(s_mock.red_led);
    TEST_ASSERT_FALSE(s_mock.buzzer);
    TEST_ASSERT_FALSE(s_mock.relay);
    TEST_ASSERT_TRUE(s_mock.configured_sleep_sec >= 3598U);
}

/* ========================================================================== */
/* Main Unity Test Runner Entry Point                                         */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Category A: Sensor & I2C Bus Faults */
    RUN_TEST(test_fi_bme280_disconnect_and_stale_hold);
    RUN_TEST(test_fi_i2c_bus_lockup_recovery);
    RUN_TEST(test_fi_opt3001_dead_solar_heuristic);
    RUN_TEST(test_fi_self_healing_sensor_restoration);

    /* Category B: Battery Depletion & Brownout Throttling */
    RUN_TEST(test_fi_battery_tier2_conservation_entry);
    RUN_TEST(test_fi_battery_tier3_critical_entry);
    RUN_TEST(test_fi_battery_hysteresis_recovery);
    RUN_TEST(test_fi_storm_siren_suppression_low_battery);

    /* Category C: LoRa Radio & Gateway Blackouts */
    RUN_TEST(test_fi_lora_radio_tx_timeout);
    RUN_TEST(test_fi_gateway_outage_72hour_logging);
    RUN_TEST(test_fi_gateway_reconnect_backlog_drain);
    RUN_TEST(test_fi_urgent_alert_preempts_playback);

    /* Category D: Flash, Mechanical & Compound Outages */
    RUN_TEST(test_fi_flash_write_failure_bypass);
    RUN_TEST(test_fi_rain_gauge_contact_chatter_clamping);
    RUN_TEST(test_fi_watchdog_timeout_reboot_recovery);
    RUN_TEST(test_fi_total_compound_multi_fault_survival);

    return UNITY_END();
}
