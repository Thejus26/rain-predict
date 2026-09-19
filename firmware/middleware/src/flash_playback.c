/**
 * @file    flash_playback.c
 * @brief   LoRaWAN reconnect historical telemetry playback & re-transmission queue implementation.
 */

#include "flash_playback.h"
#include <string.h>

/* ========================================================================== */
/* Static Configuration and Runtime State                                     */
/* ========================================================================== */

static flash_playback_config_t s_config = {
    .max_retries           = FLASH_PLAYBACK_MAX_RETRIES,
    .duty_cycle_gap_ms     = 28000U,
    .require_confirmed_ack = false
};

static flash_playback_state_t s_state = PLAYBACK_STATE_IDLE;
static flash_playback_state_t s_saved_preempt_state = PLAYBACK_STATE_IDLE;

static uint32_t s_total_records_sent  = 0U;
static uint8_t  s_current_retry_count = 0U;
static uint8_t  s_current_batch_k     = 0U;
static bool     s_is_initialized      = false;

/* ========================================================================== */
/* Private Helper Functions                                                   */
/* ========================================================================== */

/**
 * @brief  Calculates maximum record count K that fits within current Data Rate MTU.
 */
static uint8_t flash_playback_calc_max_k(uint8_t dr) {
    uint16_t mtu_bytes;

    switch (dr) {
        case 0: /* DR0 (SF12 / 125 kHz): 51B MTU */
        case 1: /* DR1 (SF11 / 125 kHz): 51B MTU */
        case 2: /* DR2 (SF10 / 125 kHz): 51B MTU */
            mtu_bytes = 51U;
            break;
        case 3: /* DR3 (SF9 / 125 kHz): 115B MTU */
            mtu_bytes = 115U;
            break;
        case 4: /* DR4 (SF8 / 125 kHz): 222B MTU */
        case 5: /* DR5 (SF7 / 125 kHz): 222B MTU */
        default:
            mtu_bytes = 222U;
            break;
    }

    if (mtu_bytes <= FLASH_PLAYBACK_HEADER_SIZE) {
        return 1U;
    }

    uint16_t available_payload = mtu_bytes - FLASH_PLAYBACK_HEADER_SIZE;
    uint16_t max_records = available_payload / FLASH_RING_TELEMETRY_PAYLOAD_SIZE;

    if (max_records > FLASH_PLAYBACK_MAX_RECORDS_PER_BATCH) {
        max_records = FLASH_PLAYBACK_MAX_RECORDS_PER_BATCH;
    }
    if (max_records == 0U) {
        max_records = 1U;
    }

    return (uint8_t)max_records;
}

/* ========================================================================== */
/* Public API Implementation                                                  */
/* ========================================================================== */

status_t flash_playback_init(const flash_playback_config_t *config) {
    if (config != NULL) {
        s_config = *config;
        if (s_config.max_retries == 0U) {
            s_config.max_retries = FLASH_PLAYBACK_MAX_RETRIES;
        }
    } else {
        s_config.max_retries           = FLASH_PLAYBACK_MAX_RETRIES;
        s_config.duty_cycle_gap_ms     = 28000U;
        s_config.require_confirmed_ack = false;
    }

    s_state               = PLAYBACK_STATE_IDLE;
    s_saved_preempt_state = PLAYBACK_STATE_IDLE;
    s_total_records_sent  = 0U;
    s_current_retry_count = 0U;
    s_current_batch_k     = 0U;
    s_is_initialized      = true;

    return STATUS_OK;
}

status_t flash_playback_trigger(void) {
    if (!s_is_initialized) {
        status_t st = flash_playback_init(NULL);
        if (st != STATUS_OK) {
            return st;
        }
    }

    uint32_t pending = flash_ring_get_count();
    if (pending == 0U) {
        s_state = PLAYBACK_STATE_IDLE;
        return STATUS_ERROR_EMPTY;
    }

    s_total_records_sent  = 0U;
    s_current_retry_count = 0U;
    s_current_batch_k     = 0U;
    s_state               = PLAYBACK_STATE_ARMED;

    return STATUS_OK;
}

status_t flash_playback_build_next_batch(uint8_t current_dr, flash_playback_batch_t *out_batch) {
    if (!s_is_initialized) {
        status_t st = flash_playback_init(NULL);
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (out_batch == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    uint32_t pending_total = flash_ring_get_count();
    if (pending_total == 0U) {
        s_state = PLAYBACK_STATE_COMPLETED;
        return STATUS_ERROR_EMPTY;
    }

    uint8_t max_k = flash_playback_calc_max_k(current_dr);
    uint8_t batch_k = (pending_total < (uint32_t)max_k) ? (uint8_t)pending_total : max_k;

    memset(out_batch, 0, sizeof(flash_playback_batch_t));

    /* Peek first record to obtain starting sequence ID */
    flash_record_t first_rec;
    status_t st = flash_ring_peek(0U, &first_rec);
    if (st != STATUS_OK) {
        return st;
    }

    out_batch->record_count = batch_k;
    out_batch->start_seq_id = first_rec.sequence_id;
    out_batch->more_pending = (pending_total > (uint32_t)batch_k);

    /* Byte 0: Control Header [Bit 7: More Pending | Bits 3..0: K] */
    uint8_t ctrl_header = (uint8_t)(batch_k & FLASH_PLAYBACK_MASK_RECORD_COUNT);
    if (out_batch->more_pending) {
        ctrl_header = (uint8_t)(ctrl_header | FLASH_PLAYBACK_FLAG_MORE_PENDING);
    }
    out_batch->payload[0] = ctrl_header;

    /* Bytes 1..2: Starting Sequence ID (Big-Endian) */
    out_batch->payload[1] = (uint8_t)((first_rec.sequence_id >> 8) & 0xFFU);
    out_batch->payload[2] = (uint8_t)(first_rec.sequence_id & 0xFFU);

    /* Bytes 3..: Pack K x 12-byte telemetry records */
    uint16_t write_offset = FLASH_PLAYBACK_HEADER_SIZE;
    for (uint8_t i = 0U; i < batch_k; i++) {
        flash_record_t rec;
        st = flash_ring_peek((uint32_t)i, &rec);
        if (st != STATUS_OK) {
            return st;
        }
        memcpy(&out_batch->payload[write_offset], rec.payload, FLASH_RING_TELEMETRY_PAYLOAD_SIZE);
        write_offset = (uint16_t)(write_offset + FLASH_RING_TELEMETRY_PAYLOAD_SIZE);
    }

    out_batch->length = write_offset;
    s_current_batch_k = batch_k;
    s_state = PLAYBACK_STATE_WAIT_ACK;

    return STATUS_OK;
}

status_t flash_playback_on_tx_result(bool success) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    if (success) {
        /* Advance Flash ring tail by K records */
        if (s_current_batch_k > 0U) {
            status_t st = flash_ring_mark_transmitted((uint32_t)s_current_batch_k);
            if (st != STATUS_OK) {
                return st;
            }

            s_total_records_sent += s_current_batch_k;
            s_current_batch_k = 0U;
        }

        s_current_retry_count = 0U;

        if (flash_ring_get_count() == 0U) {
            s_state = PLAYBACK_STATE_COMPLETED;
        } else {
            s_state = PLAYBACK_STATE_DUTY_WAIT;
        }
    } else {
        /* Transmission failed / NACK */
        s_current_retry_count++;
        if (s_current_retry_count >= s_config.max_retries) {
            s_state = PLAYBACK_STATE_ABORTED;
        } else {
            s_state = PLAYBACK_STATE_NACK_RETRY;
        }
    }

    return STATUS_OK;
}

status_t flash_playback_process_step(void) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    switch (s_state) {
        case PLAYBACK_STATE_IDLE:
        case PLAYBACK_STATE_WAIT_ACK:
        case PLAYBACK_STATE_PREEMPTED:
        case PLAYBACK_STATE_DUTY_WAIT:
            /* Passive waiting states; no automatic transition */
            break;

        case PLAYBACK_STATE_ARMED:
        case PLAYBACK_STATE_ACKNOWLEDGED:
        case PLAYBACK_STATE_NACK_RETRY:
            if (flash_ring_get_count() == 0U) {
                s_state = PLAYBACK_STATE_COMPLETED;
            } else {
                s_state = PLAYBACK_STATE_FETCH_BATCH;
            }
            break;

        case PLAYBACK_STATE_COMPLETED:
        case PLAYBACK_STATE_ABORTED:
            /* Terminal states; retained until flash_playback_reset or new trigger */
            break;

        case PLAYBACK_STATE_FETCH_BATCH:
            /* Awaiting flash_playback_build_next_batch invocation */
            break;

        default:
            s_state = PLAYBACK_STATE_IDLE;
            break;
    }

    return STATUS_OK;
}

status_t flash_playback_preempt(void) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (s_state != PLAYBACK_STATE_PREEMPTED && s_state != PLAYBACK_STATE_IDLE) {
        s_saved_preempt_state = s_state;
        s_state = PLAYBACK_STATE_PREEMPTED;
    }
    return STATUS_OK;
}

status_t flash_playback_resume(void) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (s_state == PLAYBACK_STATE_PREEMPTED) {
        s_state = (s_saved_preempt_state != PLAYBACK_STATE_IDLE) ?
                   s_saved_preempt_state : PLAYBACK_STATE_FETCH_BATCH;
    }
    return STATUS_OK;
}

status_t flash_playback_get_status(flash_playback_status_t *out_status) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (out_status == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    out_status->state               = s_state;
    out_status->total_records_sent  = s_total_records_sent;
    out_status->remaining_records   = flash_ring_get_count();
    out_status->current_retry_count = s_current_retry_count;
    out_status->current_batch_k     = s_current_batch_k;
    out_status->is_active           = (s_state != PLAYBACK_STATE_IDLE &&
                                       s_state != PLAYBACK_STATE_COMPLETED &&
                                       s_state != PLAYBACK_STATE_ABORTED);

    return STATUS_OK;
}

status_t flash_playback_reset(void) {
    s_state               = PLAYBACK_STATE_IDLE;
    s_saved_preempt_state = PLAYBACK_STATE_IDLE;
    s_total_records_sent  = 0U;
    s_current_retry_count = 0U;
    s_current_batch_k     = 0U;
    return STATUS_OK;
}
