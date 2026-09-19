/**
 * @file    test_lorawan_tx_queue.c
 * @brief   ThrowTheSwitch Unity unit test suite for LoRaWAN Priority TX Queue Manager.
 * @details Verifies TC-QUE-01 through TC-QUE-10 covering prioritization, preemption,
 *          periodic deduplication, duty-cycle gating, retries, and defensive guards.
 */

#include "unity.h"
#include "lorawan_tx_queue.h"
#include "lorawan_service.h"
#include "lorawan_regional.h"
#include "flash_storage.h"
#include "flash_playback.h"
#include <string.h>

/* ========================================================================== */
/* Static Test Helpers & Callback Tracking                                    */
/* ========================================================================== */

static uint32_t               s_cb_count = 0U;
static lorawan_tx_priority_t  s_last_cb_priority = TX_PRIORITY_MAC_RESP;
static uint8_t                s_last_cb_fport = 0U;
static bool                   s_last_cb_success = false;
static void                  *s_last_cb_user_ctx = NULL;

static void test_tx_completion_callback(lorawan_tx_priority_t priority,
                                        uint8_t fport,
                                        bool success,
                                        void *user_ctx) {
    s_cb_count++;
    s_last_cb_priority = priority;
    s_last_cb_fport    = fport;
    s_last_cb_success  = success;
    s_last_cb_user_ctx = user_ctx;
}

static void helper_join_lorawan(void) {
    lorawan_credentials_t creds;
    (void)memset(&creds, 0x11, sizeof(creds));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_init(&creds));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_join_otaa());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_service_process_step());
}

static void helper_drain_radio_steps(void) {
    (void)lorawan_service_process_step();
    (void)lorawan_service_process_step();
    (void)lorawan_service_process_step();
}

/* ========================================================================== */
/* Setup & Teardown                                                           */
/* ========================================================================== */

void setUp(void) {
    s_cb_count         = 0U;
    s_last_cb_priority = TX_PRIORITY_MAC_RESP;
    s_last_cb_fport    = 0U;
    s_last_cb_success  = false;
    s_last_cb_user_ctx = NULL;

    (void)lorawan_reset();
    (void)lorawan_regional_init(LORAWAN_REGION_IN865, 0U);
    (void)flash_playback_init(NULL);
    (void)lorawan_tx_queue_init();
    helper_join_lorawan();
}

void tearDown(void) {
    (void)lorawan_tx_queue_clear();
    (void)lorawan_reset();
}

/* ========================================================================== */
/* 10-Point Verification Matrix Tests (TC-QUE-01 through TC-QUE-10)           */
/* ========================================================================== */

/**
 * @brief TC-QUE-01: Empty Queue Process Tick
 */
static void test_tc_que_01_empty_queue_process_tick(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_init());

    status_t st = lorawan_tx_queue_process_step();
    TEST_ASSERT_EQUAL(STATUS_ERROR_IDLE, st);

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(0U, status.count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.urgent_count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.periodic_count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.playback_count);
    TEST_ASSERT_FALSE(status.is_busy);
    TEST_ASSERT_FALSE(status.is_duty_blocked);
}

/**
 * @brief TC-QUE-02: Single Periodic Telemetry Enqueue
 */
static void test_tc_que_02_single_periodic_telemetry_enqueue(void) {
    const uint8_t periodic_buf[12] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_periodic(periodic_buf, 12U, NULL, NULL));

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(1U, status.count);
    TEST_ASSERT_EQUAL_UINT8(1U, status.periodic_count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.urgent_count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.playback_count);

    lorawan_tx_item_t item;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_peek(0U, &item));
    TEST_ASSERT_EQUAL_UINT8(LORAWAN_FPORT_PERIODIC, item.fport);
    TEST_ASSERT_EQUAL(TX_PRIORITY_PERIODIC, item.priority);
    TEST_ASSERT_FALSE(item.is_confirmed);
    TEST_ASSERT_EQUAL_UINT8(12U, item.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(periodic_buf, item.payload, 12U);
}

/**
 * @brief TC-QUE-03: Urgent Alert Preemption (Priority Inversion)
 */
static void test_tc_que_03_urgent_alert_preemption(void) {
    const uint8_t periodic_buf[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const uint8_t alert_buf[4]     = {0xAA, 0xBB, 0xCC, 0xDD};

    /* Enqueue periodic first, then urgent alert */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_periodic(periodic_buf, 12U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_alert(alert_buf, 4U, NULL, NULL));

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(2U, status.count);
    TEST_ASSERT_EQUAL_UINT8(1U, status.urgent_count);
    TEST_ASSERT_EQUAL_UINT8(1U, status.periodic_count);

    /* Step 1: Head-of-line priority MUST extract Alert (Tier 0) first */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_TRUE(status.is_busy);

    /* Complete Alert TX */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
    helper_drain_radio_steps();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_FALSE(status.is_busy);
    TEST_ASSERT_EQUAL_UINT8(1U, status.count);
    TEST_ASSERT_EQUAL_UINT8(0U, status.urgent_count);
    TEST_ASSERT_EQUAL_UINT8(1U, status.periodic_count);

    /* Step 2: Extract Periodic (Tier 1) second */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_TRUE(status.is_busy);

    /* Complete Periodic TX */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
    helper_drain_radio_steps();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(0U, status.count);
    TEST_ASSERT_FALSE(status.is_busy);
}

/**
 * @brief TC-QUE-04: Periodic Deduplication Rule
 */
static void test_tc_que_04_periodic_deduplication_rule(void) {
    const uint8_t frame_a[12] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                                 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
    const uint8_t frame_b[12] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                                 0x22, 0x22, 0x22, 0x22, 0x22, 0x22};

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_periodic(frame_a, 12U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_periodic(frame_b, 12U, NULL, NULL));

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(1U, status.count);
    TEST_ASSERT_EQUAL_UINT8(1U, status.periodic_count);

    lorawan_tx_item_t item;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_peek(0U, &item));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame_b, item.payload, 12U);
}

/**
 * @brief TC-QUE-05: Duty Cycle Blocking Enforcement
 */
static void test_tc_que_05_duty_cycle_blocking_enforcement(void) {
    /* Arm regional duty-cycle off-time */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_update_duty_cycle(865062500U, 51U));
    TEST_ASSERT_TRUE(lorawan_regional_get_off_time_ms(0U) > 5000U);

    /* Enqueue low-priority Tier 2 playback */
    const uint8_t playback_buf[32] = {0xAB};
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(playback_buf, 32U, false, NULL, NULL));

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_TRUE(status.is_duty_blocked);
    TEST_ASSERT_EQUAL_UINT8(1U, status.playback_count);

    /* Step MUST be blocked by duty-cycle off-time */
    status_t st = lorawan_tx_queue_process_step();
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUSY, st);

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_FALSE(status.is_busy);

    /* Clear off-time */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, lorawan_regional_get_off_time_ms(0U));

    /* Step MUST now dispatch cleanly */
    st = lorawan_tx_queue_process_step();
    TEST_ASSERT_EQUAL(STATUS_OK, st);

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_TRUE(status.is_busy);

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
}

/**
 * @brief TC-QUE-06: Urgent Alert Duty-Cycle Emergency Bypass
 */
static void test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass(void) {
    /* Set active duty off-time */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_update_duty_cycle(865062500U, 51U));
    TEST_ASSERT_TRUE(lorawan_regional_get_off_time_ms(0U) > 0U);

    /* Enqueue Tier 0 urgent alert */
    const uint8_t alert_buf[4] = {0x77, 0x88, 0x99, 0xAA};
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_alert(alert_buf, 4U, NULL, NULL));

    /* Emergency bypass MUST dispatch even though off-time > 0 */
    status_t st = lorawan_tx_queue_process_step();
    TEST_ASSERT_EQUAL(STATUS_OK, st);

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_TRUE(status.is_busy);

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
}

/**
 * @brief TC-QUE-07: Confirmed ACK Completion Callback
 */
static void test_tc_que_07_confirmed_ack_completion_callback(void) {
    const uint8_t payload[16] = {0x0F};
    uint32_t user_magic = 0xCAFEBABE;

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(payload, 16U, true,
                                                                   test_tx_completion_callback,
                                                                   &user_magic));

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));

    TEST_ASSERT_EQUAL_UINT32(1U, s_cb_count);
    TEST_ASSERT_EQUAL(TX_PRIORITY_PLAYBACK, s_last_cb_priority);
    TEST_ASSERT_EQUAL_UINT8(LORAWAN_FPORT_PLAYBACK, s_last_cb_fport);
    TEST_ASSERT_TRUE(s_last_cb_success);
    TEST_ASSERT_EQUAL_PTR(&user_magic, s_last_cb_user_ctx);

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(0U, status.count);
}

/**
 * @brief TC-QUE-08: Confirmed NACK Retry Counter
 */
static void test_tc_que_08_confirmed_nack_retry_counter(void) {
    const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(payload, 8U, true,
                                                                   test_tx_completion_callback,
                                                                   NULL));

    /* 1st transmission attempt fails (NACK) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(false));
    helper_drain_radio_steps();

    lorawan_tx_item_t item;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_peek(0U, &item));
    TEST_ASSERT_EQUAL_UINT8(1U, item.retry_count);
    TEST_ASSERT_TRUE(item.in_use);
    TEST_ASSERT_EQUAL_UINT32(0U, s_cb_count);

    /* 2nd transmission attempt fails (NACK) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(false));
    helper_drain_radio_steps();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_peek(0U, &item));
    TEST_ASSERT_EQUAL_UINT8(2U, item.retry_count);
    TEST_ASSERT_TRUE(item.in_use);
    TEST_ASSERT_EQUAL_UINT32(0U, s_cb_count);
}

/**
 * @brief TC-QUE-09: Max Retries Dropped & Callback Invocation
 */
static void test_tc_que_09_max_retries_dropped_and_callback(void) {
    const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(payload, 8U, true,
                                                                   test_tx_completion_callback,
                                                                   NULL));

    /* Attempts 1 and 2 */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(false));
    helper_drain_radio_steps();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(false));
    helper_drain_radio_steps();

    /* Attempt 3 (reaches max retries = 3) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(false));
    helper_drain_radio_steps();

    /* Callback invoked with success == false */
    TEST_ASSERT_EQUAL_UINT32(1U, s_cb_count);
    TEST_ASSERT_FALSE(s_last_cb_success);
    TEST_ASSERT_EQUAL(TX_PRIORITY_PLAYBACK, s_last_cb_priority);

    /* Item dropped from queue */
    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(0U, status.count);
}

/**
 * @brief TC-QUE-10: Capacity Overflow & NULL Parameter Guards
 */
static void test_tc_que_10_capacity_overflow_and_null_guards(void) {
    const uint8_t dummy[4] = {1, 2, 3, 4};

    /* Fill all 8 queue slots */
    for (uint8_t i = 0U; i < LORAWAN_TX_QUEUE_CAPACITY; i++) {
        TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(dummy, 4U, false, NULL, NULL));
    }

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(8U, status.count);

    /* 9th item MUST be rejected with STATUS_ERROR_OUT_OF_BOUNDS */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_tx_queue_enqueue_playback(dummy, 4U, false, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_tx_queue_enqueue_alert(dummy, 4U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_tx_queue_enqueue_mac_resp(dummy, 4U, NULL, NULL));

    /* NULL Pointer Guards */
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER,
                      lorawan_tx_queue_enqueue_alert(NULL, 4U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER,
                      lorawan_tx_queue_enqueue_periodic(NULL, 12U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER,
                      lorawan_tx_queue_enqueue_playback(NULL, 4U, false, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER,
                      lorawan_tx_queue_enqueue_mac_resp(NULL, 4U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER,
                      lorawan_tx_queue_get_status(NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER,
                      lorawan_tx_queue_peek(0U, NULL));

    /* Length Boundary Guards */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_tx_queue_enqueue_alert(dummy, 0U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_tx_queue_enqueue_alert(dummy, 243U, NULL, NULL));
    lorawan_tx_item_t dummy_item;
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS,
                      lorawan_tx_queue_peek(8U, &dummy_item));
}

/**
 * @brief Additional Test: MAC Response Enqueue and Full Priority Tier Hierarchy
 */
static void test_tc_que_11_full_priority_tier_hierarchy(void) {
    const uint8_t d[4] = {0x55, 0xAA, 0x55, 0xAA};

    /* Enqueue in reverse priority order: Tier 3, Tier 2, Tier 1, Tier 0 */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_mac_resp(d, 4U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(d, 4U, false, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_periodic(d, 4U, NULL, NULL));
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_alert(d, 4U, NULL, NULL));

    lorawan_tx_queue_status_t status;
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(4U, status.count);

    /* 1. Should dispatch Tier 0 (Alert) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
    helper_drain_radio_steps();

    /* 2. Should dispatch Tier 1 (Periodic) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
    helper_drain_radio_steps();

    /* 3. Should dispatch Tier 2 (Playback) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
    helper_drain_radio_steps();

    /* 4. Should dispatch Tier 3 (MAC Response) */
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_process_step());
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_on_tx_complete(true));
    helper_drain_radio_steps();

    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_get_status(&status));
    TEST_ASSERT_EQUAL_UINT8(0U, status.count);
}

/**
 * @brief Additional Test: Flash Playback Preemption on Alert Enqueue
 */
static void test_tc_que_12_playback_preemption_on_alert(void) {
    /* Initialize flash storage and push a telemetry record */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_init());
    uint8_t dummy_telemetry[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(dummy_telemetry, 1U));

    /* Trigger a playback session and build a batch */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_playback_trigger());
    flash_playback_batch_t batch;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_playback_build_next_batch(5U, &batch));

    flash_playback_status_t pb_status;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_playback_get_status(&pb_status));
    TEST_ASSERT_TRUE(pb_status.is_active);
    TEST_ASSERT_EQUAL(PLAYBACK_STATE_WAIT_ACK, pb_status.state);

    /* Enqueueing Tier 0 Alert MUST preempt flash playback */
    const uint8_t alert[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_alert(alert, 4U, NULL, NULL));

    TEST_ASSERT_EQUAL(STATUS_OK, flash_playback_get_status(&pb_status));
    TEST_ASSERT_EQUAL(PLAYBACK_STATE_PREEMPTED, pb_status.state);
}

/* ========================================================================== */
/* Test Runner Main Entry Point                                               */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tc_que_01_empty_queue_process_tick);
    RUN_TEST(test_tc_que_02_single_periodic_telemetry_enqueue);
    RUN_TEST(test_tc_que_03_urgent_alert_preemption);
    RUN_TEST(test_tc_que_04_periodic_deduplication_rule);
    RUN_TEST(test_tc_que_05_duty_cycle_blocking_enforcement);
    RUN_TEST(test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass);
    RUN_TEST(test_tc_que_07_confirmed_ack_completion_callback);
    RUN_TEST(test_tc_que_08_confirmed_nack_retry_counter);
    RUN_TEST(test_tc_que_09_max_retries_dropped_and_callback);
    RUN_TEST(test_tc_que_10_capacity_overflow_and_null_guards);
    RUN_TEST(test_tc_que_11_full_priority_tier_hierarchy);
    RUN_TEST(test_tc_que_12_playback_preemption_on_alert);

    return UNITY_END();
}
