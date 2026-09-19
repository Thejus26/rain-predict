/**
 * @file    test_lorawan_service.c
 * @brief   ThrowTheSwitch Unity unit test suite for STM32WL LoRaWAN Class A Network Service (S5-T4.1).
 * @details Validates OTAA activation, unconfirmed/confirmed uplinks, RX1/RX2 sequence,
 *          downlink dispatching, RF switch steering, and defensive boundary guards.
 */

#include "unity.h"
#include "lorawan_service.h"
#include <string.h>

/* ========================================================================== */
/* Test Constants & Fixtures                                                  */
/* ========================================================================== */

static const lorawan_credentials_t s_test_credentials = {
    .dev_eui  = { 0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU },
    .join_eui = { 0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U },
    .app_key  = { 0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU, 0xD2U, 0xA6U,
                  0xABU, 0xF7U, 0x15U, 0x88U, 0x09U, 0xCFU, 0x4FU, 0x3CU }
};

static lorawan_rx_packet_t s_received_downlink;
static uint32_t            s_rx_callback_invocation_count = 0U;

/* ========================================================================== */
/* Helper Functions                                                           */
/* ========================================================================== */

static void test_rx_callback(const lorawan_rx_packet_t *rx_packet) {
    if (rx_packet != NULL) {
        s_received_downlink = *rx_packet;
        s_rx_callback_invocation_count++;
    }
}

static void helper_join_network(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_TRUE(status.is_joined);
    TEST_ASSERT_EQUAL(LORAWAN_STATE_JOINED, status.state);
}

/* ========================================================================== */
/* Setup & Teardown                                                           */
/* ========================================================================== */

void setUp(void) {
    (void)memset(&s_received_downlink, 0, sizeof(s_received_downlink));
    s_rx_callback_invocation_count = 0U;
    (void)lorawan_reset();
}

void tearDown(void) {
    (void)lorawan_reset();
}

/* ========================================================================== */
/* 10-Point Verification Matrix Tests (TC-LORA-01 through TC-LORA-10)         */
/* ========================================================================== */

/**
 * @brief TC-LORA-01: Unjoined Initial State Verification
 */
static void test_tc_lora_01_unjoined_initial_state(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_UNJOINED, status.state);
    TEST_ASSERT_FALSE(status.is_joined);
    TEST_ASSERT_EQUAL_UINT32(0U, status.fcnt_up);
    TEST_ASSERT_EQUAL_UINT32(0U, status.fcnt_down);
    TEST_ASSERT_EQUAL_UINT32(0U, status.dev_addr);
    TEST_ASSERT_EQUAL_INT16(-120, status.last_rssi_dbm);
    TEST_ASSERT_EQUAL_INT8(0, status.last_snr_db);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_SHUTDOWN, lorawan_get_rf_mode());
}

/**
 * @brief TC-LORA-02: OTAA Join-Request Transition & High-Power RF Switch
 */
static void test_tc_lora_02_otaa_join_request_transition(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_JOINING, status.state);
    TEST_ASSERT_FALSE(status.is_joined);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_TX_HP, lorawan_get_rf_mode());
}

/**
 * @brief TC-LORA-03: Join-Accept Processing & Network Parameters Allocation
 */
static void test_tc_lora_03_join_accept_processing(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());

    /* Execute process step to ingest simulated Join-Accept */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_JOINED, status.state);
    TEST_ASSERT_TRUE(status.is_joined);
    TEST_ASSERT_EQUAL_HEX32(0x26011234U, status.dev_addr);
    TEST_ASSERT_EQUAL_UINT32(0U, status.fcnt_up);
    TEST_ASSERT_EQUAL_UINT32(0U, status.fcnt_down);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_SHUTDOWN, lorawan_get_rf_mode());
}

/**
 * @brief TC-LORA-04: Unconfirmed Uplink Transmission
 */
static void test_tc_lora_04_unconfirmed_uplink_transmission(void) {
    helper_join_network();

    const uint8_t telemetry_payload[12] = {
        0x08U, 0x98U, 0x19U, 0x64U, 0xC5U, 0x60U, 0x00U, 0x64U, 0x05U, 0x14U, 0x00U, 0x3CU
    };

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC,
                                                          telemetry_payload,
                                                          sizeof(telemetry_payload)));

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_TX_UPLINK, status.state);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_TX_HP, lorawan_get_rf_mode());
}

/**
 * @brief TC-LORA-05: RX1/RX2 Sequence & Frame Counter Increment
 */
static void test_tc_lora_05_rx1_rx2_sequence_and_counter_increment(void) {
    helper_join_network();

    const uint8_t telemetry_payload[12] = { 0xAAU };
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC,
                                                          telemetry_payload,
                                                          sizeof(telemetry_payload)));

    /* Step 1: TX Done -> Enter RX1 (+1s) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_WAIT_RX1, status.state);
    TEST_ASSERT_EQUAL_UINT32(1U, status.fcnt_up);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_RX, lorawan_get_rf_mode());

    /* Step 2: RX1 Done -> Enter RX2 (+2s) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_WAIT_RX2, status.state);

    /* Step 3: RX2 Done -> Back to JOINED */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_JOINED, status.state);
    TEST_ASSERT_EQUAL_UINT32(1U, status.fcnt_up);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_SHUTDOWN, lorawan_get_rf_mode());

    /* Send a second unconfirmed frame: verify monotonic fcnt_up increment */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC,
                                                          telemetry_payload,
                                                          sizeof(telemetry_payload)));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step()); /* TX -> RX1 */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step()); /* RX1 -> RX2 */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step()); /* RX2 -> JOINED */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL_UINT32(2U, status.fcnt_up);
}

/**
 * @brief TC-LORA-06: Confirmed Uplink & ACK Reception Handling
 */
static void test_tc_lora_06_confirmed_uplink_and_ack_reception(void) {
    helper_join_network();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_register_rx_callback(test_rx_callback));

    const uint8_t alert_payload[4] = { 0x03U, 0x55U, 0xE8U, 0x1AU };
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_send_confirmed(LORAWAN_FPORT_ALERT,
                                                        alert_payload,
                                                        sizeof(alert_payload)));

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_TX_UPLINK, status.state);

    /* Step 1: TX Uplink Complete -> WAIT_RX1 */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_WAIT_RX1, status.state);
    TEST_ASSERT_EQUAL_UINT32(1U, status.fcnt_up);

    /* Step 2: WAIT_RX1 -> WAIT_RX2 */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_WAIT_RX2, status.state);

    /* Step 3: WAIT_RX2 -> Gateway ACK received -> JOINED */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_JOINED, status.state);
    TEST_ASSERT_TRUE(lorawan_is_ack_received());

    /* Verify callback was notified of ACK */
    TEST_ASSERT_EQUAL_UINT32(1U, s_rx_callback_invocation_count);
    TEST_ASSERT_TRUE(s_received_downlink.is_ack);
    TEST_ASSERT_EQUAL_UINT8(LORAWAN_FPORT_ALERT, s_received_downlink.fport);
}

/**
 * @brief TC-LORA-07: Unjoined Transmission Rejection
 */
static void test_tc_lora_07_unjoined_transmission_rejection(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));

    const uint8_t payload[12] = { 0x01U };

    /* Must reject unconfirmed send */
    TEST_ASSERT_EQUAL(STATUS_ERROR_NOT_INITIALIZED,
                      lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC, payload, sizeof(payload)));

    /* Must reject confirmed send */
    TEST_ASSERT_EQUAL(STATUS_ERROR_NOT_INITIALIZED,
                      lorawan_send_confirmed(LORAWAN_FPORT_ALERT, payload, 4U));

    /* RF switch must remain shutdown */
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_SHUTDOWN, lorawan_get_rf_mode());
}

/**
 * @brief TC-LORA-08: Port and Boundary Clamping
 */
static void test_tc_lora_08_port_and_boundary_clamping(void) {
    helper_join_network();

    uint8_t large_payload[250];
    (void)memset(large_payload, 0xEE, sizeof(large_payload));
    const uint8_t normal_payload[12] = { 0x11U };

    /* Invalid Port 0 (MAC layer reserved) */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_unconfirmed(0U, normal_payload, 12U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_confirmed(0U, normal_payload, 12U));

    /* Invalid Port > 223 (App port limit) */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_unconfirmed(224U, normal_payload, 12U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_confirmed(255U, normal_payload, 12U));

    /* Zero payload length */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC, normal_payload, 0U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_confirmed(LORAWAN_FPORT_PERIODIC, normal_payload, 0U));

    /* Payload length > 242 bytes */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC, large_payload, 243U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_send_confirmed(LORAWAN_FPORT_PERIODIC, large_payload, 250U));

    /* Boundary edge: 242 bytes must be accepted */
    TEST_ASSERT_EQUAL(STATUS_OK,
                      lorawan_send_unconfirmed(LORAWAN_FPORT_PERIODIC, large_payload, 242U));
}

/**
 * @brief TC-LORA-09: Downlink Dispatch Callback Execution
 */
static void test_tc_lora_09_downlink_dispatch_callback(void) {
    helper_join_network();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_register_rx_callback(test_rx_callback));

    /* Simulate 3-byte config downlink on FPort 10 */
    const lorawan_rx_packet_t config_downlink = {
        .fport    = LORAWAN_FPORT_CONFIG,
        .payload  = { 0x01U, 0x00U, 0x3CU }, /* Command: Set Sampling Interval to 60s */
        .length   = 3U,
        .rssi_dbm = -85,
        .snr_db   = 8,
        .is_ack   = false
    };

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_inject_downlink(&config_downlink));

    /* Verify callback invocation and data fidelity */
    TEST_ASSERT_EQUAL_UINT32(1U, s_rx_callback_invocation_count);
    TEST_ASSERT_EQUAL_UINT8(LORAWAN_FPORT_CONFIG, s_received_downlink.fport);
    TEST_ASSERT_EQUAL_UINT8(3U, s_received_downlink.length);
    TEST_ASSERT_EQUAL_HEX8(0x01U, s_received_downlink.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00U, s_received_downlink.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x3CU, s_received_downlink.payload[2]);
    TEST_ASSERT_EQUAL_INT16(-85, s_received_downlink.rssi_dbm);
    TEST_ASSERT_EQUAL_INT8(8, s_received_downlink.snr_db);
    TEST_ASSERT_FALSE(s_received_downlink.is_ack);

    /* Verify downlink counter and diagnostics updated */
    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL_UINT32(1U, status.fcnt_down);
    TEST_ASSERT_EQUAL_INT16(-85, status.last_rssi_dbm);
    TEST_ASSERT_EQUAL_INT8(8, status.last_snr_db);
}

/**
 * @brief TC-LORA-10: Stack Reset & Re-join Safeguards
 */
static void test_tc_lora_10_stack_reset_and_rejoin_safeguards(void) {
    helper_join_network();

    /* Transmit a packet so counters and state are populated */
    const uint8_t payload[12] = { 0x55U };
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_send_unconfirmed(1U, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());

    /* Reset the stack */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_reset());

    lorawan_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_EQUAL(LORAWAN_STATE_UNJOINED, status.state);
    TEST_ASSERT_FALSE(status.is_joined);
    TEST_ASSERT_EQUAL_UINT32(0U, status.fcnt_up);
    TEST_ASSERT_EQUAL_UINT32(0U, status.fcnt_down);
    TEST_ASSERT_EQUAL_UINT32(0U, status.dev_addr);
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_SHUTDOWN, lorawan_get_rf_mode());

    /* Uplinks must now be safely rejected */
    TEST_ASSERT_EQUAL(STATUS_ERROR_NOT_INITIALIZED,
                      lorawan_send_unconfirmed(1U, payload, sizeof(payload)));

    /* Re-join should succeed cleanly */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_status(&status));
    TEST_ASSERT_TRUE(status.is_joined);
    TEST_ASSERT_EQUAL(LORAWAN_STATE_JOINED, status.state);
}

/* ========================================================================== */
/* Additional Robustness, Factory UID & NULL Pointer Guard Tests              */
/* ========================================================================== */

/**
 * @brief Verify initialization with NULL credentials derives factory DevEUI.
 */
static void test_lorawan_init_null_credentials_factory_uid(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(NULL));

    lorawan_credentials_t creds;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_get_credentials(&creds));

    /* DevEUI must not be all zeros */
    bool non_zero = false;
    for (size_t i = 0U; i < LORAWAN_EUI_LENGTH; i++) {
        if (creds.dev_eui[i] != 0U) {
            non_zero = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(non_zero);
}

/**
 * @brief Verify defensive NULL pointer validations across all public APIs.
 */
static void test_lorawan_null_pointer_guards(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));

    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, lorawan_register_rx_callback(NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, lorawan_get_status(NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, lorawan_get_credentials(NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, lorawan_inject_downlink(NULL));

    /* Join network for transmit tests */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());

    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, lorawan_send_unconfirmed(1U, NULL, 12U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, lorawan_send_confirmed(2U, NULL, 4U));
}

/**
 * @brief Verify busy state rejection during active transmission or joining.
 */
static void test_lorawan_busy_state_guards(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());

    /* While in JOINING state, another join request must be rejected as BUSY */
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUSY, lorawan_join_otaa());

    /* Complete join */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());

    const uint8_t payload[12] = { 0x33U };
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_send_unconfirmed(1U, payload, sizeof(payload)));

    /* While in TX_UPLINK state, sending another frame or joining must be rejected as BUSY */
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUSY, lorawan_send_unconfirmed(1U, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUSY, lorawan_send_confirmed(2U, payload, 4U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUSY, lorawan_join_otaa());
}

/**
 * @brief Verify RF switch modes direct setting and querying.
 */
static void test_lorawan_rf_switch_modes(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&s_test_credentials));

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_set_rf_switch(LORAWAN_RF_MODE_RX));
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_RX, lorawan_get_rf_mode());

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_set_rf_switch(LORAWAN_RF_MODE_TX_HP));
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_TX_HP, lorawan_get_rf_mode());

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_set_rf_switch(LORAWAN_RF_MODE_TX_LP));
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_TX_LP, lorawan_get_rf_mode());

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_set_rf_switch(LORAWAN_RF_MODE_SHUTDOWN));
    TEST_ASSERT_EQUAL(LORAWAN_RF_MODE_SHUTDOWN, lorawan_get_rf_mode());
}

/* ========================================================================== */
/* Main Unity Test Runner Execution                                           */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* 10-Point Verification Matrix */
    RUN_TEST(test_tc_lora_01_unjoined_initial_state);
    RUN_TEST(test_tc_lora_02_otaa_join_request_transition);
    RUN_TEST(test_tc_lora_03_join_accept_processing);
    RUN_TEST(test_tc_lora_04_unconfirmed_uplink_transmission);
    RUN_TEST(test_tc_lora_05_rx1_rx2_sequence_and_counter_increment);
    RUN_TEST(test_tc_lora_06_confirmed_uplink_and_ack_reception);
    RUN_TEST(test_tc_lora_07_unjoined_transmission_rejection);
    RUN_TEST(test_tc_lora_08_port_and_boundary_clamping);
    RUN_TEST(test_tc_lora_09_downlink_dispatch_callback);
    RUN_TEST(test_tc_lora_10_stack_reset_and_rejoin_safeguards);

    /* Robustness, Factory UID & Defensive Guards */
    RUN_TEST(test_lorawan_init_null_credentials_factory_uid);
    RUN_TEST(test_lorawan_null_pointer_guards);
    RUN_TEST(test_lorawan_busy_state_guards);
    RUN_TEST(test_lorawan_rf_switch_modes);

    return UNITY_END();
}
