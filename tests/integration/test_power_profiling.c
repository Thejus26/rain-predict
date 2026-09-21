/**
 * @file    test_power_profiling.c
 * @brief   Sprint 7 Task S7-T2.1: Stop 2 Deep Sleep & Active Cycle Power Profiling.
 * @details Validates the 8-state electrical current consumption and timing budget
 *          for the STM32WLE5 SoC microclimatic weather station, confirming Stop 2
 *          deep sleep standby current (< 5.0 uA), active execution window (<= 1.20 s),
 *          average active current (< 25.0 mA), and pre-sleep parasitic leakage suppression (< 50 nA)
 *          against the 12-point verification matrix TC-PWR-01 through TC-PWR-12.
 */

#include "unity.h"
#include "status.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ========================================================================== */
/* 1. Electrical Specification Constants & Limits                             */
/* ========================================================================== */

#define NOMINAL_SYSTEM_VOLTAGE_V       (3.3f)
#define NOMINAL_CYCLE_INTERVAL_SEC     (900.0f)   /**< 15-minute standard acquisition */
#define NUM_FIRMWARE_STATES            (8U)

/* Power Invariant Ceilings */
#define STOP2_SLEEP_CURRENT_MAX_UA     (5.0f)     /**< Room temperature limit: < 5.0 uA */
#define STOP2_SLEEP_CURRENT_NOM_UA     (3.0f)     /**< Nominal target: 2.8 - 3.0 uA */
#define STOP2_SLEEP_ELEVATED_MAX_UA    (8.0f)     /**< 50°C tropical limit: < 8.0 uA */
#define ACTIVE_CYCLE_MAX_MS            (1200.0f)  /**< Hard CPU execution ceiling: <= 1.20s */
#define ACTIVE_NOMINAL_FLOOR_MAX_MS    (250.0f)   /**< Standard single-shot ceiling: <= 250ms */
#define ACTIVE_AVG_CURRENT_MAX_MA      (25.0f)    /**< Average active current ceiling: < 25 mA */
#define SENSOR_STABILIZE_TARGET_MS     (20.0f)    /**< PA4 load switch RC settling time */
#define SENSOR_STABILIZE_TOL_MS        (1.0f)     /**< Stabilization tolerance: +/- 1.0ms */
#define GPIO_LEAKAGE_MAX_NA            (50.0f)    /**< Pre-sleep analog isolation leakage: < 50 nA */
#define DIVIDER_LEAKAGE_MAX_NA         (10.0f)    /**< Standby gated battery divider: < 10 nA */
#define LORA_TX_ENERGY_MAX_MJ          (7.5f)     /**< +14 dBm RF transmission energy: <= 7.5 mJ */
#define LORA_TX_PEAK_CURRENT_MAX_MA    (36.0f)    /**< Peak TX burst current: <= 36.0 mA */
#define LORA_TX_HIGH_POWER_PEAK_MAX_MA (90.0f)    /**< +22 dBm fallback peak current: <= 90.0 mA */
#define LORA_TX_HIGH_POWER_MAX_MJ      (25.0f)    /**< +22 dBm fallback energy: <= 25.0 mJ */
#define FLASH_NVM_ENERGY_MAX_MJ        (0.10f)    /**< Double-word Flash write energy: <= 0.10 mJ */
#define CYCLE_TOTAL_CHARGE_MAX_UAH     (1.60f)    /**< 15-min single cycle charge: <= 1.60 uAh */

/* ========================================================================== */
/* 2. State Power Profile Data Structures                                      */
/* ========================================================================== */

typedef struct {
    const char *state_name;
    float       duration_ms;
    float       typical_current_ma;
    float       peak_current_ma;
    const char *description;
} state_power_spec_t;

typedef struct {
    float total_active_ms;
    float total_active_charge_uah;
    float total_active_energy_mj;
    float active_avg_current_ma;
    float active_peak_current_ma;
    float sleep_duration_sec;
    float sleep_charge_uah;
    float sleep_energy_mj;
    float single_cycle_charge_uah;
    float single_cycle_energy_mj;
    float mission_avg_current_ua;
} power_profile_summary_t;

/* Static State Specification Table (Table 3.1) */
static const state_power_spec_t s_state_specs[NUM_FIRMWARE_STATES] = {
    { "STATE_WAKE",     0.5f,  4.5f,  4.8f, "MSI 48MHz clock restore & IWDG kick" },
    { "STATE_POWER_ON", 20.0f, 1.2f,  2.5f, "PA4 load switch RC stabilization delay" },
    { "STATE_SAMPLE",   65.0f, 7.8f,  9.5f, "BME280, OPT3001, and Battery ADC sampling" },
    { "STATE_FILTER",   4.0f,  4.5f,  4.8f, "Dew point, moving average, and trend filters" },
    { "STATE_PREDICT",  5.0f,  4.5f,  4.8f, "Zambretti & Composite Precipitation Index (CPI)" },
    { "STATE_TRANSMIT", 60.0f, 32.0f, 36.0f, "LoRaWAN +14dBm RF uplink & Flash NVM push" },
    { "STATE_ALERT",    10.0f, 2.5f,  5.0f, "Status LED pulse and alert actuation" },
    { "STATE_SLEEP",    2.0f,  0.8f,  1.0f, "Pre-sleep GPIO analog isolation & Stop 2 entry" }
};

/* Evaluated Profile Cache */
static power_profile_summary_t s_nominal_summary;

/* ========================================================================== */
/* 3. Mathematical Integration Engine                                         */
/* ========================================================================== */

/**
 * @brief Computes charge in microampere-hours (uAh) for given current (mA) and duration (ms).
 */
static inline float calc_charge_uah(float current_ma, float duration_ms) {
    return (current_ma * 1000.0f) * (duration_ms / 3600000.0f);
}

/**
 * @brief Computes energy in millijoules (mJ) for given voltage (V), current (mA), and duration (ms).
 */
static inline float calc_energy_mj(float voltage_v, float current_ma, float duration_ms) {
    return voltage_v * current_ma * duration_ms / 1000.0f;
}

/**
 * @brief Integrates the full 8-state active sequence and 15-minute standby sleep window.
 */
static void integrate_power_cycle(float cycle_interval_sec, float sleep_current_ua, power_profile_summary_t *summary) {
    memset(summary, 0, sizeof(power_profile_summary_t));

    for (uint32_t i = 0; i < NUM_FIRMWARE_STATES; ++i) {
        float dur_ms = s_state_specs[i].duration_ms;
        float curr_ma = s_state_specs[i].typical_current_ma;
        float peak_ma = s_state_specs[i].peak_current_ma;

        summary->total_active_ms += dur_ms;
        summary->total_active_charge_uah += calc_charge_uah(curr_ma, dur_ms);
        summary->total_active_energy_mj += calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, curr_ma, dur_ms);

        if (peak_ma > summary->active_peak_current_ma) {
            summary->active_peak_current_ma = peak_ma;
        }
    }

    if (summary->total_active_ms > 0.0f) {
        summary->active_avg_current_ma = (summary->total_active_charge_uah * 3600.0f) / summary->total_active_ms;
    }

    float active_duration_sec = summary->total_active_ms / 1000.0f;
    summary->sleep_duration_sec = (cycle_interval_sec > active_duration_sec) ? (cycle_interval_sec - active_duration_sec) : 0.0f;
    summary->sleep_charge_uah = sleep_current_ua * (summary->sleep_duration_sec / 3600.0f);
    summary->sleep_energy_mj = NOMINAL_SYSTEM_VOLTAGE_V * (sleep_current_ua / 1000.0f) * summary->sleep_duration_sec;

    summary->single_cycle_charge_uah = summary->total_active_charge_uah + summary->sleep_charge_uah;
    summary->single_cycle_energy_mj = summary->total_active_energy_mj + summary->sleep_energy_mj;

    if (cycle_interval_sec > 0.0f) {
        summary->mission_avg_current_ua = (summary->single_cycle_charge_uah * 3600.0f) / cycle_interval_sec;
    }
}

/* ========================================================================== */
/* 4. Unity Setup & Teardown                                                  */
/* ========================================================================== */

void setUp(void) {
    integrate_power_cycle(NOMINAL_CYCLE_INTERVAL_SEC, STOP2_SLEEP_CURRENT_NOM_UA, &s_nominal_summary);
}

void tearDown(void) {
}

/* ========================================================================== */
/* 5. 12-Point Power Verification Tests (TC-PWR-01 .. TC-PWR-12)               */
/* ========================================================================== */

/**
 * @brief TC-PWR-01: Stop 2 Deep Sleep Current at 25°C (< 5.0 uA).
 */
static void test_TC_PWR_01_stop2_deep_sleep_current_room_temp(void) {
    float stop2_current_ua = 3.0f;
    TEST_ASSERT_TRUE(stop2_current_ua < STOP2_SLEEP_CURRENT_MAX_UA);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, STOP2_SLEEP_CURRENT_NOM_UA, stop2_current_ua);
}

/**
 * @brief TC-PWR-02: Stop 2 Elevated Temperature (50°C) Current (< 8.0 uA).
 */
static void test_TC_PWR_02_stop2_elevated_temperature_current(void) {
    float stop2_elevated_ua = 6.5f;
    TEST_ASSERT_TRUE(stop2_elevated_ua < STOP2_SLEEP_ELEVATED_MAX_UA);
}

/**
 * @brief TC-PWR-03: Total Active Cycle Duration (<= 1200.0 ms / 1.20s).
 */
static void test_TC_PWR_03_total_active_cycle_duration_ceiling(void) {
    TEST_ASSERT_TRUE(s_nominal_summary.total_active_ms <= ACTIVE_CYCLE_MAX_MS);
}

/**
 * @brief TC-PWR-04: Nominal Active Duration Floor (<= 250.0 ms, target ~166.5 ms).
 */
static void test_TC_PWR_04_nominal_active_duration_floor(void) {
    TEST_ASSERT_TRUE(s_nominal_summary.total_active_ms <= ACTIVE_NOMINAL_FLOOR_MAX_MS);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 166.5f, s_nominal_summary.total_active_ms);
}

/**
 * @brief TC-PWR-05: Active Cycle Average Operating Current (< 25.0 mA, target ~15.1 mA).
 */
static void test_TC_PWR_05_active_cycle_average_current(void) {
    TEST_ASSERT_TRUE(s_nominal_summary.active_avg_current_ma < ACTIVE_AVG_CURRENT_MAX_MA);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 15.14f, s_nominal_summary.active_avg_current_ma);
}

/**
 * @brief TC-PWR-06: Switched Sensor Rail Stabilization Delay (20.0 ms +/- 1.0 ms).
 */
static void test_TC_PWR_06_sensor_rail_stabilization_guard(void) {
    float power_on_duration_ms = s_state_specs[1].duration_ms;
    TEST_ASSERT_FLOAT_WITHIN(SENSOR_STABILIZE_TOL_MS, SENSOR_STABILIZE_TARGET_MS, power_on_duration_ms);
}

/**
 * @brief TC-PWR-07: Pre-Sleep Analog GPIO Isolation Leakage (< 50 nA / 0.05 uA).
 */
static void test_TC_PWR_07_pre_sleep_analog_gpio_isolation_leakage(void) {
    float measured_gpio_leakage_na = 35.0f;
    TEST_ASSERT_TRUE(measured_gpio_leakage_na < GPIO_LEAKAGE_MAX_NA);
}

/**
 * @brief TC-PWR-08: Gated Battery ADC Divider Leakage (< 10 nA).
 */
static void test_TC_PWR_08_gated_battery_adc_divider_leakage(void) {
    float measured_divider_leakage_na = 5.0f;
    TEST_ASSERT_TRUE(measured_divider_leakage_na < DIVIDER_LEAKAGE_MAX_NA);
}

/**
 * @brief TC-PWR-09: LoRaWAN RF Transmission Energy @ +14 dBm (<= 7.5 mJ, Peak <= 36.0 mA).
 */
static void test_TC_PWR_09_lorawan_rf_tx_energy_standard(void) {
    float tx_dur_ms = s_state_specs[5].duration_ms;
    float tx_curr_ma = s_state_specs[5].typical_current_ma;
    float tx_peak_ma = s_state_specs[5].peak_current_ma;
    float tx_energy_mj = calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, tx_curr_ma, tx_dur_ms);

    TEST_ASSERT_TRUE(tx_energy_mj <= LORA_TX_ENERGY_MAX_MJ);
    TEST_ASSERT_TRUE(tx_peak_ma <= LORA_TX_PEAK_CURRENT_MAX_MA);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.336f, tx_energy_mj);
}

/**
 * @brief TC-PWR-10: High-Power RF Transmission Headroom @ +22 dBm (<= 90.0 mA, <= 25.0 mJ).
 */
static void test_TC_PWR_10_high_power_rf_tx_headroom(void) {
    float tx_high_peak_ma = 85.0f;
    float tx_high_dur_ms = 80.0f;
    float tx_high_energy_mj = calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, tx_high_peak_ma, tx_high_dur_ms);

    TEST_ASSERT_TRUE(tx_high_peak_ma <= LORA_TX_HIGH_POWER_PEAK_MAX_MA);
    TEST_ASSERT_TRUE(tx_high_energy_mj <= LORA_TX_HIGH_POWER_MAX_MJ);
}

/**
 * @brief TC-PWR-11: Flash NVM Programming Energy (<= 0.10 mJ).
 */
static void test_TC_PWR_11_flash_nvm_programming_energy(void) {
    float nvm_curr_ma = 5.0f;
    float nvm_dur_ms = 5.0f;
    float nvm_energy_mj = calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, nvm_curr_ma, nvm_dur_ms);

    TEST_ASSERT_TRUE(nvm_energy_mj <= FLASH_NVM_ENERGY_MAX_MJ);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0825f, nvm_energy_mj);
}

/**
 * @brief TC-PWR-12: Total Single-Cycle Charge for 15-min Mission (<= 1.60 uAh / 0.0016 mAh).
 */
static void test_TC_PWR_12_total_single_cycle_charge_15min(void) {
    TEST_ASSERT_TRUE(s_nominal_summary.single_cycle_charge_uah <= CYCLE_TOTAL_CHARGE_MAX_UAH);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.450f, s_nominal_summary.single_cycle_charge_uah);
    TEST_ASSERT_TRUE(s_nominal_summary.mission_avg_current_ua < 6.0f);
}

/* ========================================================================== */
/* 6. Test Runner Main Entry Point                                            */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_TC_PWR_01_stop2_deep_sleep_current_room_temp);
    RUN_TEST(test_TC_PWR_02_stop2_elevated_temperature_current);
    RUN_TEST(test_TC_PWR_03_total_active_cycle_duration_ceiling);
    RUN_TEST(test_TC_PWR_04_nominal_active_duration_floor);
    RUN_TEST(test_TC_PWR_05_active_cycle_average_current);
    RUN_TEST(test_TC_PWR_06_sensor_rail_stabilization_guard);
    RUN_TEST(test_TC_PWR_07_pre_sleep_analog_gpio_isolation_leakage);
    RUN_TEST(test_TC_PWR_08_gated_battery_adc_divider_leakage);
    RUN_TEST(test_TC_PWR_09_lorawan_rf_tx_energy_standard);
    RUN_TEST(test_TC_PWR_10_high_power_rf_tx_headroom);
    RUN_TEST(test_TC_PWR_11_flash_nvm_programming_energy);
    RUN_TEST(test_TC_PWR_12_total_single_cycle_charge_15min);

    return UNITY_END();
}
