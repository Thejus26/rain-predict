/**
 * @file    app_fault_handler.h
 * @brief   System fault monitoring, self-healing, and graceful degradation engine.
 * @details Manages hardware communication recovery, sensor fallbacks, and diagnostic registries.
 *
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#ifndef APP_FAULT_HANDLER_H
#define APP_FAULT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Fault Bitmask Definitions                                                  */
/* ========================================================================== */

#define FAULT_MASK_NONE                 0x0000U
#define FAULT_MASK_BME280_COMM          0x0001U /**< BME280 I2C communication failure */
#define FAULT_MASK_OPT3001_COMM         0x0002U /**< OPT3001 I2C communication failure */
#define FAULT_MASK_I2C_BUS_LOCKUP       0x0004U /**< I2C bus lockup detected and recovered */
#define FAULT_MASK_RAIN_GAUGE_CHATTER   0x0008U /**< Rain gauge contact chatter clamped */
#define FAULT_MASK_MODBUS_COMM          0x0010U /**< Auxiliary RS-485 bus timeout */
#define FAULT_MASK_SDI12_COMM           0x0020U /**< Auxiliary SDI-12 bus timeout */
#define FAULT_MASK_FLASH_WRITE          0x0040U /**< Flash NVM double-word write failure */
#define FAULT_MASK_LORA_TX_TIMEOUT      0x0080U /**< Sub-GHz LoRaWAN radio TX timeout */
#define FAULT_MASK_BATTERY_LOW          0x0100U /**< Battery voltage below Conservation threshold */
#define FAULT_MASK_BATTERY_CRITICAL     0x0200U /**< Battery voltage below Critical threshold */

#define FAULT_MAX_CONSECUTIVE_HEAL      3U      /**< Clean cycles to auto-clear fault */
#define FAULT_MAX_PRESSURE_STALE_CYCLES 3U      /**< Max cycles to hold stale pressure */

#define FAULT_PRESSURE_NEUTRAL_DEFAULT_HPA  950.0f  /**< Standard elevation neutral pressure */
#define FAULT_TEMP_NEUTRAL_DEFAULT_C        20.0f   /**< Neutral baseline temperature */
#define FAULT_HUM_NEUTRAL_DEFAULT_PCT       70.0f   /**< Neutral baseline relative humidity */
#define FAULT_LUX_DAYTIME_ESTIMATE          25000.0f /**< Neutral daylight estimate (06:00..18:00) */
#define FAULT_LUX_NIGHTTIME_ESTIMATE        0.0f    /**< Nocturnal illuminance default */

/* ========================================================================== */
/* Type Definitions                                                           */
/* ========================================================================== */

/**
 * @brief  Diagnostic status snapshot of system health and fault counters.
 */
typedef struct {
    uint16_t active_fault_mask;             /**< Bitmask of currently active faults */
    uint16_t latched_fault_mask;            /**< Cumulative latched faults since boot */
    uint32_t bme280_fail_count;             /**< Lifetime BME280 read failures */
    uint32_t opt3001_fail_count;            /**< Lifetime OPT3001 read failures */
    uint32_t i2c_lockup_count;              /**< Lifetime I2C bus recoveries */
    uint32_t lora_timeout_count;            /**< Lifetime LoRa TX timeouts */
    uint32_t flash_error_count;             /**< Lifetime Flash write errors */
    uint8_t  bme280_consecutive_clean;      /**< Consecutive successful BME280 reads */
    uint8_t  opt3001_consecutive_clean;     /**< Consecutive successful OPT3001 reads */
    uint8_t  pressure_stale_count;          /**< Cycles using held stale pressure */
    float    last_valid_pressure_hpa;       /**< Last known good barometric pressure */
    float    last_valid_temperature_c;      /**< Last known good temperature */
    float    last_valid_humidity_pct;       /**< Last known good humidity */
} fault_handler_status_t;

/* ========================================================================== */
/* Public Function Prototypes                                                 */
/* ========================================================================== */

/**
 * @brief  Initializes the fault handler and clears active diagnostic counters.
 * @return status_t STATUS_OK on success.
 */
status_t app_fault_handler_init(void);

/**
 * @brief  Records the result of a sensor or peripheral transaction.
 * @param[in] fault_bit Flag bitmask identifying the subsystem.
 * @param[in] success   True if operation succeeded, false if failed.
 */
void app_fault_handler_report(uint16_t fault_bit, bool success);

/**
 * @brief  Updates the cached last known good BME280 readings.
 * @param[in] temp_c    Valid temperature reading in °C.
 * @param[in] rh_pct    Valid relative humidity in %RH.
 * @param[in] press_hpa Valid barometric pressure in hPa.
 */
void app_fault_handler_set_last_valid_bme280(float temp_c, float rh_pct, float press_hpa);

/**
 * @brief  Attempts autonomous recovery on the I2C sensor bus if stuck.
 * @details Executes 9-clock recovery pulse; if still failing, power cycles switched sensor rail.
 * @return status_t STATUS_OK if bus is healthy or recovered, error code if still locked.
 */
status_t app_fault_handler_recover_i2c_bus(void);

/**
 * @brief  Applies meteorological fallback values if BME280 acquisition failed.
 * @param[in,out] p_temp_c  Pointer to temperature reading (updated if fallback used).
 * @param[in,out] p_rh_pct  Pointer to humidity reading (updated if fallback used).
 * @param[in,out] p_press   Pointer to pressure reading (updated if fallback used).
 * @return bool             True if valid stale cached data available, false if neutral defaults used.
 */
bool app_fault_handler_get_bme280_fallback(float *p_temp_c, float *p_rh_pct, float *p_press);

/**
 * @brief  Applies solar illuminance fallback if OPT3001 acquisition failed.
 * @param[in]     rtc_hour  Current RTC hour (0..23).
 * @param[in,out] p_lux     Pointer to lux reading (updated to daytime estimate).
 */
void app_fault_handler_get_opt3001_fallback(uint8_t rtc_hour, float *p_lux);

/**
 * @brief  Retrieves the master system fault status (Byte 11 bit 5).
 * @return bool True if any core sensor or hardware fault is currently active.
 */
bool app_fault_handler_is_system_fault_active(void);

/**
 * @brief  Retrieves full diagnostic telemetry snapshot.
 * @param[out] p_status Destination status structure pointer.
 * @return status_t     STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t app_fault_handler_get_status(fault_handler_status_t *p_status);

/**
 * @brief  Resets the fault handler internal state for unit test isolation.
 */
void app_fault_handler_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FAULT_HANDLER_H */
