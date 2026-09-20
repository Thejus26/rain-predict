/**
 * @file    test_state_machine.c
 * @brief   Full System State Machine End-to-End Integration Test Suite (S6-T4.1).
 * @details Validates the entire 4-layer embedded firmware stack executing across
 *          all 8 states under simulated tea estate meteorological missions.
 *          Implements the 16-point integration test matrix (IT-SM-01 to IT-SM-16).
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
/* Hardware & Peripheral Mock Implementation                                  */
/* ========================================================================== */

typedef struct {
    /* Switched Power Rails */
    bool                sensor_rail_en;
    uint32_t            stabilization_delay_count;

    /* Indicators & Actuators */
    bool                green_led;
    bool                red_led;
    bool                buzzer;
    bool                relay;
    uint32_t            relay_pulse_ms;

    /* Environmental & Physical Sensors */
    float               mock_temp_c;
    float               mock_rh_pct;
    float               mock_press_hpa;
    float               mock_lux;
    uint32_t            mock_rain_tips;
    bool                rain_active_flag;
    float               mock_vbat_v;

    /* LoRaWAN Radio Subsystem */
    uint8_t             last_lora_payload[64];
    uint8_t             last_lora_len;
    uint8_t             last_lora_fport;
    uint32_t            lora_tx_count;

    /* On-chip Flash NVM Storage */
    uint8_t             last_flash_payload[64];
    uint16_t            last_flash_seq_id;
    uint16_t            last_flash_magic;
    uint32_t            flash_records_stored;

    /* System Timing, Power & Sleep */
    uint32_t            current_tick_ms;
    uint32_t            last_configured_sleep_sec;
    bool                in_stop2_sleep;
    power_wake_reason_t wake_reason;
    uint8_t             rtc_hour;

    /* Watchdog Supervisor */
    uint32_t            watchdog_kicks;
    bool                mock_watchdog_reset_flag;
    bool                watchdog_enabled;
    uint32_t            watchdog_timeout_ms;

    /* Bus Diagnostics */
    uint32_t            i2c_recover_count;
} full_system_mock_t;

static full_system_mock_t s_mock;

/* --- Switched Power Rail Stubs --- */

status_t bsp_power_rails_init(void) {
    s_mock.sensor_rail_en = false;
    return STATUS_OK;
}

status_t bsp_power_rail_enable(bsp_power_rail_t rail, bool enable) {
    if (rail == BSP_POWER_RAIL_SENSORS) {
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
    s_mock.green_led = false;
    s_mock.red_led = false;
    s_mock.buzzer = false;
    s_mock.relay = false;
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
    s_mock.relay = true;
    s_mock.relay_pulse_ms = duration_ms;
    return STATUS_OK;
}

status_t bsp_indicators_all_off(void) {
    s_mock.green_led = false;
    s_mock.red_led = false;
    s_mock.buzzer = false;
    s_mock.relay = false;
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
    if (p_raw_vref == NULL) return STATUS_ERR_NULL_PTR;
    *p_raw_vref = 1660U;
    return STATUS_OK;
}

status_t bsp_adc_read_vbat_raw(uint16_t *p_raw_vbat) {
    if (p_raw_vbat == NULL) return STATUS_ERR_NULL_PTR;
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

/* --- Environmental & Optical Sensor Stubs --- */

status_t bme280_init(bme280_t *dev, uint8_t dev_addr) {
    (void)dev;
    (void)dev_addr;
    return STATUS_OK;
}

status_t bme280_configure(bme280_t *dev, const bme280_config_t *cfg) {
    (void)dev;
    (void)cfg;
    return STATUS_OK;
}

status_t bme280_read_data(bme280_t *dev, bme280_data_t *p_data) {
    (void)dev;
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
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
    uint16_t tips = (uint16_t)s_mock.mock_rain_tips;
    if (p_interval_mm != NULL) {
        *p_interval_mm = (float)tips * RAIN_GAUGE_CALIB_MM_PER_TIP;
    }
    s_mock.mock_rain_tips = 0U;
    return tips;
}

bool rain_gauge_is_rain_active(void) {
    return (s_mock.mock_rain_tips > 0U || s_mock.rain_active_flag);
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
    s_mock.last_configured_sleep_sec = interval_sec;
    return STATUS_OK;
}

status_t power_mgr_enter_stop2(uint32_t sleep_duration_sec) {
    s_mock.in_stop2_sleep = true;
    s_mock.last_configured_sleep_sec = sleep_duration_sec;
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
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    memcpy(s_mock.last_flash_payload, payload, 12);
    s_mock.last_flash_seq_id = seq_id;
    s_mock.last_flash_magic  = FLASH_RING_MAGIC_VALID;
    s_mock.flash_records_stored++;
    return STATUS_OK;
}

/* --- LoRaWAN Network Service Stubs --- */

status_t lorawan_service_init(const lorawan_config_t *config) {
    (void)config;
    s_mock.lora_tx_count = 0U;
    return STATUS_OK;
}

status_t lorawan_send_unconfirmed(uint8_t fport, const uint8_t *payload, uint8_t length) {
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    s_mock.last_lora_fport = fport;
    s_mock.last_lora_len   = length;
    memcpy(s_mock.last_lora_payload, payload, length);
    s_mock.lora_tx_count++;
    return STATUS_OK;
}

status_t lorawan_service_process_step(void) {
    return STATUS_OK;
}

/* --- Watchdog & I2C Bus Stubs --- */

status_t watchdog_init(uint32_t timeout_ms) {
    s_mock.watchdog_enabled = true;
    s_mock.watchdog_timeout_ms = timeout_ms;
    return STATUS_OK;
}

void watchdog_refresh(void) {
    s_mock.watchdog_kicks++;
}

bool watchdog_was_reset_by_watchdog(void) {
    return s_mock.mock_watchdog_reset_flag;
}

void watchdog_clear_reset_flags(void) {
    s_mock.mock_watchdog_reset_flag = false;
}

status_t i2c_bus_recover(void) {
    s_mock.i2c_recover_count++;
    return STATUS_OK;
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
    app_state_machine_reset();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
}

void tearDown(void) {
    (void)alert_manager_force_all_off();
    app_state_machine_reset();
}

/* ========================================================================== */
/* Comprehensive 16-Point Integration Test Matrix (IT-SM-01 to IT-SM-16)     */
/* ========================================================================== */

/**
 * @brief IT-SM-01: Cold boot initialization & safe initial defaults.
 */
static void test_integration_cold_boot_initialization(void) {
    TEST_ASSERT_EQUAL_INT(STATE_WAKE, app_state_machine_get_current_state());
    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_UINT32(0U, ctx->cycle_count);
    TEST_ASSERT_EQUAL_INT(STATE_WAKE, ctx->current_state);
    TEST_ASSERT_EQUAL_INT(STATE_SLEEP, ctx->previous_state);
    TEST_ASSERT_FALSE(s_mock.sensor_rail_en);
    TEST_ASSERT_TRUE(s_mock.watchdog_enabled);
    TEST_ASSERT_EQUAL_UINT32(8000UL, s_mock.watchdog_timeout_ms);
}

/**
 * @brief IT-SM-02: Single nominal quiescent 8-state cycle execution.
 */
static void test_integration_single_quiescent_cycle(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_UINT32(1U, ctx->cycle_count);
    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_UNLIKELY, ctx->rain_state);
    TEST_ASSERT_EQUAL_UINT32(1U, s_mock.lora_tx_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_mock.flash_records_stored);
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
    TEST_ASSERT_EQUAL_UINT32(8U, app_state_machine_get_watchdog_kick_count());
    TEST_ASSERT_TRUE(s_mock.watchdog_kicks >= 8U);
}

/**
 * @brief IT-SM-03: Switched power rail PA4 load switch & 20ms RC stabilization guard.
 */
static void test_integration_power_rail_stabilization(void) {
    TEST_ASSERT_EQUAL_INT(STATE_WAKE, app_state_machine_get_current_state());

    /* Step: STATE_WAKE -> STATE_POWER_ON */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    TEST_ASSERT_EQUAL_INT(STATE_POWER_ON, app_state_machine_get_current_state());

    /* Step: STATE_POWER_ON -> STATE_SAMPLE (energizes rail and delays 20ms) */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    TEST_ASSERT_EQUAL_INT(STATE_SAMPLE, app_state_machine_get_current_state());
    TEST_ASSERT_TRUE(s_mock.sensor_rail_en);
    TEST_ASSERT_TRUE(s_mock.stabilization_delay_count >= 1U);
}

/**
 * @brief IT-SM-04: 12-byte LoRaWAN binary telemetry packet generation & big-endian encoding.
 */
static void test_integration_telemetry_packet_generation(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    TEST_ASSERT_EQUAL_UINT8(LORAWAN_FPORT_PERIODIC, s_mock.last_lora_fport);
    TEST_ASSERT_EQUAL_UINT8(TELEMETRY_PERIODIC_PAYLOAD_SIZE, s_mock.last_lora_len);

    telemetry_periodic_data_t telem;
    memset(&telem, 0, sizeof(telem));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, telemetry_decode_periodic(s_mock.last_lora_payload,
                                                              s_mock.last_lora_len,
                                                              &telem));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 24.5f, telem.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 65.0f, telem.humidity_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 952.0f, telem.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(50.0f, 55000.0f, telem.ambient_lux);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.30f, telem.battery_voltage_v);
    TEST_ASSERT_FALSE(telem.sensor_fault);
}

/**
 * @brief IT-SM-05: On-chip Flash circular ring buffer NVM push & 0xAA55 magic validation.
 */
static void test_integration_flash_nvm_logging(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    TEST_ASSERT_EQUAL_UINT32(1U, s_mock.flash_records_stored);
    TEST_ASSERT_EQUAL_HEX16(FLASH_RING_MAGIC_VALID, s_mock.last_flash_magic);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(s_mock.last_lora_payload, s_mock.last_flash_payload, 12);
}

/**
 * @brief IT-SM-06: Active CPU execution time compensation for RTC sleep interval.
 */
static void test_integration_active_time_compensation(void) {
    s_mock.current_tick_ms = 1000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    /* Nominal interval is 900s; active CPU window (stabilization 20ms) compensates to 899s..900s */
    TEST_ASSERT_TRUE(s_mock.last_configured_sleep_sec >= 898U &&
                     s_mock.last_configured_sleep_sec <= 900U);
}

/**
 * @brief IT-SM-07: Pre-sleep hardware de-energization and Stop 2 deep sleep entry.
 */
static void test_integration_pre_sleep_hardware_shutdown(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    TEST_ASSERT_FALSE(s_mock.sensor_rail_en);
    TEST_ASSERT_FALSE(s_mock.green_led);
    TEST_ASSERT_FALSE(s_mock.red_led);
    TEST_ASSERT_FALSE(s_mock.buzzer);
    TEST_ASSERT_FALSE(s_mock.relay);
    TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
}

/**
 * @brief IT-SM-08: Storm precursor barometric drop accelerating cadence to 300s (Storm Watch).
 */
static void test_integration_storm_watch_acceleration(void) {
    helper_prime_history_baseline();

    /* Inject falling pressure precursor (-1.5 hPa/hr drop) */
    s_mock.mock_press_hpa = 950.5f;
    s_mock.mock_rh_pct    = 86.0f;
    s_mock.current_tick_ms += 600000U;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_STORM_WATCH, ctx->sched_decision.active_mode);
    TEST_ASSERT_TRUE(s_mock.last_configured_sleep_sec <= 300U);
}

/**
 * @brief IT-SM-09: Imminent convective storm detection, Red 10 Hz strobe & 10s siren blast.
 */
static void test_integration_imminent_storm_siren_actuation(void) {
    helper_prime_history_baseline();

    /* Inject severe convective storm (pressure plunge > -2.0 hPa/hr with RH >= 88%) */
    s_mock.mock_press_hpa = 938.0f;
    s_mock.mock_rh_pct    = 95.0f;
    s_mock.mock_lux       = 800.0f; /* Convective cloud daylight blackout */
    s_mock.current_tick_ms += 600000U;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, ctx->rain_state);
    TEST_ASSERT_EQUAL_UINT32(10000U, s_mock.relay_pulse_ms);
}

/**
 * @brief IT-SM-10: Siren relay 30-minute anti-chatter cooldown suppression on subsequent cycle.
 */
static void test_integration_siren_cooldown_suppression(void) {
    helper_prime_history_baseline();

    /* 1. Cycle 1: Trigger initial siren blast */
    s_mock.mock_press_hpa = 938.0f;
    s_mock.mock_rh_pct    = 95.0f;
    s_mock.mock_lux       = 800.0f;
    s_mock.current_tick_ms += 600000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_EQUAL_UINT32(10000U, s_mock.relay_pulse_ms);

    /* 2. Clear relay tracker and advance 5 minutes (within 30-min cooldown) */
    s_mock.relay_pulse_ms = 0U;
    s_mock.current_tick_ms += 300000U;

    /* 3. Cycle 2: Storm persists; alert manager must suppress siren due to cooldown */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, ctx->rain_state);
    TEST_ASSERT_EQUAL_UINT32(0U, s_mock.relay_pulse_ms);
}

/**
 * @brief IT-SM-11: Active rainfall tipping bucket tips promoting rapid 120s cadence.
 */
static void test_integration_active_rain_rapid_cadence(void) {
    s_mock.mock_rain_tips = 8U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_ACTIVE_RAIN, ctx->sched_decision.active_mode);
    TEST_ASSERT_TRUE(s_mock.last_configured_sleep_sec <= 120U);
    TEST_ASSERT_EQUAL_UINT32(8U, ctx->rain_pulses_cycle);
}

/**
 * @brief IT-SM-12: Anti-chatter 30-minute consecutive calm hold-down recovery before 15m restore.
 */
static void test_integration_calm_hold_down_recovery(void) {
    helper_prime_history_baseline();

    /* 1. Trigger Storm Watch mode (5 min) */
    s_mock.mock_press_hpa = 950.0f;
    s_mock.mock_rh_pct    = 86.0f;
    s_mock.current_tick_ms += 600000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_STORM_WATCH,
                          app_state_machine_get_context()->sched_decision.active_mode);

    /* 2. Weather clears: Run 5 consecutive calm 5-min cycles (25 min elapsed) */
    s_mock.mock_press_hpa = 952.0f;
    s_mock.mock_rh_pct    = 60.0f;
    s_mock.mock_lux       = 50000.0f;

    for (int i = 0; i < 5; i++) {
        s_mock.current_tick_ms += 300000U;
        TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
        /* System must remain in Storm Watch mode during 30-min hold-down */
        TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_STORM_WATCH,
                              app_state_machine_get_context()->sched_decision.active_mode);
    }

    /* 3. Cycle 6 (30 consecutive calm minutes reached): Should release to Nominal (15m) */
    s_mock.current_tick_ms += 300000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_NOMINAL, ctx->sched_decision.active_mode);
    TEST_ASSERT_TRUE(s_mock.last_configured_sleep_sec >= 898U &&
                     s_mock.last_configured_sleep_sec <= 900U);
}

/**
 * @brief IT-SM-13: Downlink sampling interval override (600s) and autonomous restore.
 */
static void test_integration_downlink_schedule_override(void) {
    /* 1. Apply 10-minute remote override (600s) */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, measurement_scheduler_set_override_interval(600U));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_OVERRIDE, ctx->sched_decision.active_mode);
    TEST_ASSERT_TRUE(s_mock.last_configured_sleep_sec <= 600U &&
                     s_mock.last_configured_sleep_sec >= 598U);

    /* 2. Clear downlink override -> returns to autonomous nominal mode (900s) */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, measurement_scheduler_clear_override());
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());

    ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(SCHEDULER_MODE_NOMINAL, ctx->sched_decision.active_mode);
    TEST_ASSERT_TRUE(s_mock.last_configured_sleep_sec >= 898U &&
                     s_mock.last_configured_sleep_sec <= 900U);
}

/**
 * @brief IT-SM-14: EXTI rain pulse wakeup during Stop 2 deep sleep.
 */
static void test_integration_exti_rain_wake_handling(void) {
    /* Simulate wake event from PA0 tipping bucket EXTI line */
    s_mock.wake_reason    = POWER_WAKE_REASON_RAIN_EXTI;
    s_mock.mock_rain_tips = 3U;

    /* Step WAKE state */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_REASON_RAIN_EXTI, ctx->wake_reason);

    /* Complete full cycle */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_UINT32(3U, ctx->rain_pulses_cycle);
}

/**
 * @brief IT-SM-15: Continuous 24-hour mission (96 consecutive 15-min cycles) execution.
 */
static void test_integration_continuous_24hour_mission(void) {
    for (int cycle = 0; cycle < 96; cycle++) {
        s_mock.current_tick_ms += 900000U; /* 15 min */
        TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    }

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_UINT32(96U, ctx->cycle_count);
    TEST_ASSERT_EQUAL_UINT32(96U, s_mock.lora_tx_count);
    TEST_ASSERT_EQUAL_UINT32(96U, s_mock.flash_records_stored);
    TEST_ASSERT_TRUE(s_mock.watchdog_kicks >= (96U * 8U));
}

/**
 * @brief IT-SM-16: Active CPU processing window bounded strictly to < 1.2 seconds (< 1200 ms).
 */
static void test_integration_processing_budget_compliance(void) {
    uint32_t start_ms = s_mock.current_tick_ms;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    uint32_t elapsed_ms = s_mock.current_tick_ms - start_ms;

    /* Assert execution completed inside 1.2s budget */
    TEST_ASSERT_TRUE(elapsed_ms < 1200U);

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_TRUE(ctx->active_duration_ms < 1200U);
}

/* ========================================================================== */
/* Main Unity Test Runner                                                     */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_integration_cold_boot_initialization);
    RUN_TEST(test_integration_single_quiescent_cycle);
    RUN_TEST(test_integration_power_rail_stabilization);
    RUN_TEST(test_integration_telemetry_packet_generation);
    RUN_TEST(test_integration_flash_nvm_logging);
    RUN_TEST(test_integration_active_time_compensation);
    RUN_TEST(test_integration_pre_sleep_hardware_shutdown);
    RUN_TEST(test_integration_storm_watch_acceleration);
    RUN_TEST(test_integration_imminent_storm_siren_actuation);
    RUN_TEST(test_integration_siren_cooldown_suppression);
    RUN_TEST(test_integration_active_rain_rapid_cadence);
    RUN_TEST(test_integration_calm_hold_down_recovery);
    RUN_TEST(test_integration_downlink_schedule_override);
    RUN_TEST(test_integration_exti_rain_wake_handling);
    RUN_TEST(test_integration_continuous_24hour_mission);
    RUN_TEST(test_integration_processing_budget_compliance);

    return UNITY_END();
}
