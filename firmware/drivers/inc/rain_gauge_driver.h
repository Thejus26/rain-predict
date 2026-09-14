/**
 * @file    rain_gauge_driver.h
 * @brief   Tipping-bucket rain gauge pulse counter driver header for STM32WLE5 SoC.
 * @details Manages EXTI0 GPIO falling-edge interrupts and 50ms software debounce filtering.
 *          Adheres to C99 standards, MISRA-C guidelines, and zero-dynamic-memory allocation.
 */

#ifndef RAIN_GAUGE_DRIVER_H
#define RAIN_GAUGE_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Hardware & Timing Definitions                                              */
/* ========================================================================== */

/** @brief Minimum allowable millisecond interval between consecutive valid bucket tips */
#define RAIN_GAUGE_DEBOUNCE_MS              50U

/** @brief Calibration constant: equivalent rainfall depth per bucket tip in mm */
#define RAIN_GAUGE_CALIB_MM_PER_TIP         0.20f

/** @brief NVIC Interrupt Preemption Priority for Rain Gauge EXTI0 */
#define RAIN_GAUGE_NVIC_PREEMPT_PRIO        2U
#define RAIN_GAUGE_NVIC_SUB_PRIO            0U

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes PA0 GPIO pin in falling-edge interrupt mode and enables EXTI0 NVIC.
 * @return STATUS_OK on success, or STATUS_ERR_HARDWARE on GPIO/NVIC failure.
 */
status_t rain_gauge_init(void);

/**
 * @brief  De-initializes rain gauge driver, disables NVIC interrupt, and tri-states pin.
 * @return STATUS_OK on success.
 */
status_t rain_gauge_deinit(void);

/**
 * @brief  Primary Interrupt Service Routine (ISR) handler for EXTI Line 0 events.
 * @param  current_tick_ms Current millisecond system tick counter.
 */
void rain_gauge_exti_isr(uint32_t current_tick_ms);

/**
 * @brief  Enables or masks the EXTI0 interrupt in the NVIC.
 * @param  enable True to enable interrupt, false to mask.
 * @return STATUS_OK on success, or STATUS_ERR_NOT_INITIALIZED if driver is uninitialized.
 */
status_t rain_gauge_enable_irq(bool enable);

/**
 * @brief  Returns true if physical rain has been registered since last state evaluation.
 * @return True if bucket tip occurred.
 */
bool rain_gauge_is_rain_active(void);

/**
 * @brief  Returns the system tick timestamp (ms) of the last valid bucket tip.
 * @return Millisecond tick value.
 */
uint32_t rain_gauge_get_last_pulse_timestamp(void);

/**
 * @brief  Returns the total number of rejected mechanical contact chatter pulses.
 * @return Cumulative chatter bounce count.
 */
uint32_t rain_gauge_get_rejected_bounce_count(void);

/**
 * @brief  Resets diagnostic counters and active rain indicators.
 */
void rain_gauge_reset_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* RAIN_GAUGE_DRIVER_H */
