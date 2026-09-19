/**
 * @file    measurement_scheduler.h
 * @brief   Adaptive multi-rate measurement scheduler header for tea plantation station.
 * @details Manages 15-min nominal, 5-min storm watch, and 2-min active rain sampling modes
 *          with 30-minute anti-chatter hysteresis and active-time sleep compensation.
 */

#ifndef MEASUREMENT_SCHEDULER_H
#define MEASUREMENT_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"
#include "rain_algo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Interval Constants & Hysteresis Timing                                     */
/* ========================================================================== */

/** @brief Default nominal measurement interval in seconds (15 minutes) */
#define SCHEDULER_INTERVAL_NOMINAL_SEC      900U

/** @brief Accelerated storm watch measurement interval in seconds (5 minutes) */
#define SCHEDULER_INTERVAL_STORM_SEC        300U

/** @brief Active tipping-bucket rainfall measurement interval in seconds (2 minutes) */
#define SCHEDULER_INTERVAL_RAIN_SEC         120U

/** @brief Minimum allowable downlink override interval in seconds (1 minute) */
#define SCHEDULER_INTERVAL_MIN_OVERRIDE_SEC 60U

/** @brief Maximum allowable downlink override interval in seconds (1 hour) */
#define SCHEDULER_INTERVAL_MAX_OVERRIDE_SEC 3600U

/** @brief Anti-chatter calm hold-down window in seconds (30 minutes = 6 * 5-min samples) */
#define SCHEDULER_HOLD_DOWN_DURATION_SEC    1800U

/** @brief Rain calm hold-down window in seconds (10 minutes = 5 * 2-min samples) */
#define SCHEDULER_RAIN_CALM_DURATION_SEC    600U

/** @brief CPI threshold triggering transition to Storm Watch mode (40%) */
#define SCHEDULER_CPI_STORM_TRIGGER_PCT     40U

/** @brief CPI calm threshold permitting release from Storm Watch mode (30%) */
#define SCHEDULER_CPI_CALM_RELEASE_PCT      30U

/** @brief Barometric drop rate threshold triggering Storm Watch (hPa/hr) */
#define SCHEDULER_PRESSURE_DROP_TRIGGER_HPA (-1.0f)

/* ========================================================================== */
/* Enumerations & Type Definitions                                            */
/* ========================================================================== */

/**
 * @brief  Operational measurement sampling modes.
 */
typedef enum {
    SCHEDULER_MODE_NOMINAL = 0,     /**< 15-minute nominal sampling (fair weather) */
    SCHEDULER_MODE_STORM_WATCH,     /**< 5-minute accelerated sampling (storm precursors) */
    SCHEDULER_MODE_ACTIVE_RAIN,     /**< 2-minute rapid sampling (active rainfall) */
    SCHEDULER_MODE_OVERRIDE         /**< Remote downlink custom override interval */
} scheduler_mode_t;

/**
 * @brief  Decision result returned by measurement scheduler evaluation.
 */
typedef struct {
    scheduler_mode_t active_mode;               /**< Selected operational mode */
    uint32_t         target_interval_sec;       /**< Target measurement interval in seconds */
    uint32_t         computed_sleep_sec;        /**< Actual RTC sleep duration (active-time compensated) */
    bool             mode_changed;              /**< True if mode changed in this evaluation step */
    uint32_t         hold_down_remaining_sec;   /**< Remaining hold-down calm timer in seconds */
} scheduler_decision_t;

/**
 * @brief  Configuration parameters for the measurement scheduler.
 */
typedef struct {
    uint32_t nominal_interval_sec;      /**< Nominal interval (default: 900 s) */
    uint32_t storm_interval_sec;        /**< Storm watch interval (default: 300 s) */
    uint32_t rain_interval_sec;         /**< Active rain interval (default: 120 s) */
    uint32_t hold_down_sec;             /**< Calm hold-down window (default: 1800 s) */
    uint8_t  cpi_trigger_pct;           /**< CPI trigger threshold (default: 40%) */
    uint8_t  cpi_calm_pct;              /**< CPI calm release threshold (default: 30%) */
    float    pressure_drop_trigger_hpa; /**< Pressure drop trigger (default: -1.0 hPa/hr) */
} scheduler_config_t;

/**
 * @brief  Diagnostic status structure for the measurement scheduler.
 */
typedef struct {
    scheduler_mode_t current_mode;              /**< Active operational mode */
    uint32_t         current_interval_sec;      /**< Current target interval in seconds */
    uint32_t         last_sleep_duration_sec;   /**< Last configured RTC sleep duration in seconds */
    uint32_t         hold_down_timer_sec;       /**< Active hold-down countdown timer in seconds */
    uint16_t         override_interval_sec;     /**< Configured override interval (0 if disabled) */
    bool             is_override_active;        /**< True if remote override is active */
    uint32_t         total_wakeups_nominal;     /**< Cumulative wakeups in nominal mode */
    uint32_t         total_wakeups_storm;       /**< Cumulative wakeups in storm watch mode */
    uint32_t         total_wakeups_rain;        /**< Cumulative wakeups in active rain mode */
} scheduler_status_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the measurement scheduler with configuration parameters.
 * @param[in] config Pointer to scheduler configuration struct (or NULL for defaults).
 * @return STATUS_OK on success.
 */
status_t measurement_scheduler_init(const scheduler_config_t *config);

/**
 * @brief  Evaluates live nowcast prediction and sensor inputs to decide the next sampling mode.
 * @param[in]  cpi_pct            Composite Precipitation Index (0..100%).
 * @param[in]  pressure_rate_hpa  Barometric gradient in hPa/hour.
 * @param[in]  solar_cloud_alarm  Daylight solar cloud drop alarm flag.
 * @param[in]  active_rain_pulses Number of rain gauge tipping bucket pulses in last interval.
 * @param[in]  active_duration_ms Measured active execution time of current cycle in milliseconds.
 * @param[out] out_decision       Pointer to destination scheduler_decision_t structure.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER.
 */
status_t measurement_scheduler_evaluate(uint8_t cpi_pct,
                                        float pressure_rate_hpa,
                                        bool solar_cloud_alarm,
                                        uint32_t active_rain_pulses,
                                        uint32_t active_duration_ms,
                                        scheduler_decision_t *out_decision);

/**
 * @brief  Applies a remote downlink sampling interval override (FPort 10 Cmd 0x01).
 * @param[in] interval_seconds Desired interval (60 to 3600 seconds; 0 to disable override).
 * @return STATUS_OK on success, or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t measurement_scheduler_set_override_interval(uint16_t interval_seconds);

/**
 * @brief  Clears any active remote downlink interval override, returning to autonomous mode.
 * @return STATUS_OK on success.
 */
status_t measurement_scheduler_clear_override(void);

/**
 * @brief  Retrieves the live diagnostic status of the measurement scheduler.
 * @param[out] out_status Pointer to destination scheduler_status_t structure.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER.
 */
status_t measurement_scheduler_get_status(scheduler_status_t *out_status);

/**
 * @brief  Resets the scheduler state machine and hold-down timers to nominal default.
 * @return STATUS_OK on success.
 */
status_t measurement_scheduler_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_SCHEDULER_H */
