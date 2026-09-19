/**
 * @file    lorawan_tx_queue.h
 * @brief   LoRaWAN multi-tier priority transmission queue manager header.
 * @details Manages prioritized uplink queuing, emergency storm alert preemption,
 *          periodic telemetry deduplication, and duty-cycle throttling.
 */

#ifndef LORAWAN_TX_QUEUE_H
#define LORAWAN_TX_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"
#include "lorawan_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Constants & Queue Sizing                                                   */
/* ========================================================================== */

/** @brief Maximum number of pending transmission items in static priority queue */
#define LORAWAN_TX_QUEUE_CAPACITY           8U

/** @brief Maximum payload buffer size per queue item (matches LoRaWAN MTU) */
#define LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE   LORAWAN_MAX_PAYLOAD_SIZE

/* ========================================================================== */
/* Enumerations & Type Definitions                                            */
/* ========================================================================== */

/**
 * @brief  Priority tiers for LoRaWAN uplink packets.
 */
typedef enum {
    TX_PRIORITY_URGENT = 0,     /**< Tier 0: Immediate storm alert (FPort 2, confirmed, highest) */
    TX_PRIORITY_PERIODIC,       /**< Tier 1: Scheduled periodic telemetry (FPort 1, unconfirmed) */
    TX_PRIORITY_PLAYBACK,       /**< Tier 2: Historical Flash catch-up backlog (FPort 3) */
    TX_PRIORITY_MAC_RESP        /**< Tier 3: Downlink MAC/Config response (FPort 10, lowest) */
} lorawan_tx_priority_t;

/**
 * @brief  Callback function signature invoked when a queued item completes transmission.
 */
typedef void (*lorawan_tx_completion_cb_t)(lorawan_tx_priority_t priority,
                                           uint8_t fport,
                                           bool success,
                                           void *user_ctx);

/**
 * @brief  Structure representing an individual transmission queue item.
 */
typedef struct {
    uint8_t                    payload[LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE]; /**< Data payload */
    uint8_t                    length;              /**< Actual payload length in bytes */
    uint8_t                    fport;               /**< Target LoRaWAN application port */
    lorawan_tx_priority_t      priority;            /**< Packet priority tier */
    bool                       is_confirmed;        /**< True for confirmed uplink */
    uint8_t                    max_retries;         /**< Maximum transmission retries */
    uint8_t                    retry_count;         /**< Current retry counter */
    lorawan_tx_completion_cb_t completion_cb;       /**< Optional completion callback */
    void                      *user_ctx;            /**< User context passed to callback */
    bool                       in_use;              /**< Slot allocation flag */
} lorawan_tx_item_t;

/**
 * @brief  Diagnostic status structure for the transmission queue.
 */
typedef struct {
    uint8_t count;                  /**< Total number of pending items in queue */
    uint8_t urgent_count;           /**< Number of Tier 0 urgent alerts pending */
    uint8_t periodic_count;         /**< Number of Tier 1 periodic packets pending */
    uint8_t playback_count;         /**< Number of Tier 2 playback batches pending */
    bool    is_busy;                /**< True if radio is actively transmitting */
    bool    is_duty_blocked;        /**< True if transmission is delayed by duty cycle */
} lorawan_tx_queue_status_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the LoRaWAN priority transmission queue.
 * @return STATUS_OK on success.
 */
status_t lorawan_tx_queue_init(void);

/**
 * @brief  Enqueues an urgent convective storm alert (Tier 0, FPort 2, confirmed).
 * @details Jumps to head-of-line priority and preempts lower priority transmissions.
 * @param[in] payload       Pointer to 4-byte alert payload buffer.
 * @param[in] length        Payload length (1 to LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE bytes).
 * @param[in] completion_cb Optional callback on transmission completion (or NULL).
 * @param[in] user_ctx      Optional user context pointer.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER, or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_tx_queue_enqueue_alert(const uint8_t *payload,
                                        uint8_t length,
                                        lorawan_tx_completion_cb_t completion_cb,
                                        void *user_ctx);

/**
 * @brief  Enqueues a routine periodic telemetry frame (Tier 1, FPort 1, unconfirmed).
 * @details Automatically deduplicates pending periodic frames by updating stale frame.
 * @param[in] payload       Pointer to periodic payload buffer.
 * @param[in] length        Payload length (1 to LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE bytes).
 * @param[in] completion_cb Optional callback on transmission completion (or NULL).
 * @param[in] user_ctx      Optional user context pointer.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER, or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_tx_queue_enqueue_periodic(const uint8_t *payload,
                                           uint8_t length,
                                           lorawan_tx_completion_cb_t completion_cb,
                                           void *user_ctx);

/**
 * @brief  Enqueues a historical catch-up batch from Flash (Tier 2, FPort 3).
 * @param[in] payload       Pointer to multi-record batch payload buffer.
 * @param[in] length        Payload length (1 to LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE bytes).
 * @param[in] is_confirmed  True if confirmed uplink required.
 * @param[in] completion_cb Optional callback on transmission completion (or NULL).
 * @param[in] user_ctx      Optional user context pointer.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER, or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_tx_queue_enqueue_playback(const uint8_t *payload,
                                           uint8_t length,
                                           bool is_confirmed,
                                           lorawan_tx_completion_cb_t completion_cb,
                                           void *user_ctx);

/**
 * @brief  Enqueues a MAC/Configuration response (Tier 3, FPort 10, unconfirmed).
 * @param[in] payload       Pointer to configuration payload buffer.
 * @param[in] length        Payload length (1 to LORAWAN_TX_QUEUE_MAX_PAYLOAD_SIZE bytes).
 * @param[in] completion_cb Optional callback on transmission completion (or NULL).
 * @param[in] user_ctx      Optional user context pointer.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER, or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_tx_queue_enqueue_mac_resp(const uint8_t *payload,
                                           uint8_t length,
                                           lorawan_tx_completion_cb_t completion_cb,
                                           void *user_ctx);

/**
 * @brief  Non-destructively peeks at an allocated queue slot by index.
 * @param[in]  index    Slot index (0 to LORAWAN_TX_QUEUE_CAPACITY - 1).
 * @param[out] out_item Pointer to destination lorawan_tx_item_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER, or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_tx_queue_peek(uint8_t index, lorawan_tx_item_t *out_item);

/**
 * @brief  Non-blocking periodic process step for the transmission queue.
 * @details Evaluates highest priority item, checks duty-cycle timers, and invokes radio.
 * @return STATUS_OK on active progress, STATUS_ERROR_IDLE if queue is empty,
 *         STATUS_ERROR_BUSY if duty-cycle blocked, or error code from radio layer.
 */
status_t lorawan_tx_queue_process_step(void);

/**
 * @brief  Notifies the queue manager of radio physical transmission result.
 * @param[in] success True if uplink was transmitted and acknowledged (if confirmed).
 * @return STATUS_OK on success, or STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_tx_queue_on_tx_complete(bool success);

/**
 * @brief  Retrieves the diagnostic status of the transmission queue.
 * @param[out] out_status Pointer to destination status structure.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER, or STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_tx_queue_get_status(lorawan_tx_queue_status_t *out_status);

/**
 * @brief  Clears all pending items from the transmission queue.
 * @return STATUS_OK on success.
 */
status_t lorawan_tx_queue_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_TX_QUEUE_H */
