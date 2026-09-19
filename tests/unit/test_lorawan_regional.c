/**
 * @file    test_lorawan_regional.c
 * @brief   ThrowTheSwitch Unity test suite for LoRaWAN Regional Channel Plans, Duty-Cycle & ADR.
 * @details Conforms to C99 and MISRA C standards with zero dynamic memory allocation.
 */

#include "unity.h"
#include "lorawan_regional.h"
#include <string.h>

/* ========================================================================== */
/* Unity Setup & Teardown                                                     */
/* ========================================================================== */

void setUp(void) {
    /* Reset to default IN865 before each test */
    (void)lorawan_regional_init(LORAWAN_REGION_IN865, 0U);
}

void tearDown(void) {
    /* Nothing to tear down */
}

/* ========================================================================== */
/* Unit Tests: Regional Channel Plans, Duty-Cycle & ADR                       */
/* ========================================================================== */

/**
 * @brief TC-REG-01: IN865 Initial Channel Table and Round-Robin Hopping
 */
static void test_tc_reg_01_in865_initial_channel_table(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
    TEST_ASSERT_EQUAL_UINT8(3U, lorawan_regional_get_channel_count());
    TEST_ASSERT_EQUAL_INT(LORAWAN_REGION_IN865, lorawan_regional_get_active_region());

    lorawan_channel_t ch0, ch1, ch2;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_channel(0U, &ch0));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_channel(1U, &ch1));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_channel(2U, &ch2));

    TEST_ASSERT_EQUAL_UINT32(865062500U, ch0.frequency_hz);
    TEST_ASSERT_EQUAL_UINT32(865402500U, ch1.frequency_hz);
    TEST_ASSERT_EQUAL_UINT32(865985000U, ch2.frequency_hz);

    TEST_ASSERT_TRUE(ch0.enabled);
    TEST_ASSERT_TRUE(ch1.enabled);
    TEST_ASSERT_TRUE(ch2.enabled);

    /* Test round-robin channel rotation */
    uint32_t freq = 0U;
    uint8_t  dr   = 0U;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(865062500U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(865402500U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(865985000U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);

    /* 4th call wraps back to channel 0 */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(865062500U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);
}

/**
 * @brief TC-REG-02: EU868 Mandatory Channels & RX1/RX2 Defaults
 */
static void test_tc_reg_02_eu868_mandatory_channels_and_rx2(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_EU868, 0U));
    TEST_ASSERT_EQUAL_UINT8(3U, lorawan_regional_get_channel_count());
    TEST_ASSERT_EQUAL_INT(LORAWAN_REGION_EU868, lorawan_regional_get_active_region());

    uint32_t freq = 0U;
    uint8_t  dr   = 0U;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(868100000U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(868300000U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
    TEST_ASSERT_EQUAL_UINT32(868500000U, freq);
    TEST_ASSERT_EQUAL_UINT8(5U, dr);

    /* RX2 Parameters: 869.525 MHz @ DR0 (SF12 / 125 kHz) */
    uint32_t rx2_freq = 0U;
    uint8_t  rx2_dr   = 0xFFU;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_rx2_params(&rx2_freq, &rx2_dr));
    TEST_ASSERT_EQUAL_UINT32(869525000U, rx2_freq);
    TEST_ASSERT_EQUAL_UINT8(0U, rx2_dr);

    /* RX1 Parameters: Same frequency as TX, same DR */
    uint32_t rx1_freq = 0U;
    uint8_t  rx1_dr   = 0xFFU;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_rx1_params(868100000U, 5U, &rx1_freq, &rx1_dr));
    TEST_ASSERT_EQUAL_UINT32(868100000U, rx1_freq);
    TEST_ASSERT_EQUAL_UINT8(5U, rx1_dr);
}

/**
 * @brief TC-REG-03: US915 Sub-Band 2 Channels and RX1/RX2 Mapping
 */
static void test_tc_reg_03_us915_subband2_channels(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_US915, 2U));
    TEST_ASSERT_EQUAL_UINT8(8U, lorawan_regional_get_channel_count());
    TEST_ASSERT_EQUAL_INT(LORAWAN_REGION_US915, lorawan_regional_get_active_region());

    /* Verify all 8 channels in Sub-Band 2 (Channels 8..15: 903.9 MHz to 905.3 MHz in 200 kHz steps) */
    for (uint8_t i = 0U; i < 8U; i++) {
        uint32_t expected_freq = 903900000U + ((uint32_t)i * 200000U);
        uint32_t freq = 0U;
        uint8_t  dr   = 0U;
        TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_tx_channel(&freq, &dr));
        TEST_ASSERT_EQUAL_UINT32(expected_freq, freq);
        TEST_ASSERT_EQUAL_UINT8(3U, dr); /* Default US915 DR is 3 (DR3) */
    }

    /* RX2 Parameters: 923.3 MHz @ DR8 (SF12 / 500 kHz) */
    uint32_t rx2_freq = 0U;
    uint8_t  rx2_dr   = 0xFFU;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_rx2_params(&rx2_freq, &rx2_dr));
    TEST_ASSERT_EQUAL_UINT32(923300000U, rx2_freq);
    TEST_ASSERT_EQUAL_UINT8(8U, rx2_dr);

    /* RX1 Parameters: 923.3 MHz + (Ch % 8) * 600 kHz, DR = 10 - tx_dr */
    uint32_t rx1_freq = 0U;
    uint8_t  rx1_dr   = 0xFFU;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_rx1_params(903900000U, 3U, &rx1_freq, &rx1_dr));
    TEST_ASSERT_EQUAL_UINT32(923300000U, rx1_freq);
    TEST_ASSERT_EQUAL_UINT8(7U, rx1_dr); /* 10 - 3 = 7 */

    /* Second channel: 904.1 MHz (Ch 1) -> 923.3 + 0.6 = 923.9 MHz */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_rx1_params(904100000U, 1U, &rx1_freq, &rx1_dr));
    TEST_ASSERT_EQUAL_UINT32(923900000U, rx1_freq);
    TEST_ASSERT_EQUAL_UINT8(9U, rx1_dr); /* 10 - 1 = 9 */
}

/**
 * @brief TC-REG-04: Exact Time-on-Air (ToA) Formula Accuracy
 */
static void test_tc_reg_04_time_on_air_formula_accuracy(void) {
    /* DR5 (SF7 / 125 kHz) for 12-byte payload: Expected ~41 ms +- 2 ms */
    uint32_t toa_dr5_12b = lorawan_calc_time_on_air_ms(12U, 5U);
    TEST_ASSERT_TRUE(toa_dr5_12b >= 39U);
    TEST_ASSERT_TRUE(toa_dr5_12b <= 43U);

    /* DR0 (SF12 / 125 kHz) for 17-byte payload: Expected 1319 ms +- 10 ms */
    uint32_t toa_dr0_17b = lorawan_calc_time_on_air_ms(17U, 0U);
    TEST_ASSERT_TRUE(toa_dr0_17b >= 1309U);
    TEST_ASSERT_TRUE(toa_dr0_17b <= 1329U);

    /* Verify monotonic ordering across all Data Rates DR5..DR0 for a constant 16-byte payload */
    uint32_t toa_dr5 = lorawan_calc_time_on_air_ms(16U, 5U);
    uint32_t toa_dr4 = lorawan_calc_time_on_air_ms(16U, 4U);
    uint32_t toa_dr3 = lorawan_calc_time_on_air_ms(16U, 3U);
    uint32_t toa_dr2 = lorawan_calc_time_on_air_ms(16U, 2U);
    uint32_t toa_dr1 = lorawan_calc_time_on_air_ms(16U, 1U);
    uint32_t toa_dr0 = lorawan_calc_time_on_air_ms(16U, 0U);

    TEST_ASSERT_TRUE(toa_dr5 < toa_dr4);
    TEST_ASSERT_TRUE(toa_dr4 < toa_dr3);
    TEST_ASSERT_TRUE(toa_dr3 < toa_dr2);
    TEST_ASSERT_TRUE(toa_dr2 < toa_dr1);
    TEST_ASSERT_TRUE(toa_dr1 < toa_dr0);
}

/**
 * @brief TC-REG-05: Duty Cycle Off-Time Enforcement
 */
static void test_tc_reg_05_duty_cycle_off_time_enforcement(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));

    /* 100 ms ToA under 1% duty-cycle -> Toff = 99 * 100 ms = 9900 ms */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_update_duty_cycle(865062500U, 100U));
    TEST_ASSERT_EQUAL_UINT32(9900U, lorawan_regional_get_off_time_ms(865062500U));

    lorawan_duty_cycle_state_t duty_state;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_duty_cycle_state(&duty_state));
    TEST_ASSERT_EQUAL_UINT32(100U, duty_state.accumulated_toa_ms);
    TEST_ASSERT_EQUAL_UINT32(9900U, duty_state.min_off_time_ms);

    /* Check US915: 100% duty-cycle allowance -> Toff = 0 */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_US915, 2U));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_update_duty_cycle(903900000U, 250U));
    TEST_ASSERT_EQUAL_UINT32(0U, lorawan_regional_get_off_time_ms(903900000U));
}

/**
 * @brief TC-REG-06: ADRACKReq Trigger at 64 Uplinks
 */
static void test_tc_reg_06_adr_ack_req_trigger_at_64_uplinks(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));

    bool adr_ack_req = true;

    /* First 63 uplinks without downlink must NOT assert ADRACKReq */
    for (uint16_t i = 1U; i <= 63U; i++) {
        TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_uplink(&adr_ack_req));
        TEST_ASSERT_FALSE(adr_ack_req);
    }

    /* 64th uplink asserts ADRACKReq */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_uplink(&adr_ack_req));
    TEST_ASSERT_TRUE(adr_ack_req);

    lorawan_adr_state_t adr_state;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_adr_state(&adr_state));
    TEST_ASSERT_EQUAL_UINT16(64U, adr_state.adr_ack_cnt);
    TEST_ASSERT_TRUE(adr_state.adr_ack_req);
}

/**
 * @brief TC-REG-07: ADR Step-Down and Power Boost at 96 Uplinks
 */
static void test_tc_reg_07_adr_step_down_at_96_uplinks(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_set_dr(5U));

    bool adr_ack_req = false;

    /* Run 95 uplinks without downlink */
    for (uint16_t i = 1U; i <= 95U; i++) {
        TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_uplink(&adr_ack_req));
    }

    lorawan_adr_state_t state_before;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_adr_state(&state_before));
    TEST_ASSERT_EQUAL_UINT8(5U, state_before.current_dr);

    /* 96th uplink (64 + 32) triggers step-down: DR 5 -> DR 4, TX power = +22 dBm */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_uplink(&adr_ack_req));
    TEST_ASSERT_TRUE(adr_ack_req);

    lorawan_adr_state_t state_after;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_adr_state(&state_after));
    TEST_ASSERT_EQUAL_UINT8(4U, state_after.current_dr);
    TEST_ASSERT_EQUAL_INT8(22, state_after.current_tx_power);
    TEST_ASSERT_EQUAL_UINT16(64U, state_after.adr_ack_cnt);

    /* Run another 32 uplinks: 64 to 96 -> DR 4 -> DR 3 */
    for (uint16_t i = 1U; i <= 32U; i++) {
        TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_uplink(&adr_ack_req));
    }

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_adr_state(&state_after));
    TEST_ASSERT_EQUAL_UINT8(3U, state_after.current_dr);
    TEST_ASSERT_EQUAL_INT8(22, state_after.current_tx_power);
}

/**
 * @brief TC-REG-08: Downlink ADR Counter Reset
 */
static void test_tc_reg_08_downlink_adr_counter_reset(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));

    bool adr_ack_req = false;

    /* Simulate 70 uplinks (ADRACKReq is armed) */
    for (uint16_t i = 1U; i <= 70U; i++) {
        TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_uplink(&adr_ack_req));
    }
    TEST_ASSERT_TRUE(adr_ack_req);

    /* Downlink arrives with RSSI -80 dBm, SNR +8 dB */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_on_downlink(-80, 8));

    lorawan_adr_state_t adr_state;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_adr_state(&adr_state));
    TEST_ASSERT_EQUAL_UINT16(0U, adr_state.adr_ack_cnt);
    TEST_ASSERT_FALSE(adr_state.adr_ack_req);
}

/**
 * @brief TC-REG-09: LinkADRReq MAC Processing and Channel Masking
 */
static void test_tc_reg_09_link_adr_req_mac_processing(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));

    /* 4-byte payload:
     * Byte 0: (DR3 << 4) | (Power 2) = 0x32
     * Byte 1: 0x07 (Channels 0, 1, 2 enabled)
     * Byte 2: 0x00
     * Byte 3: 0x02 (NbTrans = 2)
     */
    const uint8_t req_payload[4] = { 0x32U, 0x07U, 0x00U, 0x02U };
    uint8_t ans = 0x00U;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_process_link_adr_req(req_payload, &ans));
    /* All ACKs set: Bit 0 (ChMask), Bit 1 (DR), Bit 2 (Power) = 0x07 */
    TEST_ASSERT_EQUAL_UINT8(0x07U, ans);

    lorawan_adr_state_t adr_state;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_regional_get_adr_state(&adr_state));
    TEST_ASSERT_EQUAL_UINT8(3U, adr_state.current_dr);
    TEST_ASSERT_EQUAL_UINT8(2U, adr_state.nb_trans);

    /* Test rejected DR (DR > 5) */
    const uint8_t invalid_dr_payload[4] = { 0x62U, 0x07U, 0x00U, 0x01U };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_process_link_adr_req(invalid_dr_payload, &ans));
    /* DR bit (Bit 1) must be 0 -> ans == 0x05 */
    TEST_ASSERT_EQUAL_UINT8(0x05U, ans);

    /* Test rejected Channel Mask (mask == 0) */
    const uint8_t invalid_mask_payload[4] = { 0x32U, 0x00U, 0x00U, 0x01U };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, lorawan_adr_process_link_adr_req(invalid_mask_payload, &ans));
    /* ChMask bit (Bit 0) must be 0 -> ans == 0x06 */
    TEST_ASSERT_EQUAL_UINT8(0x06U, ans);
}

/**
 * @brief TC-REG-10: Parameter Validation and Defensive Null / Bounds Guards
 */
static void test_tc_reg_10_parameter_validation_guards(void) {
    /* Invalid region */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, lorawan_regional_init((lorawan_region_t)99, 0U));

    /* Invalid Data Rate */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_OUT_OF_BOUNDS, lorawan_regional_set_dr(10U));

    /* Invalid TX Power */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_OUT_OF_BOUNDS, lorawan_regional_set_tx_power(35));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_OUT_OF_BOUNDS, lorawan_regional_set_tx_power(-5));

    /* Null pointer guards */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_tx_channel(NULL, NULL));
    uint32_t dummy_freq = 0U;
    uint8_t  dummy_dr   = 0U;
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_tx_channel(&dummy_freq, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_tx_channel(NULL, &dummy_dr));

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_rx1_params(865000000U, 0U, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_rx2_params(NULL, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_adr_on_uplink(NULL));

    uint8_t ans = 0U;
    const uint8_t dummy_payload[4] = { 0x00U, 0x00U, 0x00U, 0x00U };
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_adr_process_link_adr_req(NULL, &ans));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_adr_process_link_adr_req(dummy_payload, NULL));

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_adr_state(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_duty_cycle_state(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, lorawan_regional_get_channel(0U, NULL));
    lorawan_channel_t ch;
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_OUT_OF_BOUNDS, lorawan_regional_get_channel(99U, &ch));
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tc_reg_01_in865_initial_channel_table);
    RUN_TEST(test_tc_reg_02_eu868_mandatory_channels_and_rx2);
    RUN_TEST(test_tc_reg_03_us915_subband2_channels);
    RUN_TEST(test_tc_reg_04_time_on_air_formula_accuracy);
    RUN_TEST(test_tc_reg_05_duty_cycle_off_time_enforcement);
    RUN_TEST(test_tc_reg_06_adr_ack_req_trigger_at_64_uplinks);
    RUN_TEST(test_tc_reg_07_adr_step_down_at_96_uplinks);
    RUN_TEST(test_tc_reg_08_downlink_adr_counter_reset);
    RUN_TEST(test_tc_reg_09_link_adr_req_mac_processing);
    RUN_TEST(test_tc_reg_10_parameter_validation_guards);

    return UNITY_END();
}
