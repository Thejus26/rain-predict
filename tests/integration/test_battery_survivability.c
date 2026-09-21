/**
 * @file    test_battery_survivability.c
 * @brief   Sprint 7 Task S7-T2.3: 14-Day Zero-Sunlight Battery Survivability & Autonomy.
 * @details Validates the LiFePO4 electrochemical battery discharge dynamics,
 *          14-day zero-sunlight state-of-charge retention (>= 98.0%), and multi-year
 *          darkness autonomy horizon (> 800 days) against the 12-point verification
 *          matrix TC-BAT-01 through TC-BAT-12.
 */

#include "unity.h"
#include "status.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ========================================================================== */
/* 1. Electrical & Battery Specification Constants                            */
/* ========================================================================== */

#define BATTERY_NOMINAL_CAPACITY_MAH          (2500.0f) /**< Nominal 26650 LiFePO4 pack */
#define BATTERY_USABLE_DOD_PCT                (80.0f)   /**< 80% Depth-of-Discharge cutoff */
#define BATTERY_USABLE_CAPACITY_MAH           (2000.0f) /**< 2500 mAh * 0.80 */

#define BATTERY_R_INTERNAL_25C_OHMS           (0.030f)  /**< 30 mOhm internal resistance @ 25°C */
#define BATTERY_R_INTERNAL_TEMP_COEFF         (0.025f)  /**< +2.5% per degree below 25°C */

#define LORA_TX_STANDARD_CURRENT_MA           (32.0f)   /**< +14 dBm RF transmission current */
#define LORA_TX_HIGH_POWER_CURRENT_MA         (90.0f)   /**< +22 dBm RF transmission current */

#define DURATION_14_DAYS                      (14.0f)
#define HOURS_IN_14_DAYS                      (336.0f)
#define DAYS_PER_YEAR                         (365.25f)

/* Daily Gross Draws from S7-T2.2 */
#define DAILY_GROSS_NOMINAL_MAH               (1.462f)  /**< 15-min fair weather mission */
#define DAILY_GROSS_MONSOON_MAH               (1.899f)  /**< 2-min continuous downpour */
#define DAILY_GROSS_MIXED_STORM_MAH           (2.331f)  /**< Mixed storm with 2x 10s siren pulses */
#define DAILY_GROSS_CONSERVATION_MAH          (1.405f)  /**< 30-min preservation throttle */
#define DAILY_GROSS_CRITICAL_MAH              (1.368f)  /**< 60-min critical throttle */

/* Acceptance Thresholds */
#define TC_BAT_01_NOMINAL_SOC_MIN_PCT         (98.5f)
#define TC_BAT_02_MONSOON_SOC_MIN_PCT         (98.0f)
#define TC_BAT_03_STORM_SOC_MIN_PCT           (98.0f)
#define TC_BAT_04_AUTONOMY_FLOOR_DAYS         (800.0f)  /**< Hard minimum floor */
#define TC_BAT_05_DROP_14DBM_MAX_MV           (2.5f)    /**< IR drop ceiling @ 0°C */
#define TC_BAT_06_DROP_22DBM_MAX_MV           (5.0f)    /**< IR drop ceiling @ 0°C */
#define TC_BAT_10_40C_SOC_MIN_PCT             (97.5f)
#define TC_BAT_11_MARGIN_MIN_FACTOR           (90.0f)   /**< >= 90x requirement headroom */

/* ========================================================================== */
/* 2. Data Structures                                                         */
/* ========================================================================== */

typedef struct {
    const char *profile_name;
    float       daily_gross_mah;
    float       total_consumed_mah;
    float       initial_soc_pct;
    float       final_soc_pct;
    float       initial_voltage_v;
    float       final_voltage_v;
    float       autonomy_days;
    float       autonomy_years;
    bool        passed_14day_gate;
} sim_profile_result_t;

typedef struct {
    sim_profile_result_t nominal;
    sim_profile_result_t monsoon;
    sim_profile_result_t storm;
    sim_profile_result_t conservation;
    sim_profile_result_t elevated_40c;
    float                r_internal_0c_ohms;
    float                v_drop_14dbm_mv;
    float                v_drop_22dbm_mv;
    float                autonomy_margin_factor;
    bool                 all_gates_passed;
} survivability_summary_t;

/* Cached Simulation Summary */
static survivability_summary_t s_summary;

/* ========================================================================== */
/* 3. Mathematical Electrochemical Model Engine                              */
/* ========================================================================== */

/**
 * @brief Computes temperature-dependent internal cell impedance in Ohms.
 */
static inline float calc_internal_resistance(float temp_c) {
    return BATTERY_R_INTERNAL_25C_OHMS * (1.0f + BATTERY_R_INTERNAL_TEMP_COEFF * (25.0f - temp_c));
}

/**
 * @brief 8-segment piecewise linear Open Circuit Voltage (OCV) curve for LiFePO4.
 */
static float get_lifepo4_ocv(float soc_pct) {
    float soc = soc_pct;
    if (soc < 0.0f) {
        soc = 0.0f;
    }
    if (soc > 100.0f) {
        soc = 100.0f;
    }

    if (soc >= 90.0f) {
        return 3.350f + (soc - 90.0f) * (0.300f / 10.0f);   /* 3.35V to 3.65V */
    } else if (soc >= 70.0f) {
        return 3.280f + (soc - 70.0f) * (0.070f / 20.0f);   /* 3.28V to 3.35V */
    } else if (soc >= 30.0f) {
        return 3.200f + (soc - 30.0f) * (0.080f / 40.0f);   /* 3.20V to 3.28V (Core Plateau) */
    } else if (soc >= 15.0f) {
        return 3.100f + (soc - 15.0f) * (0.100f / 15.0f);   /* 3.10V to 3.20V */
    } else if (soc >= 5.0f) {
        return 3.000f + (soc - 5.0f) * (0.100f / 10.0f);    /* 3.00V to 3.10V (Conservation) */
    } else {
        return 2.500f + soc * (0.500f / 5.0f);              /* 2.50V to 3.00V (Critical) */
    }
}

/**
 * @brief Simulates 14 days of constant solar darkness (0.0 W) for a given mission draw.
 */
static void run_14day_discharge(sim_profile_result_t *p_res, const char *name, float daily_draw_mah) {
    p_res->profile_name = name;
    p_res->daily_gross_mah = daily_draw_mah;
    p_res->initial_soc_pct = 100.0f;
    p_res->initial_voltage_v = get_lifepo4_ocv(100.0f);

    float total_consumed = 0.0f;
    float current_soc = 100.0f;

    for (uint32_t day = 0; day < 14U; ++day) {
        float delta_soc = (daily_draw_mah / BATTERY_NOMINAL_CAPACITY_MAH) * 100.0f;
        current_soc = (current_soc > delta_soc) ? (current_soc - delta_soc) : 0.0f;
        total_consumed += daily_draw_mah;
    }

    p_res->total_consumed_mah = total_consumed;
    p_res->final_soc_pct = current_soc;
    p_res->final_voltage_v = get_lifepo4_ocv(current_soc);

    /* Usable capacity to 80% DoD floor (2000 mAh) */
    p_res->autonomy_days = (daily_draw_mah > 0.0f) ? (BATTERY_USABLE_CAPACITY_MAH / daily_draw_mah) : 9999.0f;
    p_res->autonomy_years = p_res->autonomy_days / DAYS_PER_YEAR;
    p_res->passed_14day_gate = (p_res->final_soc_pct >= 98.0f) && (p_res->autonomy_days >= DURATION_14_DAYS);
}

static void evaluate_all_survivability_scenarios(survivability_summary_t *p_summary) {
    memset(p_summary, 0, sizeof(survivability_summary_t));

    /* 1. Standard Multi-Regime Runs */
    run_14day_discharge(&p_summary->nominal, "Nominal Fair Weather (15-min)", DAILY_GROSS_NOMINAL_MAH);
    run_14day_discharge(&p_summary->monsoon, "Continuous Monsoon Downpour (2-min)", DAILY_GROSS_MONSOON_MAH);
    run_14day_discharge(&p_summary->storm, "Mixed Severe Storm + 28 Sirens", DAILY_GROSS_MIXED_STORM_MAH);
    run_14day_discharge(&p_summary->conservation, "Battery Conservation Throttle (30-min)", DAILY_GROSS_CONSERVATION_MAH);

    /* 2. Elevated Temperature (40°C): Self-discharge doubles (+1.250 mAh/day) */
    run_14day_discharge(&p_summary->elevated_40c, "Elevated Temp (40°C Storm)", DAILY_GROSS_MIXED_STORM_MAH + 1.250f);

    /* 3. Voltage Drop Calculations @ 0°C */
    p_summary->r_internal_0c_ohms = calc_internal_resistance(0.0f);
    p_summary->v_drop_14dbm_mv = (LORA_TX_STANDARD_CURRENT_MA / 1000.0f) * p_summary->r_internal_0c_ohms * 1000.0f;
    p_summary->v_drop_22dbm_mv = (LORA_TX_HIGH_POWER_CURRENT_MA / 1000.0f) * p_summary->r_internal_0c_ohms * 1000.0f;

    /* 4. Autonomy Margin vs 14 Days */
    p_summary->autonomy_margin_factor = p_summary->nominal.autonomy_days / DURATION_14_DAYS;

    /* 5. Overall Status */
    p_summary->all_gates_passed = p_summary->nominal.passed_14day_gate &&
                                  p_summary->monsoon.passed_14day_gate &&
                                  p_summary->storm.passed_14day_gate &&
                                  (p_summary->storm.autonomy_days >= TC_BAT_04_AUTONOMY_FLOOR_DAYS) &&
                                  (p_summary->v_drop_14dbm_mv <= TC_BAT_05_DROP_14DBM_MAX_MV) &&
                                  (p_summary->v_drop_22dbm_mv <= TC_BAT_06_DROP_22DBM_MAX_MV);
}

/* ========================================================================== */
/* 4. Unity Setup & Teardown                                                  */
/* ========================================================================== */

void setUp(void) {
    evaluate_all_survivability_scenarios(&s_summary);
}

void tearDown(void) {
}

/* ========================================================================== */
/* 5. 12-Point Battery Autonomy Verification Tests (TC-BAT-01 .. TC-BAT-12)    */
/* ========================================================================== */

/**
 * @brief TC-BAT-01: 14-Day Nominal Darkness Retention (SoC >= 98.5%).
 *        Nominal 15-min fair weather mission consumes ~20.47 mAh over 14 days.
 */
static void test_TC_BAT_01_nominal_14day_darkness_retention(void) {
    float final_soc = s_summary.nominal.final_soc_pct;
    TEST_ASSERT_TRUE(final_soc >= TC_BAT_01_NOMINAL_SOC_MIN_PCT);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 99.18f, final_soc);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 20.47f, s_summary.nominal.total_consumed_mah);
}

/**
 * @brief TC-BAT-02: 14-Day Monsoon Darkness Retention (SoC >= 98.0%).
 *        Continuous 2-min downpour sampling consumes ~26.59 mAh over 14 days.
 */
static void test_TC_BAT_02_monsoon_14day_darkness_retention(void) {
    float final_soc = s_summary.monsoon.final_soc_pct;
    TEST_ASSERT_TRUE(final_soc >= TC_BAT_02_MONSOON_SOC_MIN_PCT);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 98.94f, final_soc);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 26.59f, s_summary.monsoon.total_consumed_mah);
}

/**
 * @brief TC-BAT-03: 14-Day Worst-Case Storm + Sirens (SoC >= 98.0%).
 *        Mixed severe storm with 28 siren pulses consumes ~32.63 mAh over 14 days.
 */
static void test_TC_BAT_03_worst_case_storm_14day_darkness_retention(void) {
    float final_soc = s_summary.storm.final_soc_pct;
    TEST_ASSERT_TRUE(final_soc >= TC_BAT_03_STORM_SOC_MIN_PCT);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 98.69f, final_soc);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 32.63f, s_summary.storm.total_consumed_mah);
}

/**
 * @brief TC-BAT-04: Minimum Darkness Autonomy Floor (Autonomy >= 800 Days / > 2.19 Yrs).
 *        Depletion to 3.00V (80% DoD, 2000 mAh usable) under continuous worst-case storm.
 */
static void test_TC_BAT_04_minimum_darkness_autonomy_floor(void) {
    float autonomy_days = s_summary.storm.autonomy_days;
    TEST_ASSERT_TRUE(autonomy_days >= TC_BAT_04_AUTONOMY_FLOOR_DAYS);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 858.0f, autonomy_days);
    TEST_ASSERT_TRUE(s_summary.storm.autonomy_years >= 2.19f);
}

/**
 * @brief TC-BAT-05: Transmit Burst Voltage Drop @ +14 dBm (<= 2.5 mV @ 0°C).
 *        32 mA TX burst across 48.75 mOhm impedance drops ~1.56 mV.
 */
static void test_TC_BAT_05_lora_tx_burst_voltage_drop_standard(void) {
    TEST_ASSERT_TRUE(s_summary.v_drop_14dbm_mv <= TC_BAT_05_DROP_14DBM_MAX_MV);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.56f, s_summary.v_drop_14dbm_mv);
}

/**
 * @brief TC-BAT-06: High-Power +22 dBm Burst Drop (<= 5.0 mV @ 0°C).
 *        90 mA TX burst across 48.75 mOhm impedance drops ~4.39 mV.
 */
static void test_TC_BAT_06_lora_tx_burst_voltage_drop_high_power(void) {
    TEST_ASSERT_TRUE(s_summary.v_drop_22dbm_mv <= TC_BAT_06_DROP_22DBM_MAX_MV);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 4.39f, s_summary.v_drop_22dbm_mv);
}

/**
 * @brief TC-BAT-07: OCV-to-SoC Monotonicity.
 *        Evaluates piecewise OCV function across 0% to 100% SoC and verifies strict monotonicity.
 */
static void test_TC_BAT_07_ocv_piecewise_curve_monotonicity(void) {
    float prev_v = get_lifepo4_ocv(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.500f, prev_v);

    for (uint32_t i = 1; i <= 200; ++i) {
        float soc = (float)i * 0.5f;
        float curr_v = get_lifepo4_ocv(soc);
        TEST_ASSERT_TRUE(curr_v > prev_v);
        prev_v = curr_v;
    }

    /* Verify boundary points */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.000f, get_lifepo4_ocv(5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.100f, get_lifepo4_ocv(15.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.200f, get_lifepo4_ocv(30.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.280f, get_lifepo4_ocv(70.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.350f, get_lifepo4_ocv(90.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.650f, get_lifepo4_ocv(100.0f));
}

/**
 * @brief TC-BAT-08: Preservation Tier Entry Timing (Vbat = 3.10V / 15.0% SoC).
 *        Verifies that 3.10V marks the promotion into 30-min Conservation mode.
 */
static void test_TC_BAT_08_preservation_tier_entry_voltage(void) {
    float ocv_conservation = get_lifepo4_ocv(15.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.100f, ocv_conservation);

    /* Confirm nominal autonomy in conservation mode */
    TEST_ASSERT_TRUE(s_summary.conservation.autonomy_days >= 1400.0f);
}

/**
 * @brief TC-BAT-09: Critical Tier Entry Timing (Vbat = 3.00V / 5.0% SoC).
 *        Verifies that 3.00V marks the promotion into 60-min Critical mode.
 */
static void test_TC_BAT_09_critical_tier_entry_voltage(void) {
    float ocv_critical = get_lifepo4_ocv(5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.000f, ocv_critical);
}

/**
 * @brief TC-BAT-10: High-Temperature Self-Discharge Margin (SoC >= 97.5% @ 40°C).
 *        Under elevated 40°C temperature with doubled self-discharge rate (+1.25 mAh/d).
 */
static void test_TC_BAT_10_high_temperature_self_discharge_margin(void) {
    float final_soc_40c = s_summary.elevated_40c.final_soc_pct;
    TEST_ASSERT_TRUE(final_soc_40c >= TC_BAT_10_40C_SOC_MIN_PCT);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 97.99f, final_soc_40c);
}

/**
 * @brief TC-BAT-11: Zero-Sunlight Autonomy Margin (>= 90.0x Requirement Headroom).
 *        Ratio of 1,368.0 nominal autonomy days to the 14-day zero-sunlight mandate.
 */
static void test_TC_BAT_11_zero_sunlight_autonomy_margin(void) {
    TEST_ASSERT_TRUE(s_summary.autonomy_margin_factor >= TC_BAT_11_MARGIN_MIN_FACTOR);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 97.7f, s_summary.autonomy_margin_factor);
}

/**
 * @brief TC-BAT-12: Battery Simulation Invariants and Gate Completeness.
 *        Verifies overall consistency, non-negative capacities, and status gates.
 */
static void test_TC_BAT_12_battery_simulation_invariants_and_gates(void) {
    TEST_ASSERT_TRUE(s_summary.all_gates_passed);
    TEST_ASSERT_TRUE(BATTERY_USABLE_CAPACITY_MAH == 2000.0f);

    /* Verify that 14 days in darkness drains < 1.5% of total battery capacity */
    float max_drain_pct = (s_summary.storm.total_consumed_mah / BATTERY_NOMINAL_CAPACITY_MAH) * 100.0f;
    TEST_ASSERT_TRUE(max_drain_pct < 1.50f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.31f, max_drain_pct);

    /* Verify progression: nominal consumes less than monsoon, which consumes less than storm */
    TEST_ASSERT_TRUE(s_summary.nominal.total_consumed_mah < s_summary.monsoon.total_consumed_mah);
    TEST_ASSERT_TRUE(s_summary.monsoon.total_consumed_mah < s_summary.storm.total_consumed_mah);

    /* Verify autonomy progression: storm < monsoon < nominal < conservation */
    TEST_ASSERT_TRUE(s_summary.storm.autonomy_days < s_summary.monsoon.autonomy_days);
    TEST_ASSERT_TRUE(s_summary.monsoon.autonomy_days < s_summary.nominal.autonomy_days);
    TEST_ASSERT_TRUE(s_summary.nominal.autonomy_days < s_summary.conservation.autonomy_days);
}

/* ========================================================================== */
/* 6. Test Runner Main Entry Point                                            */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_TC_BAT_01_nominal_14day_darkness_retention);
    RUN_TEST(test_TC_BAT_02_monsoon_14day_darkness_retention);
    RUN_TEST(test_TC_BAT_03_worst_case_storm_14day_darkness_retention);
    RUN_TEST(test_TC_BAT_04_minimum_darkness_autonomy_floor);
    RUN_TEST(test_TC_BAT_05_lora_tx_burst_voltage_drop_standard);
    RUN_TEST(test_TC_BAT_06_lora_tx_burst_voltage_drop_high_power);
    RUN_TEST(test_TC_BAT_07_ocv_piecewise_curve_monotonicity);
    RUN_TEST(test_TC_BAT_08_preservation_tier_entry_voltage);
    RUN_TEST(test_TC_BAT_09_critical_tier_entry_voltage);
    RUN_TEST(test_TC_BAT_10_high_temperature_self_discharge_margin);
    RUN_TEST(test_TC_BAT_11_zero_sunlight_autonomy_margin);
    RUN_TEST(test_TC_BAT_12_battery_simulation_invariants_and_gates);

    return UNITY_END();
}
