/**
 * @file    ring_buffer.c
 * @brief   Generic static circular FIFO ring buffer implementation.
 * @details Conforms to MISRA-C memory safety rules and zero-dynamic allocation.
 */

#include "ring_buffer.h"
#include <string.h>

status_t ring_buffer_init(ring_buffer_t *p_rb,
                          void *p_buffer,
                          uint16_t capacity,
                          uint16_t item_size,
                          ring_buffer_mode_t mode) {
    if (p_rb == NULL || p_buffer == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (capacity == 0U || item_size == 0U) {
        return STATUS_ERR_INVALID_PARAM;
    }

    if (mode != RING_BUFFER_DROP_NEW && mode != RING_BUFFER_OVERWRITE_OLD) {
        return STATUS_ERR_INVALID_PARAM;
    }

    p_rb->p_storage = (uint8_t *)p_buffer;
    p_rb->capacity  = capacity;
    p_rb->item_size = item_size;
    p_rb->head      = 0U;
    p_rb->tail      = 0U;
    p_rb->count     = 0U;
    p_rb->mode      = mode;

    return STATUS_OK;
}

status_t ring_buffer_push(ring_buffer_t *p_rb, const void *p_item) {
    if (p_rb == NULL || p_item == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (p_rb->p_storage == NULL || p_rb->capacity == 0U || p_rb->item_size == 0U) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    if (p_rb->count >= p_rb->capacity) {
        if (p_rb->mode == RING_BUFFER_DROP_NEW) {
            return STATUS_ERR_BUSY;
        }

        /* Overwrite oldest element: advance tail pointer */
        p_rb->tail = (uint16_t)(((uint32_t)p_rb->tail + 1U) % (uint32_t)p_rb->capacity);
    } else {
        p_rb->count++;
    }

    /* Copy item data to head slot */
    uint8_t *p_dest = &p_rb->p_storage[(size_t)p_rb->head * (size_t)p_rb->item_size];
    (void)memcpy(p_dest, p_item, (size_t)p_rb->item_size);

    /* Advance head pointer */
    p_rb->head = (uint16_t)(((uint32_t)p_rb->head + 1U) % (uint32_t)p_rb->capacity);

    return STATUS_OK;
}

status_t ring_buffer_pop(ring_buffer_t *p_rb, void *p_item) {
    if (p_rb == NULL || p_item == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (p_rb->p_storage == NULL || p_rb->capacity == 0U || p_rb->item_size == 0U) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    if (p_rb->count == 0U) {
        return STATUS_ERR_BUSY;
    }

    /* Copy item data from tail slot */
    const uint8_t *p_src = &p_rb->p_storage[(size_t)p_rb->tail * (size_t)p_rb->item_size];
    (void)memcpy(p_item, p_src, (size_t)p_rb->item_size);

    /* Advance tail pointer and decrement active count */
    p_rb->tail = (uint16_t)(((uint32_t)p_rb->tail + 1U) % (uint32_t)p_rb->capacity);
    p_rb->count--;

    return STATUS_OK;
}

status_t ring_buffer_peek(const ring_buffer_t *p_rb, void *p_item) {
    if (p_rb == NULL || p_item == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (p_rb->p_storage == NULL || p_rb->capacity == 0U || p_rb->item_size == 0U) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    if (p_rb->count == 0U) {
        return STATUS_ERR_BUSY;
    }

    return ring_buffer_peek_at(p_rb, 0U, p_item);
}

status_t ring_buffer_peek_at(const ring_buffer_t *p_rb, uint16_t index, void *p_item) {
    if (p_rb == NULL || p_item == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (p_rb->p_storage == NULL || p_rb->capacity == 0U || p_rb->item_size == 0U) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    if (index >= p_rb->count) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* Calculate physical slot index from logical offset (0 = oldest / tail) */
    uint16_t physical_slot =
        (uint16_t)(((uint32_t)p_rb->tail + (uint32_t)index) % (uint32_t)p_rb->capacity);
    const uint8_t *p_src = &p_rb->p_storage[(size_t)physical_slot * (size_t)p_rb->item_size];
    (void)memcpy(p_item, p_src, (size_t)p_rb->item_size);

    return STATUS_OK;
}

bool ring_buffer_is_empty(const ring_buffer_t *p_rb) {
    if (p_rb == NULL) {
        return true;
    }
    return (p_rb->count == 0U);
}

bool ring_buffer_is_full(const ring_buffer_t *p_rb) {
    if (p_rb == NULL) {
        return false;
    }
    return (p_rb->count >= p_rb->capacity);
}

uint16_t ring_buffer_get_count(const ring_buffer_t *p_rb) {
    if (p_rb == NULL) {
        return 0U;
    }
    return p_rb->count;
}

uint16_t ring_buffer_get_capacity(const ring_buffer_t *p_rb) {
    if (p_rb == NULL) {
        return 0U;
    }
    return p_rb->capacity;
}

void ring_buffer_clear(ring_buffer_t *p_rb) {
    if (p_rb != NULL) {
        p_rb->head  = 0U;
        p_rb->tail  = 0U;
        p_rb->count = 0U;
    }
}
