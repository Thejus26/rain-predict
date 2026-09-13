/**
 * @file    test_bsp_power_rails.c
 * @brief   Unit test verification suite for Switched Sensor Power Rails Driver.
 * @details Validates high-side P-MOSFET sensor rail gating, active-LOW battery divider gating,
 *          calibrated 20 ms RC stabilization delays, pre-sleep all-off shutdown, and parameter boundaries.
 */

#include "unity.h"
#include "bsp_power_rails.h"

void setUp(void) {
    bsp_power_rails_test_reset();
    (void)bsp_power_rails_init();
}

void tearDown(void) {
    (void)bsp_power_rails_all_off();
}

/**
 * @brief TC-S3-T3.1-01 & TC-S3-T3.1-02: Default Power-On States & Initialization.
 */
static void test_bsp_power_rails_default_states(void) {
    status_t status = bsp_power_rails_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify sensor rail is default OFF (PA4 LOW) */
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_FALSE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_SENSORS));

    /* Verify battery divider is default OFF (PB1 HIGH / disconnected) */
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE));

    /* Verify delay invocation count is 0 at startup */
    TEST_ASSERT_EQUAL_UINT32(0U, bsp_power_rails_test_get_delay_calls());
}

/**
 * @brief TC-S3-T3.1-03 & TC-S3-T3.1-04: Switched Sensor Rail Activation & Deactivation.
 */
static void test_bsp_power_rail_sensors_control(void) {
    /* 1. Energize Sensor Rail */
    status_t status = bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_SENSORS));

    /* Verify 20 ms RC stabilization guard was executed */
    TEST_ASSERT_EQUAL_UINT32(1U, bsp_power_rails_test_get_delay_calls());
    TEST_ASSERT_EQUAL_UINT32(BSP_POWER_RAIL_SENSORS_STABILIZE_MS, bsp_power_rails_test_get_last_delay_ms());

    /* 2. De-energize Sensor Rail */
    status = bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_FALSE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_SENSORS));

    /* De-assertion does not invoke delay */
    TEST_ASSERT_EQUAL_UINT32(1U, bsp_power_rails_test_get_delay_calls());
}

/**
 * @brief TC-S3-T3.1-05: Battery Divider Power Gating & Active-LOW Logic.
 */
static void test_bsp_power_rail_vbat_sense_control(void) {
    /* 1. Connect Battery Divider (Active LOW) */
    status_t status = bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_FALSE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE)); /* PB1 LOW = ON */

    /* Verify 2 ms stabilization guard was executed */
    TEST_ASSERT_EQUAL_UINT32(1U, bsp_power_rails_test_get_delay_calls());
    TEST_ASSERT_EQUAL_UINT32(BSP_POWER_RAIL_VBAT_STABILIZE_MS, bsp_power_rails_test_get_last_delay_ms());

    /* 2. Disconnect Battery Divider (Inactive HIGH) */
    status = bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE)); /* PB1 HIGH = OFF */
}

/**
 * @brief TC-S3-T3.1-06: Pre-Sleep Master Shutdown Routine.
 */
static void test_bsp_power_rails_all_off_shutdown(void) {
    /* Energize both rails */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, true);
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, true);

    TEST_ASSERT_TRUE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_TRUE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));

    /* Master pre-sleep shutdown */
    status_t status = bsp_power_rails_all_off();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_FALSE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_SENSORS)); /* PA4 LOW */
    TEST_ASSERT_TRUE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_VBAT_SENSE));  /* PB1 HIGH */
}

/**
 * @brief TC-S3-T3.1-07: Parameter Boundary Violations & Error Defenses.
 */
static void test_bsp_power_rails_boundary_defense(void) {
    /* Invalid rail enable */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, bsp_power_rail_enable(BSP_POWER_RAIL_MAX, true));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, bsp_power_rail_enable(BSP_POWER_RAIL_MAX, false));
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_MAX));
    TEST_ASSERT_FALSE(bsp_power_rails_test_get_raw_pin_state(BSP_POWER_RAIL_MAX));

    /* Invalid rail stabilization */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, bsp_power_rail_stabilize(BSP_POWER_RAIL_MAX));
    TEST_ASSERT_EQUAL_UINT32(0U, bsp_power_rail_get_stabilization_ms(BSP_POWER_RAIL_MAX));

    /* Valid stabilization delay accessors */
    TEST_ASSERT_EQUAL_UINT32(20U, bsp_power_rail_get_stabilization_ms(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_EQUAL_UINT32(2U, bsp_power_rail_get_stabilization_ms(BSP_POWER_RAIL_VBAT_SENSE));

    /* Explicit stabilization call */
    status_t status = bsp_power_rail_stabilize(BSP_POWER_RAIL_SENSORS);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(20U, bsp_power_rails_test_get_last_delay_ms());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bsp_power_rails_default_states);
    RUN_TEST(test_bsp_power_rail_sensors_control);
    RUN_TEST(test_bsp_power_rail_vbat_sense_control);
    RUN_TEST(test_bsp_power_rails_all_off_shutdown);
    RUN_TEST(test_bsp_power_rails_boundary_defense);
    return UNITY_END();
}
