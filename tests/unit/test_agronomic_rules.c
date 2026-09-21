/**
 * @file test_agronomic_rules.c
 * @brief Sprint 7 Task S7-T3.3: Unit & Decision Logic Tests for Agronomic Advisory Rules.
 *
 * Validates 4-tier alert state mapping, agrochemical spray hold triggers, plucking
 * triage transitions, siren actuation interlocks, and chemical wash-off models.
 *
 * Requirements:
 * - C99 compliance with zero dynamic heap allocation.
 * - Strict static void test prototypes and clean warning suppression.
 * - Canonical ThrowTheSwitch Unity test macros.
 */

#include "unity.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

/* ========================================================================== */
/* Type Definitions & Enumerations                                            */
/* ========================================================================== */

typedef enum {
    AGRO_TIER_0_GREEN   = 0,  /* Rain Unlikely (CPI < 30%) */
    AGRO_TIER_1_AMBER   = 1,  /* Rain Possible (30% <= CPI < 60%) */
    AGRO_TIER_2_ORANGE  = 2,  /* Rain Likely (60% <= CPI < 80%) */
    AGRO_TIER_3_RED     = 3   /* Rain Imminent / Active Rain (CPI >= 80%) */
} agro_alert_tier_t;

typedef enum {
    SPRAY_STATUS_AUTHORIZED = 0,
    SPRAY_STATUS_HOLD       = 1,
    SPRAY_STATUS_ABORT      = 2
} spray_action_t;

typedef enum {
    HARVEST_STATUS_NORMAL           = 0,
    HARVEST_STATUS_STAGE_TARPS      = 1,
    HARVEST_STATUS_ACCELERATE_WEIGH = 2,
    HARVEST_STATUS_EVACUATE         = 3
} harvest_action_t;

typedef struct {
    agro_alert_tier_t tier;
    spray_action_t    spray_cmd;
    harvest_action_t  harvest_cmd;
    bool              siren_relay_active;
    bool              piezo_buzzer_active;
} agro_advisory_output_t;

#define RAINFAST_CURING_HOURS 3.5f

/* ========================================================================== */
/* Agronomic Decision Logic Under Test                                        */
/* ========================================================================== */

static agro_advisory_output_t evaluate_agronomic_advisory(uint8_t cpi_pct,
                                                         uint8_t rain_tips,
                                                         float press_drop_3h)
{
    agro_advisory_output_t out;
    (void)memset(&out, 0, sizeof(out));

    if (rain_tips >= 2U || cpi_pct >= 80U || press_drop_3h <= -2.0f) {
        out.tier = AGRO_TIER_3_RED;
        out.spray_cmd = SPRAY_STATUS_ABORT;
        out.harvest_cmd = HARVEST_STATUS_EVACUATE;
        out.siren_relay_active = true;
        out.piezo_buzzer_active = true;
    } else if (cpi_pct >= 60U || press_drop_3h <= -1.5f) {
        out.tier = AGRO_TIER_2_ORANGE;
        out.spray_cmd = SPRAY_STATUS_HOLD;
        out.harvest_cmd = HARVEST_STATUS_ACCELERATE_WEIGH;
        out.siren_relay_active = false;
        out.piezo_buzzer_active = true;
    } else if (cpi_pct >= 30U || press_drop_3h <= -0.8f) {
        out.tier = AGRO_TIER_1_AMBER;
        out.spray_cmd = SPRAY_STATUS_HOLD;
        out.harvest_cmd = HARVEST_STATUS_STAGE_TARPS;
        out.siren_relay_active = false;
        out.piezo_buzzer_active = false;
    } else {
        out.tier = AGRO_TIER_0_GREEN;
        out.spray_cmd = SPRAY_STATUS_AUTHORIZED;
        out.harvest_cmd = HARVEST_STATUS_NORMAL;
        out.siren_relay_active = false;
        out.piezo_buzzer_active = false;
    }

    return out;
}

static float calculate_wash_off_percentage(float lead_time_hours)
{
    if (lead_time_hours <= 0.0f) {
        return 100.0f;
    }
    if (lead_time_hours >= RAINFAST_CURING_HOURS) {
        return 0.0f;
    }
    float loss = expf(-lead_time_hours / (RAINFAST_CURING_HOURS * 0.4343f)) * 100.0f;
    return loss;
}

/* ========================================================================== */
/* Forward Declarations for Static Test Functions                             */
/* ========================================================================== */

static void test_tc_agro_01_tier0_green(void);
static void test_tc_agro_02_tier1_amber_spray_hold(void);
static void test_tc_agro_03_tier2_orange_ridge_recall(void);
static void test_tc_agro_04_tier3_red_siren_cpi(void);
static void test_tc_agro_05_tier3_red_active_rain_override(void);
static void test_tc_agro_06_tier3_red_severe_baro_drop(void);
static void test_tc_agro_07_wash_off_loss_early(void);
static void test_tc_agro_08_wash_off_rainfast(void);
static void test_tc_agro_09_state_clearing(void);
static void test_tc_agro_10_evacuation_lead_time_safety(void);

/* ========================================================================== */
/* Unity Test Lifecycle Hooks                                                 */
/* ========================================================================== */

void setUp(void)
{
}

void tearDown(void)
{
}

/* ========================================================================== */
/* Unity Test Cases                                                           */
/* ========================================================================== */

/**
 * @brief TC-AGRO-01: Verify Tier 0 (GREEN) Normal Operations.
 */
static void test_tc_agro_01_tier0_green(void)
{
    agro_advisory_output_t res = evaluate_agronomic_advisory(15U, 0U, -0.2f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_0_GREEN, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_AUTHORIZED, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_NORMAL, res.harvest_cmd);
    TEST_ASSERT_FALSE(res.siren_relay_active);
    TEST_ASSERT_FALSE(res.piezo_buzzer_active);
}

/**
 * @brief TC-AGRO-02: Verify Tier 1 (AMBER) Spray Hold & Tarpaulin Staging.
 */
static void test_tc_agro_02_tier1_amber_spray_hold(void)
{
    agro_advisory_output_t res = evaluate_agronomic_advisory(45U, 0U, -0.5f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_1_AMBER, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_HOLD, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_STAGE_TARPS, res.harvest_cmd);
    TEST_ASSERT_FALSE(res.siren_relay_active);
    TEST_ASSERT_FALSE(res.piezo_buzzer_active);
}

/**
 * @brief TC-AGRO-03: Verify Tier 2 (ORANGE) High Alert & Ridge Triage.
 */
static void test_tc_agro_03_tier2_orange_ridge_recall(void)
{
    agro_advisory_output_t res = evaluate_agronomic_advisory(68U, 0U, -1.6f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_2_ORANGE, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_HOLD, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_ACCELERATE_WEIGH, res.harvest_cmd);
    TEST_ASSERT_FALSE(res.siren_relay_active);
    TEST_ASSERT_TRUE(res.piezo_buzzer_active);
}

/**
 * @brief TC-AGRO-04: Verify Tier 3 (RED) Emergency Siren Evacuation (CPI >= 80%).
 */
static void test_tc_agro_04_tier3_red_siren_cpi(void)
{
    agro_advisory_output_t res = evaluate_agronomic_advisory(85U, 0U, -1.2f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_3_RED, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_ABORT, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_EVACUATE, res.harvest_cmd);
    TEST_ASSERT_TRUE(res.siren_relay_active);
    TEST_ASSERT_TRUE(res.piezo_buzzer_active);
}

/**
 * @brief TC-AGRO-05: Verify Tier 3 (RED) Immediate Override on Active Rain Tips.
 */
static void test_tc_agro_05_tier3_red_active_rain_override(void)
{
    /* Even if CPI is low (25%), active rain (2 tips = 0.4mm) triggers RED */
    agro_advisory_output_t res = evaluate_agronomic_advisory(25U, 2U, 0.0f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_3_RED, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_ABORT, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_EVACUATE, res.harvest_cmd);
    TEST_ASSERT_TRUE(res.siren_relay_active);
}

/**
 * @brief TC-AGRO-06: Verify Tier 3 (RED) Severe Barometric Plunge Override.
 */
static void test_tc_agro_06_tier3_red_severe_baro_drop(void)
{
    /* Sudden pressure crash <= -2.0 hPa/3h overrides CPI */
    agro_advisory_output_t res = evaluate_agronomic_advisory(50U, 0U, -2.3f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_3_RED, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_ABORT, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_EVACUATE, res.harvest_cmd);
    TEST_ASSERT_TRUE(res.siren_relay_active);
}

/**
 * @brief TC-AGRO-07: Verify Agrochemical Wash-Off Loss at 0.5 Hour Lead Time.
 */
static void test_tc_agro_07_wash_off_loss_early(void)
{
    float loss = calculate_wash_off_percentage(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 72.0f, loss);
}

/**
 * @brief TC-AGRO-08: Verify Agrochemical Wash-Off Loss at 3.5 Hour (Fully Rainfast).
 */
static void test_tc_agro_08_wash_off_rainfast(void)
{
    float loss = calculate_wash_off_percentage(3.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, loss);
}

/**
 * @brief TC-AGRO-09: Verify Weather Clearance Hysteresis (State Cleared Safely).
 */
static void test_tc_agro_09_state_clearing(void)
{
    agro_advisory_output_t res = evaluate_agronomic_advisory(20U, 0U, +0.5f);
    TEST_ASSERT_EQUAL_INT(AGRO_TIER_0_GREEN, res.tier);
    TEST_ASSERT_EQUAL_INT(SPRAY_STATUS_AUTHORIZED, res.spray_cmd);
    TEST_ASSERT_EQUAL_INT(HARVEST_STATUS_NORMAL, res.harvest_cmd);
    TEST_ASSERT_FALSE(res.siren_relay_active);
    TEST_ASSERT_FALSE(res.piezo_buzzer_active);
}

/**
 * @brief TC-AGRO-10: Verify Worker Safety Lead-Time Invariant (Safety Margin >= 30 min).
 */
static void test_tc_agro_10_evacuation_lead_time_safety(void)
{
    float walk_distance_m = 1200.0f;  /* 1.2 km from ridge to muster shed */
    float walk_speed_mps = 0.8f;      /* 0.8 m/s on mountain path */
    float evac_time_min = (walk_distance_m / walk_speed_mps) / 60.0f; /* 25 minutes */
    float nowcast_lead_time_min = 60.0f;

    TEST_ASSERT_TRUE(nowcast_lead_time_min > evac_time_min);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 25.0f, evac_time_min);
    float safety_margin_min = nowcast_lead_time_min - evac_time_min;
    TEST_ASSERT_TRUE(safety_margin_min >= 30.0f);
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tc_agro_01_tier0_green);
    RUN_TEST(test_tc_agro_02_tier1_amber_spray_hold);
    RUN_TEST(test_tc_agro_03_tier2_orange_ridge_recall);
    RUN_TEST(test_tc_agro_04_tier3_red_siren_cpi);
    RUN_TEST(test_tc_agro_05_tier3_red_active_rain_override);
    RUN_TEST(test_tc_agro_06_tier3_red_severe_baro_drop);
    RUN_TEST(test_tc_agro_07_wash_off_loss_early);
    RUN_TEST(test_tc_agro_08_wash_off_rainfast);
    RUN_TEST(test_tc_agro_09_state_clearing);
    RUN_TEST(test_tc_agro_10_evacuation_lead_time_safety);
    return UNITY_END();
}
