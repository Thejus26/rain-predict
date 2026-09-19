/**
 * @file    lorawan_tx_queue.c
 * @brief   LoRaWAN multi-tier priority transmission queue implementation.
 * @details Manages prioritized uplink queuing, preemption, deduplication,
 *          and duty-cycle gating for STM32WLE5 LoRaWAN operations.
 */

#include "lorawan_tx_queue.h"
#include "lorawan_regional.h"
#include "flash_playback.h"
#include <string.h>

/* ========================================================================== */
/* Static Module State & Queue Storage                                        */
/* ========================================================================== */

static lorawan_tx_item_t s_queue_slots[LORAWAN_TX_QUEUE_CAPACITY];
static int8_t            s_active_transmitting_idx = -1;
static bool              s_is_initialized = false;

/* ========================================================================== */
/* Private Helper Functions                                                   */
/* ========================================================================== */

/**
 * @brief  Finds the index of the highest priority pending queue item.
 * @return Slot index (0..7), or -1 if queue is empty.
 */
static int8_t lorawan_tx_queue_find_highest_priority(void) {
    int8_t best_idx = -1;
    lorawan_tx_priority_t highest_prio = (lorawan_tx_priority_t)(TX_PRIORITY_MAC_RESP + 1);

    for (uint8_t i = 0U; i < LORAWAN_TX_QUEUE_CAPACITY; i++) {
        if (s_queue_slots[i].in_use && ((int8_t)i != s_active_transmitting_idx)) {
            if (s_queue_slots[i].priority < highest_prio) {
                highest_prio = s_queue_slots[i].priority;
                best_idx = (int8_t)i;
                if (highest_prio == TX_PRIORITY_URGENT) {
                    break; /* Tier 0 is absolute highest priority */
                }
            }
        }
    }
    return best_idx;
}

/**
 * @brief  Finds an empty slot in the queue array.
 * @return Free slot index (0..7), or -1 if queue is full.
 */
static int8_t lorawan_tx_queue_find_free_slot(void) {
    for (uint8_t i = 0U; i < LORAWAN_TX_QUEUE_CAPACITY; i++) {
        if (!s_queue_slots[i].in_use) {
            return (int8_t)i;
        }
    }
    return -1;
}

/* ========================================================================== */
/* Public API Implementation                                                  */
/* ========================================================================== */

status_t lorawan_tx_queue_init(void) {
    (void)memset(s_queue_slots, 0, sizeof(s_queue_slots));
    s_active_transmitting_idx = -1;
    s_is_initialized = true;
    return STATUS_OK;
}

status_t lorawan_tx_queue_enqueue_alert(const uint8_t *payload,
                                        uint8_t length,
                                        lorawan_tx_completion_cb_t completion_cb,
                                        void *user_ctx) {
    if (!s_is_initialized) {
        (void)lorawan_tx_queue_init();
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if ((length == 0U) || (length > LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    int8_t slot_idx = lorawan_tx_queue_find_free_slot();
    if (slot_idx < 0) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    lorawan_tx_item_t *item = &s_queue_slots[slot_idx];
    (void)memcpy(item->payload, payload, length);
    item->length        = length;
    item->fport         = LORAWAN_FPORT_ALERT;
    item->priority      = TX_PRIORITY_URGENT;
    item->is_confirmed  = true;
    item->max_retries   = LORAWAN_DEFAULT_MAX_RETRIES;
    item->retry_count   = 0U;
    item->completion_cb = completion_cb;
    item->user_ctx      = user_ctx;
    item->in_use        = true;

    /* Preempt any active historical playback session immediately */
    (void)flash_playback_preempt();

    return STATUS_OK;
}

status_t lorawan_tx_queue_enqueue_periodic(const uint8_t *payload,
                                           uint8_t length,
                                           lorawan_tx_completion_cb_t completion_cb,
                                           void *user_ctx) {
    if (!s_is_initialized) {
        (void)lorawan_tx_queue_init();
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if ((length == 0U) || (length > LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    /* Deduplication: Replace existing pending Tier 1 periodic frame if not actively transmitting */
    int8_t slot_idx = -1;
    for (uint8_t i = 0U; i < LORAWAN_TX_QUEUE_CAPACITY; i++) {
        if (s_queue_slots[i].in_use && (s_queue_slots[i].priority == TX_PRIORITY_PERIODIC) &&
            ((int8_t)i != s_active_transmitting_idx)) {
            slot_idx = (int8_t)i;
            break;
        }
    }

    if (slot_idx < 0) {
        slot_idx = lorawan_tx_queue_find_free_slot();
        if (slot_idx < 0) {
            return STATUS_ERROR_OUT_OF_BOUNDS;
        }
    }

    lorawan_tx_item_t *item = &s_queue_slots[slot_idx];
    (void)memcpy(item->payload, payload, length);
    item->length        = length;
    item->fport         = LORAWAN_FPORT_PERIODIC;
    item->priority      = TX_PRIORITY_PERIODIC;
    item->is_confirmed  = false;
    item->max_retries   = 1U;
    item->retry_count   = 0U;
    item->completion_cb = completion_cb;
    item->user_ctx      = user_ctx;
    item->in_use        = true;

    return STATUS_OK;
}

status_t lorawan_tx_queue_enqueue_playback(const uint8_t *payload,
                                           uint8_t length,
                                           bool is_confirmed,
                                           lorawan_tx_completion_cb_t completion_cb,
                                           void *user_ctx) {
    if (!s_is_initialized) {
        (void)lorawan_tx_queue_init();
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if ((length == 0U) || (length > LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    int8_t slot_idx = lorawan_tx_queue_find_free_slot();
    if (slot_idx < 0) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    lorawan_tx_item_t *item = &s_queue_slots[slot_idx];
    (void)memcpy(item->payload, payload, length);
    item->length        = length;
    item->fport         = LORAWAN_FPORT_PLAYBACK;
    item->priority      = TX_PRIORITY_PLAYBACK;
    item->is_confirmed  = is_confirmed;
    item->max_retries   = is_confirmed ? LORAWAN_DEFAULT_MAX_RETRIES : 1U;
    item->retry_count   = 0U;
    item->completion_cb = completion_cb;
    item->user_ctx      = user_ctx;
    item->in_use        = true;

    return STATUS_OK;
}

status_t lorawan_tx_queue_enqueue_mac_resp(const uint8_t *payload,
                                           uint8_t length,
                                           lorawan_tx_completion_cb_t completion_cb,
                                           void *user_ctx) {
    if (!s_is_initialized) {
        (void)lorawan_tx_queue_init();
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if ((length == 0U) || (length > LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    int8_t slot_idx = lorawan_tx_queue_find_free_slot();
    if (slot_idx < 0) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    lorawan_tx_item_t *item = &s_queue_slots[slot_idx];
    (void)memcpy(item->payload, payload, length);
    item->length        = length;
    item->fport         = LORAWAN_FPORT_CONFIG;
    item->priority      = TX_PRIORITY_MAC_RESP;
    item->is_confirmed  = false;
    item->max_retries   = 1U;
    item->retry_count   = 0U;
    item->completion_cb = completion_cb;
    item->user_ctx      = user_ctx;
    item->in_use        = true;

    return STATUS_OK;
}

status_t lorawan_tx_queue_peek(uint8_t index, lorawan_tx_item_t *out_item) {
    if (out_item == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (index >= LORAWAN_TX_QUEUE_CAPACITY) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    if (!s_queue_slots[index].in_use) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    (void)memcpy(out_item, &s_queue_slots[index], sizeof(lorawan_tx_item_t));
    return STATUS_OK;
}

status_t lorawan_tx_queue_process_step(void) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    if (s_active_transmitting_idx >= 0) {
        /* Transmission currently in progress by radio layer */
        return STATUS_OK;
    }

    int8_t next_idx = lorawan_tx_queue_find_highest_priority();
    if (next_idx < 0) {
        return STATUS_ERROR_IDLE; /* Queue empty */
    }

    lorawan_tx_item_t *item = &s_queue_slots[next_idx];

    /* Check duty cycle restrictions for non-urgent packets */
    if (item->priority != TX_PRIORITY_URGENT) {
        uint32_t off_time = lorawan_regional_get_off_time_ms(0U);
        if (off_time > 0U) {
            return STATUS_ERROR_BUSY; /* Blocked by duty cycle */
        }
    }

    status_t tx_st;
    if (item->is_confirmed) {
        tx_st = lorawan_send_confirmed(item->fport, item->payload, item->length);
    } else {
        tx_st = lorawan_send_unconfirmed(item->fport, item->payload, item->length);
    }

    if (tx_st == STATUS_OK) {
        s_active_transmitting_idx = next_idx;
        return STATUS_OK;
    }

    return tx_st;
}

status_t lorawan_tx_queue_on_tx_complete(bool success) {
    if (!s_is_initialized || (s_active_transmitting_idx < 0)) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    lorawan_tx_item_t *item = &s_queue_slots[s_active_transmitting_idx];

    if (success || !item->is_confirmed) {
        if (item->completion_cb != NULL) {
            item->completion_cb(item->priority, item->fport, success, item->user_ctx);
        }
        item->in_use = false;
        s_active_transmitting_idx = -1;
    } else {
        /* Confirmed transmission NACK retry logic */
        item->retry_count++;
        if (item->retry_count >= item->max_retries) {
            if (item->completion_cb != NULL) {
                item->completion_cb(item->priority, item->fport, false, item->user_ctx);
            }
            item->in_use = false;
            s_active_transmitting_idx = -1;
        } else {
            /* Release transmitting lock to permit duty backoff retry */
            s_active_transmitting_idx = -1;
        }
    }

    return STATUS_OK;
}

status_t lorawan_tx_queue_get_status(lorawan_tx_queue_status_t *out_status) {
    if (out_status == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    (void)memset(out_status, 0, sizeof(lorawan_tx_queue_status_t));
    for (uint8_t i = 0U; i < LORAWAN_TX_QUEUE_CAPACITY; i++) {
        if (s_queue_slots[i].in_use) {
            out_status->count++;
            switch (s_queue_slots[i].priority) {
                case TX_PRIORITY_URGENT:
                    out_status->urgent_count++;
                    break;
                case TX_PRIORITY_PERIODIC:
                    out_status->periodic_count++;
                    break;
                case TX_PRIORITY_PLAYBACK:
                    out_status->playback_count++;
                    break;
                case TX_PRIORITY_MAC_RESP:
                default:
                    break;
            }
        }
    }
    out_status->is_busy         = (s_active_transmitting_idx >= 0);
    out_status->is_duty_blocked = (lorawan_regional_get_off_time_ms(0U) > 0U);

    return STATUS_OK;
}

status_t lorawan_tx_queue_clear(void) {
    (void)memset(s_queue_slots, 0, sizeof(s_queue_slots));
    s_active_transmitting_idx = -1;
    return STATUS_OK;
}
