/**
 * @file    moving_avg_filter.h
 * @brief   Single-precision floating-point moving average and outlier filter.
 * @details O(1) running sum execution with zero dynamic memory allocation.
 *          Targeted for STM32WLE5 Cortex-M4 and host unit test environments.
 */

#ifndef MOVING_AVG_FILTER_H
#define MOVING_AVG_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

/**
 * @brief Moving average filter control structure.
 */
typedef struct {
    float      *p_buffer;               /**< Pointer to caller-allocated static float buffer */
    uint16_t    window_size;            /**< Window capacity (3, 4, 6, 12 samples) */
    uint16_t    head;                   /**< Next write index in circular buffer */
    uint16_t    count;                  /**< Active sample count in buffer */
    float       running_sum;            /**< O(1) Running sum accumulator */
    bool        enable_outlier_filter;  /**< Outlier suppression toggle */
    float       outlier_threshold;      /**< Maximum allowable jump delta from current average */
} moving_avg_filter_t;

/**
 * @brief  Initializes a moving average filter.
 * @param[out] p_filter              Pointer to filter control block.
 * @param[in]  p_storage             Pointer to caller-allocated float array (length >= window_size).
 * @param[in]  window_size           Number of samples in the moving window (must be > 0).
 * @param[in]  enable_outlier_filter Enable spike suppression.
 * @param[in]  outlier_threshold     Maximum jump allowed before clamping.
 * @return status_t                  STATUS_OK on success,
 *                                   STATUS_ERR_NULL_PTR if p_filter or p_storage is NULL,
 *                                   STATUS_ERR_INVALID_PARAM if window_size == 0.
 */
status_t moving_avg_init(moving_avg_filter_t *p_filter,
                         float *p_storage,
                         uint16_t window_size,
                         bool enable_outlier_filter,
                         float outlier_threshold);

/**
 * @brief  Pushes a new sample into the filter and computes updated moving average in O(1).
 * @param[in,out] p_filter          Pointer to filter control block.
 * @param[in]     new_sample        Raw incoming measurement.
 * @param[out]    p_filtered_output Pointer to float where filtered average will be stored.
 * @return status_t                 STATUS_OK on success,
 *                                   STATUS_ERR_NULL_PTR if p_filter or p_filtered_output is NULL,
 *                                   STATUS_ERR_NOT_INITIALIZED if buffer storage is NULL.
 */
status_t moving_avg_update(moving_avg_filter_t *p_filter,
                           float new_sample,
                           float *p_filtered_output);

/**
 * @brief  Retrieves current moving average without modifying state.
 * @param[in]  p_filter             Pointer to filter control block.
 * @param[out] p_average            Pointer to float where average will be stored.
 * @return status_t                 STATUS_OK on success,
 *                                   STATUS_ERR_NULL_PTR if p_filter or p_average is NULL,
 *                                   STATUS_ERR_NOT_INITIALIZED if buffer storage is NULL,
 *                                   STATUS_ERR_BUSY if filter is empty (count == 0).
 */
status_t moving_avg_get_average(const moving_avg_filter_t *p_filter, float *p_average);

/**
 * @brief  Checks if filter has received a full window of samples.
 * @param[in] p_filter              Pointer to filter control block.
 * @return bool                     true if count >= window_size, false otherwise.
 */
bool moving_avg_is_primed(const moving_avg_filter_t *p_filter);

/**
 * @brief  Returns active sample count in the filter.
 * @param[in] p_filter              Pointer to filter control block.
 * @return uint16_t                 Active sample count (0 if p_filter is NULL).
 */
uint16_t moving_avg_get_count(const moving_avg_filter_t *p_filter);

/**
 * @brief  Resets filter state, clearing accumulator, count, and buffer.
 * @param[in,out] p_filter          Pointer to filter control block.
 */
void moving_avg_reset(moving_avg_filter_t *p_filter);

#ifdef __cplusplus
}
#endif

#endif /* MOVING_AVG_FILTER_H */
