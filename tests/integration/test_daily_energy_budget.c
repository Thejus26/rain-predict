/**
 * @file    test_daily_energy_budget.c
 * @brief   Sprint 7 Task S7-T2.2: 24-Hour Total Daily Energy Budget Verification.
 * @details Validates the 24-hour gross energy consumption (< 25.0 mAh/day) across
 *          Nominal, Storm Watch, Active Downpour, Mixed Severe Storm, and Battery
 *          Preservation Regimes against the 12-point verification matrix
 *          TC-ENG-01 through TC-ENG-12.
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

#define NOMINAL_SYSTEM_VOLTAGE_V              (3.3f)
#define DAILY_DESIGN_CEILING_MAH_DAY          (25.0f)   /**< Hard system constraint */
#define MINIMUM_SAFETY_MARGIN_FACTOR          (10.0f)   /**< Target >= 10x safety headroom */

/* Non-Firmware Parasitic Overheads */
#define LDO_QUIESCENT_CURRENT_UA              (0.025f)  /**< TPS7A02 ground current: 25 nA */
#define PROTECTION_IC_CURRENT_UA              (3.0f)    /**< DW01A operating current: 3.0 uA */
#define BATTERY_CAPACITY_MAH                  (2500.0f) /**< LiFePO4 nominal cell capacity */
#define BATTERY_SELF_DISCHARGE_PCT_MONTH      (1.5f)    /**< 1.5% per month @ 25°C */
#define DAYS_PER_MONTH                        (30.0f)
#define HOURS_PER_DAY                         (24.0f)

/* Active Cycle & Actuation Parameters */
#define ACTIVE_CYCLE_TIME_SEC                 (0.1665f) /**< 166.5 ms active window */
#define ACTIVE_CYCLE_CHARGE_NOMINAL_UAH       (0.7001f) /**< Nominal active charge: 0.7001 uAh */
#define ACTIVE_CYCLE_CHARGE_CONSERVE_UAH      (0.4583f) /**< Conservation active charge */
#define ACTIVE_CYCLE_CHARGE_CRITICAL_UAH      (0.3750f) /**< Critical preservation active charge */

#define STOP2_SLEEP_CURRENT_NOMINAL_UA        (3.0f)    /**< Nominal Stop 2 sleep current */
#define STOP2_SLEEP_CURRENT_CONSERVE_UA       (2.5f)    /**< Isolated bus sleep current */
#define STOP2_SLEEP_CURRENT_CRITICAL_UA       (1.5f)    /**< Minimum hardware sleep current */

#define SIREN_RELAY_CURRENT_MA                (150.0f)  /**< Siren coil actuation current */
#define SIREN_RELAY_PULSE_DURATION_SEC        (10.0f)   /**< Siren pulse duration */

#define SECONDS_PER_HOUR                      (3600.0f)

/* Acceptance Thresholds */
#define TC_ENG_01_NOMINAL_FW_MAX_MAH          (0.200f)
#define TC_ENG_02_NOMINAL_GROSS_MAX_MAH       (2.000f)
#define TC_ENG_03_STORM_2MIN_FW_MAX_MAH       (0.800f)
#define TC_ENG_04_MIXED_GROSS_MAX_MAH         (3.500f)
#define TC_ENG_05_CEILING_MAX_MAH             (25.000f)
#define TC_ENG_06_CONSERVATION_FW_MAX_MAH     (0.100f)
#define TC_ENG_07_CRITICAL_FW_MAX_MAH         (0.050f)
#define TC_ENG_08_SIREN_PULSE_NOMINAL_MAH     (0.4167f)
#define TC_ENG_08_SIREN_TOLERANCE_MAH         (0.0210f) /**< +/- 5% */
#define TC_ENG_09_SLEEP_DUTY_MIN_PCT          (99.80f)
#define TC_ENG_10_30DAY_GROSS_MAX_MAH         (60.0f)
#define TC_ENG_11_40C_GROSS_MAX_MAH           (4.000f)

/* ========================================================================== */
/* 2. Data Structures                                                         */
/* ========================================================================== */

typedef struct {
    const char *regime_name;
    float       interval_sec;
    uint32_t    cycles_per_day;
    float       active_charge_uah;
    float       sleep_current_ua;
    uint32_t    siren_pulses_per_day;
    float       firmware_mah_day;
    float       gross_mah_day;
    float       gross_mwh_day;
} regime_budget_t;

typedef struct {
    regime_budget_t nominal;
    regime_budget_t storm_watch;
    regime_budget_t downpour;
    regime_budget_t mixed_storm;
    regime_budget_t conservation;
    regime_budget_t critical;
    float           ldo_mah_day;
    float           prot_ic_mah_day;
    float           self_discharge_mah_day;
    float           total_overhead_mah_day;
    float           worst_case_gross_mah_day;
    float           worst_case_gross_mwh_day;
    float           safety_margin_factor;
    bool            all_gates_passed;
} daily_energy_summary_t;

/* Cached Daily Budget Evaluation Result */
static daily_energy_summary_t s_budget_summary;

/* ========================================================================== */
/* 3. Mathematical Integration Engine                                         */
/* ========================================================================== */

static inline float calc_cycle_sleep_charge_uah(float sleep_current_ua, float interval_sec) {
    float sleep_time_sec = (interval_sec > ACTIVE_CYCLE_TIME_SEC) ? (interval_sec - ACTIVE_CYCLE_TIME_SEC) : 0.0f;
    return sleep_current_ua * (sleep_time_sec / SECONDS_PER_HOUR);
}

static inline float calc_siren_charge_mah(float siren_current_ma, float siren_duration_sec, uint32_t pulses) {
    return (float)pulses * (siren_current_ma * (siren_duration_sec / SECONDS_PER_HOUR));
}

static void evaluate_regime(regime_budget_t *p_regime, float overhead_mah_day) {
    float sleep_charge_uah = calc_cycle_sleep_charge_uah(p_regime->sleep_current_ua, p_regime->interval_sec);
    float single_cycle_uah = p_regime->active_charge_uah + sleep_charge_uah;
    float cycle_firmware_mah = ((float)p_regime->cycles_per_day * single_cycle_uah) / 1000.0f;
    float siren_mah = calc_siren_charge_mah(SIREN_RELAY_CURRENT_MA, SIREN_RELAY_PULSE_DURATION_SEC, p_regime->siren_pulses_per_day);

    p_regime->firmware_mah_day = cycle_firmware_mah + siren_mah;
    p_regime->gross_mah_day = p_regime->firmware_mah_day + overhead_mah_day;
    p_regime->gross_mwh_day = p_regime->gross_mah_day * NOMINAL_SYSTEM_VOLTAGE_V;
}

static void integrate_daily_energy_budget(daily_energy_summary_t *p_summary) {
    memset(p_summary, 0, sizeof(daily_energy_summary_t));

    /* 1. Standby Parasitic Overheads */
    p_summary->ldo_mah_day = (LDO_QUIESCENT_CURRENT_UA * HOURS_PER_DAY) / 1000.0f;
    p_summary->prot_ic_mah_day = (PROTECTION_IC_CURRENT_UA * HOURS_PER_DAY) / 1000.0f;
    p_summary->self_discharge_mah_day = (BATTERY_CAPACITY_MAH * (BATTERY_SELF_DISCHARGE_PCT_MONTH / 100.0f)) / DAYS_PER_MONTH;
    p_summary->total_overhead_mah_day = p_summary->ldo_mah_day + p_summary->prot_ic_mah_day + p_summary->self_discharge_mah_day;

    float overhead = p_summary->total_overhead_mah_day;

    /* 2. Nominal Fair Weather (15-min / 96 cycles) */
    p_summary->nominal.regime_name = "Nominal Fair Weather (15-min)";
    p_summary->nominal.interval_sec = 900.0f;
    p_summary->nominal.cycles_per_day = 96U;
    p_summary->nominal.active_charge_uah = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH;
    p_summary->nominal.sleep_current_ua = STOP2_SLEEP_CURRENT_NOMINAL_UA;
    p_summary->nominal.siren_pulses_per_day = 0U;
    evaluate_regime(&p_summary->nominal, overhead);

    /* 3. Storm Watch Mode (5-min / 288 cycles) */
    p_summary->storm_watch.regime_name = "Storm Watch Mode (5-min)";
    p_summary->storm_watch.interval_sec = 300.0f;
    p_summary->storm_watch.cycles_per_day = 288U;
    p_summary->storm_watch.active_charge_uah = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH;
    p_summary->storm_watch.sleep_current_ua = STOP2_SLEEP_CURRENT_NOMINAL_UA;
    p_summary->storm_watch.siren_pulses_per_day = 0U;
    evaluate_regime(&p_summary->storm_watch, overhead);

    /* 4. Active Monsoon Downpour (2-min / 720 cycles) */
    p_summary->downpour.regime_name = "Active Monsoon Downpour (2-min)";
    p_summary->downpour.interval_sec = 120.0f;
    p_summary->downpour.cycles_per_day = 720U;
    p_summary->downpour.active_charge_uah = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH;
    p_summary->downpour.sleep_current_ua = STOP2_SLEEP_CURRENT_NOMINAL_UA;
    p_summary->downpour.siren_pulses_per_day = 0U;
    evaluate_regime(&p_summary->downpour, overhead);

    /* 5. Mixed Severe Storm Day (80x 15m + 36x 5m + 30x 2m + 2 siren pulses) */
    p_summary->mixed_storm.regime_name = "Mixed Severe Storm Day (Worst-Case)";
    p_summary->mixed_storm.interval_sec = 900.0f;
    p_summary->mixed_storm.cycles_per_day = 146U;
    p_summary->mixed_storm.active_charge_uah = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH;
    p_summary->mixed_storm.sleep_current_ua = STOP2_SLEEP_CURRENT_NOMINAL_UA;
    p_summary->mixed_storm.siren_pulses_per_day = 2U;

    float q_15m_uah = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH + calc_cycle_sleep_charge_uah(STOP2_SLEEP_CURRENT_NOMINAL_UA, 900.0f);
    float q_5m_uah  = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH + calc_cycle_sleep_charge_uah(STOP2_SLEEP_CURRENT_NOMINAL_UA, 300.0f);
    float q_2m_uah  = ACTIVE_CYCLE_CHARGE_NOMINAL_UAH + calc_cycle_sleep_charge_uah(STOP2_SLEEP_CURRENT_NOMINAL_UA, 120.0f);
    float q_siren_mah = calc_siren_charge_mah(SIREN_RELAY_CURRENT_MA, SIREN_RELAY_PULSE_DURATION_SEC, 2U);

    p_summary->mixed_storm.firmware_mah_day = ((80.0f * q_15m_uah) + (36.0f * q_5m_uah) + (30.0f * q_2m_uah)) / 1000.0f + q_siren_mah;
    p_summary->mixed_storm.gross_mah_day = p_summary->mixed_storm.firmware_mah_day + overhead;
    p_summary->mixed_storm.gross_mwh_day = p_summary->mixed_storm.gross_mah_day * NOMINAL_SYSTEM_VOLTAGE_V;

    /* 6. Battery Conservation Throttle (30-min / 48 cycles) */
    p_summary->conservation.regime_name = "Battery Conservation Throttle (30-min)";
    p_summary->conservation.interval_sec = 1800.0f;
    p_summary->conservation.cycles_per_day = 48U;
    p_summary->conservation.active_charge_uah = ACTIVE_CYCLE_CHARGE_CONSERVE_UAH;
    p_summary->conservation.sleep_current_ua = STOP2_SLEEP_CURRENT_CONSERVE_UA;
    p_summary->conservation.siren_pulses_per_day = 0U;
    evaluate_regime(&p_summary->conservation, overhead);

    /* 7. Critical Preservation Throttle (60-min / 24 cycles) */
    p_summary->critical.regime_name = "Critical Preservation Throttle (60-min)";
    p_summary->critical.interval_sec = 3600.0f;
    p_summary->critical.cycles_per_day = 24U;
    p_summary->critical.active_charge_uah = ACTIVE_CYCLE_CHARGE_CRITICAL_UAH;
    p_summary->critical.sleep_current_ua = STOP2_SLEEP_CURRENT_CRITICAL_UA;
    p_summary->critical.siren_pulses_per_day = 0U;
    evaluate_regime(&p_summary->critical, overhead);

    /* 8. Worst-Case Identification */
    float worst = p_summary->nominal.gross_mah_day;
    if (p_summary->storm_watch.gross_mah_day > worst)  { worst = p_summary->storm_watch.gross_mah_day; }
    if (p_summary->downpour.gross_mah_day > worst)     { worst = p_summary->downpour.gross_mah_day; }
    if (p_summary->mixed_storm.gross_mah_day > worst)  { worst = p_summary->mixed_storm.gross_mah_day; }
    if (p_summary->conservation.gross_mah_day > worst) { worst = p_summary->conservation.gross_mah_day; }
    if (p_summary->critical.gross_mah_day > worst)     { worst = p_summary->critical.gross_mah_day; }

    p_summary->worst_case_gross_mah_day = worst;
    p_summary->worst_case_gross_mwh_day = worst * NOMINAL_SYSTEM_VOLTAGE_V;
    p_summary->safety_margin_factor = DAILY_DESIGN_CEILING_MAH_DAY / worst;
    p_summary->all_gates_passed = (worst < DAILY_DESIGN_CEILING_MAH_DAY);
}

/* ========================================================================== */
/* 4. Unity Setup & Teardown                                                  */
/* ========================================================================== */

void setUp(void) {
    integrate_daily_energy_budget(&s_budget_summary);
}

void tearDown(void) {
}

/* ========================================================================== */
/* 5. 12-Point 24-Hour Energy Verification Tests (TC-ENG-01 .. TC-ENG-12)      */
/* ========================================================================== */

/**
 * @brief TC-ENG-01: Nominal 24-Hour Firmware Charge (<= 0.200 mAh/day).
 *        Standard 15-minute sampling (96 cycles/day) consumes ~0.139 mAh/day.
 */
static void test_TC_ENG_01_nominal_24h_firmware_charge(void) {
    float fw_charge = s_budget_summary.nominal.firmware_mah_day;
    TEST_ASSERT_TRUE(fw_charge <= TC_ENG_01_NOMINAL_FW_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.010f, 0.139f, fw_charge);
}

/**
 * @brief TC-ENG-02: Nominal 24-Hour Gross Energy (<= 2.000 mAh/day).
 *        Nominal fair-weather mission including all non-firmware hardware overheads.
 */
static void test_TC_ENG_02_nominal_24h_gross_energy(void) {
    float gross_charge = s_budget_summary.nominal.gross_mah_day;
    TEST_ASSERT_TRUE(gross_charge <= TC_ENG_02_NOMINAL_GROSS_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.020f, 1.462f, gross_charge);
    TEST_ASSERT_FLOAT_WITHIN(0.050f, 4.824f, s_budget_summary.nominal.gross_mwh_day);
}

/**
 * @brief TC-ENG-03: Continuous 2-min Storm Day (<= 0.800 mAh/day).
 *        720 consecutive 2-minute active downpour cycles consumes ~0.576 mAh/day.
 */
static void test_TC_ENG_03_continuous_2min_storm_day(void) {
    float fw_charge = s_budget_summary.downpour.firmware_mah_day;
    TEST_ASSERT_TRUE(fw_charge <= TC_ENG_03_STORM_2MIN_FW_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.020f, 0.576f, fw_charge);
    TEST_ASSERT_TRUE(s_budget_summary.downpour.gross_mah_day < 2.000f);
}

/**
 * @brief TC-ENG-04: Mixed Severe Storm + Siren Day (<= 3.500 mAh/day).
 *        Realistic worst-case: 80x 15m + 36x 5m + 30x 2m + 2x 10s siren pulses.
 */
static void test_TC_ENG_04_mixed_severe_storm_siren_day(void) {
    float gross_charge = s_budget_summary.mixed_storm.gross_mah_day;
    TEST_ASSERT_TRUE(gross_charge <= TC_ENG_04_MIXED_GROSS_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.050f, 2.330f, gross_charge);
    TEST_ASSERT_FLOAT_WITHIN(0.100f, 7.689f, s_budget_summary.mixed_storm.gross_mwh_day);
}

/**
 * @brief TC-ENG-05: Absolute Daily Design Ceiling (< 25.000 mAh/day, > 10x headroom).
 *        Worst-case gross consumption across all regimes must maintain > 10x safety margin.
 */
static void test_TC_ENG_05_absolute_daily_design_ceiling(void) {
    float worst_gross = s_budget_summary.worst_case_gross_mah_day;
    TEST_ASSERT_TRUE(worst_gross < TC_ENG_05_CEILING_MAX_MAH);
    TEST_ASSERT_TRUE(s_budget_summary.safety_margin_factor >= MINIMUM_SAFETY_MARGIN_FACTOR);
    TEST_ASSERT_TRUE(s_budget_summary.all_gates_passed);
}

/**
 * @brief TC-ENG-06: Battery Conservation Throttling (<= 0.100 mAh/day).
 *        48 cycles @ 30 min with muted acoustics and throttled LED.
 */
static void test_TC_ENG_06_battery_conservation_throttling(void) {
    float fw_charge = s_budget_summary.conservation.firmware_mah_day;
    TEST_ASSERT_TRUE(fw_charge <= TC_ENG_06_CONSERVATION_FW_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.010f, 0.082f, fw_charge);
    TEST_ASSERT_TRUE(s_budget_summary.conservation.gross_mah_day < s_budget_summary.nominal.gross_mah_day);
}

/**
 * @brief TC-ENG-07: Critical Preservation Throttling (<= 0.050 mAh/day).
 *        24 cycles @ 60 min with LED disabled, LoRa limited to +14dBm LP PA.
 */
static void test_TC_ENG_07_critical_preservation_throttling(void) {
    float fw_charge = s_budget_summary.critical.firmware_mah_day;
    TEST_ASSERT_TRUE(fw_charge <= TC_ENG_07_CRITICAL_FW_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.010f, 0.045f, fw_charge);
    TEST_ASSERT_TRUE(s_budget_summary.critical.firmware_mah_day < s_budget_summary.conservation.firmware_mah_day);
}

/**
 * @brief TC-ENG-08: Siren Relay Pulse Energy (0.417 mAh +/- 5%).
 *        Single 10-second 150mA relay coil pulse actuation.
 */
static void test_TC_ENG_08_siren_relay_pulse_energy(void) {
    float single_siren_mah = calc_siren_charge_mah(SIREN_RELAY_CURRENT_MA, SIREN_RELAY_PULSE_DURATION_SEC, 1U);
    TEST_ASSERT_FLOAT_WITHIN(TC_ENG_08_SIREN_TOLERANCE_MAH, TC_ENG_08_SIREN_PULSE_NOMINAL_MAH, single_siren_mah);
}

/**
 * @brief TC-ENG-09: Standby Duty-Cycle Percentage (>= 99.80%).
 *        Calculates ratio of Stop 2 deep sleep time in nominal 15-minute window.
 */
static void test_TC_ENG_09_standby_duty_cycle_percentage(void) {
    float total_window_sec = 900.0f;
    float sleep_time_sec = total_window_sec - ACTIVE_CYCLE_TIME_SEC;
    float sleep_duty_pct = (sleep_time_sec / total_window_sec) * 100.0f;

    TEST_ASSERT_TRUE(sleep_duty_pct >= TC_ENG_09_SLEEP_DUTY_MIN_PCT);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 99.981f, sleep_duty_pct);
}

/**
 * @brief TC-ENG-10: Continuous 30-Day Total Consumption (<= 60.0 mAh).
 *        Cumulative 30-day realistic multi-regime simulation (25 fair + 4 storm + 1 downpour).
 */
static void test_TC_ENG_10_continuous_30day_total_consumption(void) {
    float month_gross_mah = (25.0f * s_budget_summary.nominal.gross_mah_day) +
                            (4.0f * s_budget_summary.mixed_storm.gross_mah_day) +
                            (1.0f * s_budget_summary.downpour.gross_mah_day);

    TEST_ASSERT_TRUE(month_gross_mah <= TC_ENG_10_30DAY_GROSS_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 48.0f, month_gross_mah);
}

/**
 * @brief TC-ENG-11: Temperature Self-Discharge Margin (<= 4.000 mAh/day @ 40°C).
 *        Verifies that even when self-discharge doubles under tropical heat (2.50 mAh/day),
 *        the daily gross consumption remains well within safety limits.
 */
static void test_TC_ENG_11_temperature_self_discharge_margin(void) {
    float self_discharge_40c_mah = s_budget_summary.self_discharge_mah_day * 2.0f;
    float overhead_40c_mah = s_budget_summary.ldo_mah_day + s_budget_summary.prot_ic_mah_day + self_discharge_40c_mah;
    float worst_gross_40c_mah = s_budget_summary.mixed_storm.firmware_mah_day + overhead_40c_mah;

    TEST_ASSERT_TRUE(worst_gross_40c_mah <= TC_ENG_11_40C_GROSS_MAX_MAH);
    TEST_ASSERT_FLOAT_WITHIN(0.050f, 3.580f, worst_gross_40c_mah);
}

/**
 * @brief TC-ENG-12: Energy Report Invariants and Gate Completeness.
 *        Confirms sanity of parasitic hardware overheads and regime progression.
 */
static void test_TC_ENG_12_energy_report_invariants_and_gate_completeness(void) {
    /* LDO: 25 nA * 24h = 0.0006 mAh */
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0006f, s_budget_summary.ldo_mah_day);

    /* DW01A: 3 uA * 24h = 0.0720 mAh */
    TEST_ASSERT_FLOAT_WITHIN(0.0010f, 0.0720f, s_budget_summary.prot_ic_mah_day);

    /* Self-discharge: 2500 mAh * 1.5% / 30d = 1.250 mAh */
    TEST_ASSERT_FLOAT_WITHIN(0.0100f, 1.2500f, s_budget_summary.self_discharge_mah_day);

    /* Total non-firmware overhead = ~1.3226 mAh */
    TEST_ASSERT_FLOAT_WITHIN(0.0100f, 1.3226f, s_budget_summary.total_overhead_mah_day);

    /* Check regime firmware progression: critical < conservation < nominal < storm_watch < downpour < mixed */
    TEST_ASSERT_TRUE(s_budget_summary.critical.firmware_mah_day < s_budget_summary.conservation.firmware_mah_day);
    TEST_ASSERT_TRUE(s_budget_summary.conservation.firmware_mah_day < s_budget_summary.nominal.firmware_mah_day);
    TEST_ASSERT_TRUE(s_budget_summary.nominal.firmware_mah_day < s_budget_summary.storm_watch.firmware_mah_day);
    TEST_ASSERT_TRUE(s_budget_summary.storm_watch.firmware_mah_day < s_budget_summary.downpour.firmware_mah_day);
    TEST_ASSERT_TRUE(s_budget_summary.downpour.firmware_mah_day < s_budget_summary.mixed_storm.firmware_mah_day);
}

/* ========================================================================== */
/* 6. Test Runner Main Entry Point                                            */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_TC_ENG_01_nominal_24h_firmware_charge);
    RUN_TEST(test_TC_ENG_02_nominal_24h_gross_energy);
    RUN_TEST(test_TC_ENG_03_continuous_2min_storm_day);
    RUN_TEST(test_TC_ENG_04_mixed_severe_storm_siren_day);
    RUN_TEST(test_TC_ENG_05_absolute_daily_design_ceiling);
    RUN_TEST(test_TC_ENG_06_battery_conservation_throttling);
    RUN_TEST(test_TC_ENG_07_critical_preservation_throttling);
    RUN_TEST(test_TC_ENG_08_siren_relay_pulse_energy);
    RUN_TEST(test_TC_ENG_09_standby_duty_cycle_percentage);
    RUN_TEST(test_TC_ENG_10_continuous_30day_total_consumption);
    RUN_TEST(test_TC_ENG_11_temperature_self_discharge_margin);
    RUN_TEST(test_TC_ENG_12_energy_report_invariants_and_gate_completeness);

    return UNITY_END();
}
