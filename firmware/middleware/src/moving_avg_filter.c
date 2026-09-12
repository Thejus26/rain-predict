/**
 * @file    moving_avg_filter.c
 * @brief   Implementation of single-precision floating-point moving average filter.
 * @details Conforms to MISRA-C memory safety rules and zero-dynamic allocation.
 */

#include "moving_avg_filter.h"
#include <math.h>
#include <string.h>

status_t moving_avg_init(moving_avg_filter_t *p_filter,
                         float *p_storage,
                         uint16_t window_size,
                         bool enable_outlier_filter,
                         float outlier_threshold) {
    if (p_filter == NULL || p_storage == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (window_size == 0U) {
        return STATUS_ERR_INVALID_PARAM;
    }

    p_filter->p_buffer              = p_storage;
    p_filter->window_size           = window_size;
    p_filter->head                  = 0U;
    p_filter->count                 = 0U;
    p_filter->running_sum           = 0.0f;
    p_filter->enable_outlier_filter = enable_outlier_filter;
    p_filter->outlier_threshold     = fabsf(outlier_threshold);

    (void)memset(p_storage, 0, (size_t)window_size * sizeof(float));

    return STATUS_OK;
}

status_t moving_avg_update(moving_avg_filter_t *p_filter,
                           float new_sample,
                           float *p_filtered_output) {
    if (p_filter == NULL || p_filtered_output == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (p_filter->p_buffer == NULL || p_filter->window_size == 0U) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    float sample_to_insert = new_sample;

    /* Outlier Suppression Check: active only once filter has at least 3 baseline samples */
    if (p_filter->enable_outlier_filter && p_filter->count >= 3U) {
        float current_avg = p_filter->running_sum / (float)p_filter->count;
        float delta = fabsf(new_sample - current_avg);

        if (delta > p_filter->outlier_threshold) {
            /* Clamp extreme spike to threshold boundary */
            if (new_sample > current_avg) {
                sample_to_insert = current_avg + p_filter->outlier_threshold;
            } else {
                sample_to_insert = current_avg - p_filter->outlier_threshold;
            }
        }
    }

    /* O(1) Running Sum & Circular Buffer Update */
    if (p_filter->count < p_filter->window_size) {
        p_filter->running_sum += sample_to_insert;
        p_filter->p_buffer[p_filter->head] = sample_to_insert;
        p_filter->head = (uint16_t)(((uint32_t)p_filter->head + 1U) % (uint32_t)p_filter->window_size);
        p_filter->count++;
    } else {
        /* Buffer is full: subtract oldest sample before adding new */
        float oldest_sample = p_filter->p_buffer[p_filter->head];
        p_filter->running_sum = (p_filter->running_sum - oldest_sample) + sample_to_insert;
        p_filter->p_buffer[p_filter->head] = sample_to_insert;
        p_filter->head = (uint16_t)(((uint32_t)p_filter->head + 1U) % (uint32_t)p_filter->window_size);
    }

    /* Guard against floating-point accumulation drift near zero */
    if (p_filter->count == 0U || fabsf(p_filter->running_sum) < 1e-7f) {
        p_filter->running_sum = 0.0f;
    }

    *p_filtered_output = p_filter->running_sum / (float)p_filter->count;
    return STATUS_OK;
}

status_t moving_avg_get_average(const moving_avg_filter_t *p_filter, float *p_average) {
    if (p_filter == NULL || p_average == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (p_filter->p_buffer == NULL || p_filter->window_size == 0U) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    if (p_filter->count == 0U) {
        return STATUS_ERR_BUSY;
    }

    *p_average = p_filter->running_sum / (float)p_filter->count;
    return STATUS_OK;
}

bool moving_avg_is_primed(const moving_avg_filter_t *p_filter) {
    if (p_filter == NULL || p_filter->p_buffer == NULL || p_filter->window_size == 0U) {
        return false;
    }

    return (p_filter->count >= p_filter->window_size);
}

uint16_t moving_avg_get_count(const moving_avg_filter_t *p_filter) {
    if (p_filter == NULL) {
        return 0U;
    }

    return p_filter->count;
}

void moving_avg_reset(moving_avg_filter_t *p_filter) {
    if (p_filter != NULL) {
        p_filter->head        = 0U;
        p_filter->count       = 0U;
        p_filter->running_sum = 0.0f;

        if (p_filter->p_buffer != NULL && p_filter->window_size > 0U) {
            (void)memset(p_filter->p_buffer, 0, (size_t)p_filter->window_size * sizeof(float));
        }
    }
}
