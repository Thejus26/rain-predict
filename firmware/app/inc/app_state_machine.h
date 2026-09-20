/**
 * @file    app_state_machine.h
 * @brief   Top-level application state machine and operational lifecycle coordinator.
 * @details Implements the 8-state cyclic sequence: WAKE -> POWER_ON -> SAMPLE ->
 *          FILTER -> PREDICT -> TRANSMIT -> ALERT -> SLEEP for STM32WLE5 SoC.
 */

#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"
#include "rain_algo.h"
#include "measurement_scheduler.h"
#include "alert_manager.h"
#include "power_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Enumerations & Constants                                                   */
/* ========================================================================== */

/**
 * @brief  The 8 discrete states of the edge weather station lifecycle.
 */
typedef enum {
    STATE_WAKE = 0,         /**< 0: Clock restore, wake reason identification */
    STATE_POWER_ON,         /**< 1: Switched sensor power rails enable & 20ms delay */
    STATE_SAMPLE,           /**< 2: BME280, OPT3001, Rain Gauge, ADC sensor read */
    STATE_FILTER,           /**< 3: Ring buffer push, dew point, gradient math */
    STATE_PREDICT,          /**< 4: Zambretti, CPI nowcast & alert state evaluation */
    STATE_TRANSMIT,         /**< 5: Telemetry bit-pack, Flash log, LoRaWAN uplink */
    STATE_ALERT,            /**< 6: Local status LEDs, buzzer, siren dispatch */
    STATE_SLEEP,            /**< 7: Interval math, rail shutdown, Stop 2 entry */
    STATE_MAX
} app_state_t;

/**
 * @brief  Master system operational context structure.
 */
typedef struct {
    /* Lifecycle & State */
    app_state_t             current_state;          /**< Active operational state */
    app_state_t             previous_state;         /**< Previous operational state */
    uint32_t                cycle_count;            /**< Cumulative completed cycles */
    uint32_t                active_start_tick_ms;   /**< System tick at cycle start */
    uint32_t                active_duration_ms;     /**< Duration of current active cycle */
    power_wake_reason_t     wake_reason;            /**< Wakeup source (RTC / EXTI) */

    /* Meteorological Raw Acquisition */
    float                   temperature_c;          /**< Ambient temperature (°C) */
    float                   humidity_pct;           /**< Relative humidity (%RH) */
    float                   pressure_hpa;           /**< Station barometric pressure (hPa) */
    float                   solar_lux;              /**< Ambient illuminance (Lux) */
    uint32_t                rain_pulses_cycle;      /**< Bucket tips in this cycle */
    float                   battery_volts;          /**< Battery cell voltage (V) */
    bool                    sensor_fault;           /**< True if any core sensor faulted */

    /* Derived Metrics & Forecast */
    float                   dew_point_c;            /**< Calculated dew point (°C) */
    float                   dpd_c;                  /**< Dew point depression (°C) */
    float                   delta_p_1h_hpa;         /**< 1-hour pressure gradient (hPa/hr) */
    float                   cpi_pct;                /**< Composite Precipitation Index (%) */
    rain_alert_state_t      rain_state;             /**< Classified rain alert state */
    uint8_t                 zambretti_code;         /**< Zambretti forecast index (1..26) */

    /* Scheduler & Power Decisions */
    scheduler_decision_t    sched_decision;         /**< Active scheduler decision */
    uint32_t                configured_sleep_sec;   /**< Next configured RTC sleep duration */
} app_context_t;

/* ========================================================================== */
/* Public State Machine API Prototypes                                        */
/* ========================================================================== */

/**
 * @brief  Initializes all system subsystems, drivers, algorithms, and state context.
 * @return status_t STATUS_OK on success, or hardware fault code.
 */
status_t app_state_machine_init(void);

/**
 * @brief  Executes a single non-blocking step of the active state.
 * @return status_t STATUS_OK on success, STATUS_ERR_BUSY if in-state, or error code.
 */
status_t app_state_machine_step(void);

/**
 * @brief  Executes one full 8-state cyclic sequence from WAKE to SLEEP entry.
 * @return status_t STATUS_OK on completion of full cycle.
 */
status_t app_state_machine_run_cycle(void);

/**
 * @brief  Retrieves the current operational state of the machine.
 * @return app_state_t Current state enumeration.
 */
app_state_t app_state_machine_get_current_state(void);

/**
 * @brief  Retrieves a read-only pointer to the master system operational context.
 * @return const app_context_t* Pointer to context struct.
 */
const app_context_t *app_state_machine_get_context(void);

/**
 * @brief  Returns human-readable English descriptor for an operational state.
 * @param[in] state     State enum identifier.
 * @return const char*  Pointer to static string descriptor.
 */
const char *app_state_machine_get_state_name(app_state_t state);

/**
 * @brief  Resets the application state machine context for unit testing isolation.
 */
void app_state_machine_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_MACHINE_H */
