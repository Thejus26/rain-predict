/**
 * @file    rain_gauge_driver.h
 * @brief   Tipping-bucket rain gauge pulse counter driver header for STM32WLE5 SoC.
 * @details Manages EXTI0 GPIO falling-edge interrupts, 50ms software debounce filtering,
 *          multi-horizon atomic accumulation registers, instantaneous and interval rain rate
 *          math, peak tracking, meteorological classification, and LoRaWAN Byte 8 telemetry.
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
/* Accumulator & Telemetry Constants                                          */
/* ========================================================================== */

/** @brief Number of 10-minute intervals in a rolling 1-hour FIFO (6 * 10 min = 60 min) */
#define RAIN_GAUGE_HOURLY_FIFO_SIZE         6U

/** @brief Default sample interval duration in seconds (10 minutes) */
#define RAIN_GAUGE_INTERVAL_DURATION_SEC    600U

/** @brief Maximum tip count representable in 8-bit unsigned LoRaWAN Byte 8 (51.0 mm) */
#define RAIN_GAUGE_TELEMETRY_MAX_TIPS       255U

/* ========================================================================== */
/* Rain Rate Constants                                                        */
/* ========================================================================== */

#define RAIN_RATE_NUMERATOR_MS              720000.0f   /**< 0.20 mm * 3,600,000 ms/hr */
#define RAIN_RATE_NUMERATOR_SEC             720.0f      /**< 0.20 mm * 3,600 s/hr */
#define RAIN_RATE_MAX_MM_HR                 300.0f      /**< Terrestrial physical clamping limit */
#define RAIN_RATE_INACTIVITY_TIMEOUT_MS     900000U     /**< 15 min decay to 0.0 mm/hr */
#define RAIN_RATE_ACCELERATION_THRESH_MM_HR 5.0f        /**< Threshold to trigger 2-min tracking */

/* Intensity Boundaries (mm/hr) */
#define RAIN_THRESH_DRIZZLE_MIN_MM_HR       0.001f
#define RAIN_THRESH_LIGHT_MIN_MM_HR         2.5f
#define RAIN_THRESH_MODERATE_MIN_MM_HR      7.5f
#define RAIN_THRESH_HEAVY_MIN_MM_HR         15.0f
#define RAIN_THRESH_TORRENTIAL_MIN_MM_HR    30.0f
#define RAIN_THRESH_CLOUDBURST_MIN_MM_HR    100.0f

/* ========================================================================== */
/* Data Types & Enums                                                         */
/* ========================================================================== */

/**
 * @brief Meteorological precipitation intensity classification tiers (IMD/WMO standard).
 */
typedef enum {
    RAIN_INTENSITY_NONE = 0,    /**< 0.0 mm/hr: No precipitation detected */
    RAIN_INTENSITY_DRIZZLE,     /**< >0.0 to <2.5 mm/hr: Very light rain / mist / drizzle */
    RAIN_INTENSITY_LIGHT,       /**< 2.5 to <7.5 mm/hr: Light precipitation */
    RAIN_INTENSITY_MODERATE,    /**< 7.5 to <15.0 mm/hr: Moderate steady rain */
    RAIN_INTENSITY_HEAVY,       /**< 15.0 to <30.0 mm/hr: Heavy downpour */
    RAIN_INTENSITY_TORRENTIAL,  /**< 30.0 to <100.0 mm/hr: Torrential tropical rain */
    RAIN_INTENSITY_CLOUDBURST   /**< >=100.0 mm/hr: Violent convective cloudburst */
} rain_intensity_t;

/**
 * @brief Comprehensive rainfall accumulation telemetry structure.
 */
typedef struct {
    uint16_t    interval_tips;          /**< Tips recorded during current sample interval */
    float       interval_rain_mm;       /**< Rainfall in mm during current interval */
    uint32_t    hourly_tips;            /**< Cumulative tips over past 60 minutes */
    float       hourly_rain_mm;         /**< Rainfall in mm over past 60 minutes */
    uint32_t    daily_tips;             /**< Cumulative tips over past 24 hours */
    float       daily_rain_mm;          /**< Rainfall in mm over past 24 hours */
    uint32_t    total_lifetime_tips;    /**< Monotonic total lifetime tip count */
    uint8_t     telemetry_byte8;        /**< Formatted 8-bit unsigned integer for LoRaWAN Byte 8 */
} rain_gauge_data_t;

/**
 * @brief Comprehensive real-time rain rate metrics and intensity tracking structure.
 */
typedef struct {
    float               instantaneous_rate_mm_hr;   /**< Current inter-tip derivative rate (decayed) */
    float               interval_rate_mm_hr;        /**< Rain rate over current sample interval */
    float               hourly_rate_mm_hr;          /**< Rolling 60-minute rain rate */
    float               peak_instantaneous_mm_hr;   /**< Peak instantaneous rate recorded */
    float               peak_interval_mm_hr;        /**< Peak interval rate recorded */
    rain_intensity_t    current_intensity;          /**< Meteorological intensity tier */
    uint32_t            last_tip_tick_ms;           /**< System tick (ms) of most recent tip */
    uint32_t            inter_tip_delta_ms;         /**< Measured time delta between last two tips */
} rain_rate_metrics_t;

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

/**
 * @brief  Thread-safe atomic read-and-clear of interval pulse counter.
 * @param[out] p_interval_mm Optional pointer to store computed interval rainfall in mm (may be NULL).
 * @return Number of tips registered during the interval.
 */
uint16_t rain_gauge_read_and_clear_interval(float *p_interval_mm);

/**
 * @brief  Retrieves complete multi-horizon accumulation telemetry.
 * @param[out] p_data Pointer to rain_gauge_data_t structure to populate.
 * @return STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t rain_gauge_get_accumulation(rain_gauge_data_t *p_data);

/**
 * @brief  Pushes interval tips into rolling 1-hour FIFO ring buffer.
 * @param  interval_tips Tips recorded during just-completed interval.
 */
void rain_gauge_update_hourly_history(uint16_t interval_tips);

/**
 * @brief  Resets the 24-hour daily accumulation counter (midnight RTC rollover).
 */
void rain_gauge_reset_daily(void);

/**
 * @brief  Formats and clamps interval tips into 8-bit LoRaWAN Byte 8 format.
 * @param  interval_tips Tips recorded during interval.
 * @return Formatted uint8_t byte (0 to 255).
 */
uint8_t rain_gauge_encode_telemetry_byte(uint16_t interval_tips);

/**
 * @brief  Resets all accumulation registers (interval, hourly, daily, lifetime).
 */
void rain_gauge_reset_all_accumulators(void);

/**
 * @brief  Computes instantaneous rain rate with time-decay aging.
 * @param  current_tick_ms Current system timestamp in milliseconds.
 * @return Decayed instantaneous rain rate in mm/hr (clamped to 0.0 - 300.0 mm/hr).
 */
float rain_gauge_get_instantaneous_rate(uint32_t current_tick_ms);

/**
 * @brief  Calculates rain rate for a discrete sample interval.
 * @param  interval_tips Total tips recorded during interval.
 * @param  interval_duration_sec Duration of the interval in seconds (e.g. 600 s).
 * @return Calculated interval rain rate in mm/hr.
 */
float rain_gauge_compute_interval_rate(uint16_t interval_tips, uint32_t interval_duration_sec);

/**
 * @brief  Retrieves full snapshot of rain rate metrics and intensity tier.
 * @param  current_tick_ms Current system timestamp in milliseconds.
 * @param  interval_duration_sec Duration of the active interval in seconds.
 * @param[out] p_metrics Pointer to rain_rate_metrics_t structure to populate.
 * @return STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t rain_gauge_get_rate_metrics(uint32_t current_tick_ms,
                                     uint32_t interval_duration_sec,
                                     rain_rate_metrics_t *p_metrics);

/**
 * @brief  Classifies a rain rate into meteorological intensity tier.
 * @param  rain_rate_mm_hr Rain rate in mm/hr.
 * @return Corresponding rain_intensity_t enum.
 */
rain_intensity_t rain_gauge_classify_intensity(float rain_rate_mm_hr);

/**
 * @brief  Converts rain_intensity_t enum to human-readable string.
 * @param  intensity Intensity tier enum.
 * @return Constant pointer to descriptive string.
 */
const char *rain_gauge_intensity_to_str(rain_intensity_t intensity);

/**
 * @brief  Evaluates whether current rain rate requires accelerating duty cycle.
 * @param  rain_rate_mm_hr Rain rate in mm/hr.
 * @return true if rate >= 5.0 mm/hr, false otherwise.
 */
bool rain_gauge_should_accelerate_sampling(float rain_rate_mm_hr);

/**
 * @brief  Resets peak instantaneous and interval rate registers to zero.
 */
void rain_gauge_reset_peak_rates(void);

#ifdef __cplusplus
}
#endif

#endif /* RAIN_GAUGE_DRIVER_H */
