/**
 * @file    flash_playback.h
 * @brief   LoRaWAN reconnect historical telemetry playback & re-transmission queue manager.
 * @details Manages multi-record batch packing on FPort 3, duty-cycle throttling, preemption,
 *          and in-place Flash ring buffer invalidation upon gateway delivery.
 */

#ifndef FLASH_PLAYBACK_H
#define FLASH_PLAYBACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"
#include "flash_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Constants & Configuration Defaults                                         */
/* ========================================================================== */

/** @brief Dedicated LoRaWAN application port for historical playback uplinks */
#define FLASH_PLAYBACK_FPORT                3U

/** @brief Maximum number of 12-byte telemetry records in a single FPort 3 batch frame */
#define FLASH_PLAYBACK_MAX_RECORDS_PER_BATCH 16U

/** @brief FPort 3 frame header length in bytes (1B control header + 2B start SeqID) */
#define FLASH_PLAYBACK_HEADER_SIZE          3U

/** @brief Maximum FPort 3 batch frame payload buffer size (3 + 16 * 12 = 195 bytes) */
#define FLASH_PLAYBACK_MAX_FRAME_SIZE       (FLASH_PLAYBACK_HEADER_SIZE + \
                                             (FLASH_PLAYBACK_MAX_RECORDS_PER_BATCH * FLASH_RING_TELEMETRY_PAYLOAD_SIZE))

/** @brief Maximum consecutive re-transmission attempts before aborting a playback session */
#define FLASH_PLAYBACK_MAX_RETRIES          3U

/** @brief Batch Header Control Flag: More historical records pending in Flash */
#define FLASH_PLAYBACK_FLAG_MORE_PENDING    0x80U

/** @brief Batch Header Control Mask for Record Count K (5 bits: 1..16) */
#define FLASH_PLAYBACK_MASK_RECORD_COUNT    0x1FU

/* ========================================================================== */
/* Enumerations & Type Definitions                                            */
/* ========================================================================== */

/**
 * @brief  State machine states for the historical playback manager.
 */
typedef enum {
    PLAYBACK_STATE_IDLE = 0,        /**< Quiescent state; no active playback session */
    PLAYBACK_STATE_ARMED,           /**< Reconnect event detected; backlog verified */
    PLAYBACK_STATE_FETCH_BATCH,     /**< Extracting records from Flash and assembling frame */
    PLAYBACK_STATE_WAIT_ACK,        /**< Uplink queued in LoRaWAN stack; awaiting ACK/status */
    PLAYBACK_STATE_ACKNOWLEDGED,    /**< Batch confirmed; advancing Flash tail pointer */
    PLAYBACK_STATE_NACK_RETRY,      /**< Uplink failed; preparing backoff retry */
    PLAYBACK_STATE_DUTY_WAIT,       /**< Throttling inter-batch transmission for duty-cycle */
    PLAYBACK_STATE_PREEMPTED,       /**< Suspended to allow live measurement / storm alert */
    PLAYBACK_STATE_COMPLETED,       /**< Backlog fully drained (0 un-transmitted records) */
    PLAYBACK_STATE_ABORTED          /**< Max retries exhausted; session terminated */
} flash_playback_state_t;

/**
 * @brief  Structure representing a formatted FPort 3 batch uplink frame.
 */
typedef struct {
    uint8_t  payload[FLASH_PLAYBACK_MAX_FRAME_SIZE]; /**< Binary batch payload buffer */
    uint16_t length;                                 /**< Actual payload length in bytes */
    uint8_t  record_count;                           /**< Number of records packed (K) */
    uint16_t start_seq_id;                           /**< Sequence ID of first record */
    bool     more_pending;                           /**< True if additional records remain in Flash */
} flash_playback_batch_t;

/**
 * @brief  Configuration parameters for the playback manager.
 */
typedef struct {
    uint8_t  max_retries;           /**< Max NACK retries before aborting (default: 3) */
    uint32_t duty_cycle_gap_ms;     /**< Forced inter-batch delay in ms (default: 28000 ms) */
    bool     require_confirmed_ack; /**< True to request confirmed LoRaWAN uplinks for playback */
} flash_playback_config_t;

/**
 * @brief  Live diagnostic status of the playback engine.
 */
typedef struct {
    flash_playback_state_t state;               /**< Current state machine state */
    uint32_t               total_records_sent;  /**< Cumulative records confirmed in this session */
    uint32_t               remaining_records;   /**< Records still awaiting transmission in Flash */
    uint8_t                current_retry_count; /**< Current retry attempt for active batch */
    uint8_t                current_batch_k;     /**< Number of records in active batch */
    bool                   is_active;           /**< True if playback session is actively running */
} flash_playback_status_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the historical playback manager with configuration parameters.
 * @param[in] config Pointer to configuration struct (or NULL for default settings).
 * @return STATUS_OK on success, or error status code.
 */
status_t flash_playback_init(const flash_playback_config_t *config);

/**
 * @brief  Triggers a historical playback session upon LoRaWAN reconnect.
 * @details Checks Flash ring buffer. If un-transmitted records exist, transitions state
 *          to PLAYBACK_STATE_ARMED; otherwise remains IDLE.
 * @return STATUS_OK on success, STATUS_ERROR_EMPTY if no records pending.
 */
status_t flash_playback_trigger(void);

/**
 * @brief  Executes one non-blocking tick / step of the playback state machine.
 * @details Call periodically from the main application loop or scheduler task.
 * @return STATUS_OK on active progress, STATUS_ERROR_IDLE if no playback active.
 */
status_t flash_playback_process_step(void);

/**
 * @brief  Constructs the next FPort 3 batch frame based on current LoRaWAN Data Rate.
 * @param[in]  current_dr Active LoRaWAN Data Rate (DR0 to DR5).
 * @param[out] out_batch  Pointer to destination flash_playback_batch_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if out_batch is NULL,
 *         or STATUS_ERROR_EMPTY if buffer contains no records.
 */
status_t flash_playback_build_next_batch(uint8_t current_dr, flash_playback_batch_t *out_batch);

/**
 * @brief  Notifies the playback manager of the physical LoRaWAN transmission result.
 * @param[in] success True if uplink was transmitted and acknowledged; false on timeout/error.
 * @return STATUS_OK on success.
 */
status_t flash_playback_on_tx_result(bool success);

/**
 * @brief  Preempts the playback engine to prioritize live telemetry or urgent storm alerts.
 * @details Freezes the playback state and yields the RF radio interface immediately.
 * @return STATUS_OK on success.
 */
status_t flash_playback_preempt(void);

/**
 * @brief  Resumes a previously preempted playback session.
 * @return STATUS_OK on success.
 */
status_t flash_playback_resume(void);

/**
 * @brief  Retrieves the current diagnostic status of the playback engine.
 * @param[out] out_status Pointer to destination flash_playback_status_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if out_status is NULL.
 */
status_t flash_playback_get_status(flash_playback_status_t *out_status);

/**
 * @brief  Resets and aborts any active playback session, returning state to IDLE.
 * @return STATUS_OK on success.
 */
status_t flash_playback_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_PLAYBACK_H */
