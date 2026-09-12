/**
 * @file    ring_buffer.h
 * @brief   Generic static circular FIFO ring buffer interface.
 * @details Zero dynamic memory allocation; supports arbitrary fixed-size items.
 *          Targeted for STM32WLE5 Cortex-M4 and host unit test environments.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

/**
 * @brief Full-buffer insertion policy.
 */
typedef enum {
    RING_BUFFER_DROP_NEW = 0,       /**< Reject push when buffer is full (returns STATUS_ERR_BUSY) */
    RING_BUFFER_OVERWRITE_OLD = 1   /**< Evict oldest item and advance tail when buffer is full */
} ring_buffer_mode_t;

/**
 * @brief Ring buffer control structure.
 */
typedef struct {
    uint8_t            *p_storage;  /**< Pointer to caller-allocated static byte array */
    uint16_t            capacity;   /**< Maximum number of items */
    uint16_t            item_size;  /**< Size of each item in bytes */
    uint16_t            head;       /**< Write index */
    uint16_t            tail;       /**< Read index */
    uint16_t            count;      /**< Current number of stored items */
    ring_buffer_mode_t  mode;       /**< Overwrite mode */
} ring_buffer_t;

/**
 * @brief  Initializes a circular ring buffer with static storage.
 * @param[out] p_rb      Pointer to ring buffer control block.
 * @param[in]  p_buffer  Pointer to pre-allocated static byte array.
 * @param[in]  capacity  Maximum number of items buffer can hold (must be > 0).
 * @param[in]  item_size Size in bytes of each item (must be > 0).
 * @param[in]  mode      Full buffer policy (DROP_NEW or OVERWRITE_OLD).
 * @return status_t      STATUS_OK on success,
 *                       STATUS_ERR_NULL_PTR if p_rb or p_buffer is NULL,
 *                       STATUS_ERR_INVALID_PARAM if capacity == 0, item_size == 0, or mode invalid.
 */
status_t ring_buffer_init(ring_buffer_t *p_rb,
                          void *p_buffer,
                          uint16_t capacity,
                          uint16_t item_size,
                          ring_buffer_mode_t mode);

/**
 * @brief  Pushes an item into the ring buffer.
 * @param[in,out] p_rb   Pointer to ring buffer control block.
 * @param[in]     p_item Pointer to item data to copy into buffer.
 * @return status_t      STATUS_OK on success,
 *                       STATUS_ERR_NULL_PTR if p_rb or p_item is NULL,
 *                       STATUS_ERR_NOT_INITIALIZED if buffer storage is NULL,
 *                       STATUS_ERR_BUSY if full and mode is RING_BUFFER_DROP_NEW.
 */
status_t ring_buffer_push(ring_buffer_t *p_rb, const void *p_item);

/**
 * @brief  Pops the oldest item from the ring buffer.
 * @param[in,out] p_rb   Pointer to ring buffer control block.
 * @param[out]    p_item Pointer to memory buffer where popped item will be copied.
 * @return status_t      STATUS_OK on success,
 *                       STATUS_ERR_NULL_PTR if p_rb or p_item is NULL,
 *                       STATUS_ERR_NOT_INITIALIZED if buffer storage is NULL,
 *                       STATUS_ERR_BUSY if buffer is empty.
 */
status_t ring_buffer_pop(ring_buffer_t *p_rb, void *p_item);

/**
 * @brief  Peeks at the oldest item without removing it.
 * @param[in]  p_rb      Pointer to ring buffer control block.
 * @param[out] p_item    Pointer to buffer where peeked item will be copied.
 * @return status_t      STATUS_OK on success,
 *                       STATUS_ERR_NULL_PTR if p_rb or p_item is NULL,
 *                       STATUS_ERR_NOT_INITIALIZED if buffer storage is NULL,
 *                       STATUS_ERR_BUSY if buffer is empty.
 */
status_t ring_buffer_peek(const ring_buffer_t *p_rb, void *p_item);

/**
 * @brief  Peeks at an item at a logical offset from oldest (0 = oldest, count-1 = newest).
 * @param[in]  p_rb      Pointer to ring buffer control block.
 * @param[in]  index     Logical index (0 to count - 1).
 * @param[out] p_item    Pointer to buffer where peeked item will be copied.
 * @return status_t      STATUS_OK on success,
 *                       STATUS_ERR_NULL_PTR if p_rb or p_item is NULL,
 *                       STATUS_ERR_NOT_INITIALIZED if buffer storage is NULL,
 *                       STATUS_ERR_INVALID_PARAM if index >= count.
 */
status_t ring_buffer_peek_at(const ring_buffer_t *p_rb, uint16_t index, void *p_item);

/**
 * @brief  Checks if ring buffer is empty.
 * @param[in] p_rb Pointer to ring buffer control block.
 * @return bool    true if empty or p_rb is NULL, false otherwise.
 */
bool ring_buffer_is_empty(const ring_buffer_t *p_rb);

/**
 * @brief  Checks if ring buffer is full.
 * @param[in] p_rb Pointer to ring buffer control block.
 * @return bool    true if full, false otherwise (or false if p_rb is NULL).
 */
bool ring_buffer_is_full(const ring_buffer_t *p_rb);

/**
 * @brief  Returns number of elements currently stored.
 * @param[in] p_rb Pointer to ring buffer control block.
 * @return uint16_t Active item count (0 if p_rb is NULL).
 */
uint16_t ring_buffer_get_count(const ring_buffer_t *p_rb);

/**
 * @brief  Returns maximum item capacity.
 * @param[in] p_rb Pointer to ring buffer control block.
 * @return uint16_t Maximum capacity (0 if p_rb is NULL).
 */
uint16_t ring_buffer_get_capacity(const ring_buffer_t *p_rb);

/**
 * @brief  Flushes all items from the ring buffer.
 * @param[in,out] p_rb Pointer to ring buffer control block.
 */
void ring_buffer_clear(ring_buffer_t *p_rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
