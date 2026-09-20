/**
 * @file    telemetry_codec.h
 * @brief   LoRaWAN binary telemetry encoder and decoder for STM32WLE5 SoC.
 * @details Packs physical meteorological parameters into a deterministic 12-byte payload.
 */

#ifndef TELEMETRY_CODEC_H
#define TELEMETRY_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Protocol Constants & Port Allocations                                      */
/* ========================================================================== */

/** @brief Fixed length of periodic telemetry payload in bytes */
#define TELEMETRY_PERIODIC_PAYLOAD_SIZE     12U

#define TELEMETRY_ALERT_PAYLOAD_SIZE        4U

/** @brief LoRaWAN Application Port for scheduled periodic telemetry */
#define TELEMETRY_FPORT_PERIODIC            1U

/** @brief LoRaWAN Application Port for emergency storm warning uplinks */
#define TELEMETRY_FPORT_ALERT               2U

/** @brief LoRaWAN Application Port for downlink node configuration */
#define TELEMETRY_FPORT_CONFIG              10U

/* ========================================================================== */
/* Physical Scaling, Resolution & Offset Constants (Periodic Telemetry)       */
/* ========================================================================== */

#define TELEMETRY_TEMP_SCALE                100.0f      /**< 0.01 °C per LSB */
#define TELEMETRY_TEMP_MIN_C                (-40.0f)    /**< -40.00 °C */
#define TELEMETRY_TEMP_MAX_C                85.0f       /**< +85.00 °C */

#define TELEMETRY_HUM_SCALE                 100.0f      /**< 0.01 %RH per LSB */
#define TELEMETRY_HUM_MIN_PCT               0.0f        /**< 0.00 %RH */
#define TELEMETRY_HUM_MAX_PCT               100.0f      /**< 100.00 %RH */

#define TELEMETRY_PRESS_OFFSET_HPA          300.0f      /**< 300.00 hPa base */
#define TELEMETRY_PRESS_STEP_HPA            0.02f       /**< 0.02 hPa per LSB */
#define TELEMETRY_PRESS_MIN_HPA             300.0f      /**< 300.00 hPa */
#define TELEMETRY_PRESS_MAX_HPA             1100.0f     /**< 1100.00 hPa */

#define TELEMETRY_LUX_STEP                  2.0f        /**< 2.0 Lux per LSB */
#define TELEMETRY_LUX_MIN                   0.0f        /**< 0.0 Lux */
#define TELEMETRY_LUX_MAX                   83000.0f    /**< 83,000.0 Lux */

#define TELEMETRY_RAIN_STEP_MM              0.2f        /**< 0.20 mm per LSB */
#define TELEMETRY_RAIN_MIN_MM               0.0f        /**< 0.0 mm */
#define TELEMETRY_RAIN_MAX_MM               51.0f       /**< 51.0 mm (255 * 0.2) */

#define TELEMETRY_VBAT_OFFSET_V             2.50f       /**< 2.500 V base */
#define TELEMETRY_VBAT_STEP_V               0.020f      /**< 0.020 V (20 mV) per LSB */
#define TELEMETRY_VBAT_MIN_V                2.50f       /**< 2.500 V */
#define TELEMETRY_VBAT_MAX_V                3.76f       /**< 3.760 V (2.50 + 63 * 0.02) */

#define TELEMETRY_ZAMBRETTI_MIN             1U          /**< Zambretti 'A' */
#define TELEMETRY_ZAMBRETTI_MAX             26U         /**< Zambretti 'Z' */

#define TELEMETRY_CPI_MAX_PCT               100U        /**< 100% Probability */

/* ========================================================================== */
/* Urgent Alert Payload Scaling & Offset Constants                            */
/* ========================================================================== */

#define TELEMETRY_PRESS_RATE_STEP_HPA_H     0.05f       /**< 0.05 hPa/hr per LSB */
#define TELEMETRY_PRESS_RATE_MIN_HPA_H      (-6.40f)    /**< -6.40 hPa/hr */
#define TELEMETRY_PRESS_RATE_MAX_HPA_H      6.35f       /**< +6.35 hPa/hr */

#define TELEMETRY_ALERT_VBAT_OFFSET_V       2.50f       /**< 2.500 V base */
#define TELEMETRY_ALERT_VBAT_STEP_V         0.040f      /**< 0.040 V (40 mV) per LSB */
#define TELEMETRY_ALERT_VBAT_MIN_V          2.50f       /**< 2.500 V */
#define TELEMETRY_ALERT_VBAT_MAX_V          3.74f       /**< 3.740 V (2.50 + 31 * 0.04) */

#define TELEMETRY_ALERT_SEQ_MAX             7U          /**< 3-bit rolling sequence [0..7] */

/* ========================================================================== */
/* Data Structures & Typedefs                                                 */
/* ========================================================================== */

/**
 * @brief Nowcast rain alert operational state classification for over-the-air telemetry.
 */
typedef enum {
    TELEMETRY_RAIN_STATE_UNLIKELY    = 0U,    /**< 00: Rain unlikely (CPI < 30%) */
    TELEMETRY_RAIN_STATE_POSSIBLE    = 1U,    /**< 01: Rain possible (30% <= CPI < 60%) */
    TELEMETRY_RAIN_STATE_IMMINENT    = 2U,    /**< 10: Rain imminent (CPI >= 60%) */
    TELEMETRY_RAIN_STATE_ACTIVE_RAIN = 3U     /**< 11: Physical rainfall in progress */
} telemetry_rain_state_t;

/**
 * @brief Primary trigger cause for urgent storm alert uplink.
 */
typedef enum {
    ALERT_TRIGGER_MANUAL            = 0U,   /**< 000: Manual diagnostic / test trigger */
    ALERT_TRIGGER_CPI_THRESHOLD     = 1U,   /**< 001: CPI score threshold exceeded (CPI >= 70%) */
    ALERT_TRIGGER_PRESSURE_PLUNGE   = 2U,   /**< 010: Rapid barometric drop (> 2.0 hPa / 3h) */
    ALERT_TRIGGER_SOLAR_COLLAPSE    = 3U,   /**< 011: Sudden solar attenuation (> 50% drop in 30min) */
    ALERT_TRIGGER_FIRST_RAIN_TIP    = 4U,   /**< 100: Physical rain gauge bucket tip detected */
    ALERT_TRIGGER_COMBINED_GRADIENT = 5U,   /**< 101: Multi-variable simultaneous surge */
    ALERT_TRIGGER_DEW_CONVERGENCE   = 6U,   /**< 110: Dew point depression convergence (DPD < 0.5 °C) */
    ALERT_TRIGGER_LOW_BATTERY       = 7U    /**< 111: Critical battery depletion warning (Vbat < 2.80 V) */
} telemetry_alert_trigger_t;

/**
 * @brief Rainfall rate intensity classification tier.
 */
typedef enum {
    TELEMETRY_RAIN_INTENSITY_NONE     = 0U,   /**< 00: No rain (0.0 mm/hr) */
    TELEMETRY_RAIN_INTENSITY_LIGHT    = 1U,   /**< 01: Light rain (0.1 .. 2.5 mm/hr) */
    TELEMETRY_RAIN_INTENSITY_MODERATE = 2U,   /**< 10: Moderate rain (2.5 .. 10.0 mm/hr) */
    TELEMETRY_RAIN_INTENSITY_HEAVY    = 3U    /**< 11: Heavy / torrential rain (> 10.0 mm/hr) */
} telemetry_rain_intensity_t;

/**
 * @brief Unpacked periodic telemetry structure representing physical engineering units.
 */
typedef struct {
    float                  temperature_c;          /**< Ambient temperature in °C [-40.00 .. +85.00] */
    float                  humidity_pct;           /**< Relative humidity in %RH [0.00 .. 100.00] */
    float                  pressure_hpa;           /**< Barometric pressure in hPa [300.00 .. 1100.00] */
    float                  ambient_lux;            /**< Ambient illuminance in Lux [0.0 .. 83000.0] */
    float                  rain_interval_mm;       /**< Rainfall accumulated in interval in mm [0.0 .. 51.0] */
    telemetry_rain_state_t forecast_state;         /**< Operational nowcast state */
    uint8_t                zambretti_index;        /**< Zambretti heuristic index [1 .. 26] */
    uint8_t                cpi_prob_pct;           /**< Composite Precipitation Index probability [0 .. 100%] */
    bool                   solar_cloud_drop_alarm; /**< Convective cloud attenuation trigger flag */
    float                  battery_voltage_v;      /**< Battery terminal voltage in Volts [2.50 .. 3.76] */
    bool                   sensor_fault;           /**< True if sensor bus communication error detected */
    bool                   unexpected_reset;       /**< True if MCU underwent watchdog/brownout/hardware reset */
} telemetry_periodic_data_t;

/**
 * @brief Unpacked urgent storm alert structure representing engineering units.
 */
typedef struct {
    telemetry_rain_state_t      alert_state;            /**< Alert operational state */
    telemetry_alert_trigger_t   trigger_cause;          /**< Primary trigger cause code */
    uint8_t                     alert_sequence_id;      /**< Rolling sequence token [0 .. 7] */
    uint8_t                     cpi_prob_pct;           /**< Composite Precipitation Index [0 .. 100%] */
    bool                        solar_cloud_drop_alarm; /**< Convective cloud attenuation trigger */
    float                       pressure_rate_hpa_per_h;/**< 1-hour barometric rate in hPa/hr [-6.40 .. +6.35] */
    telemetry_rain_intensity_t  rain_intensity;         /**< Current rain intensity tier */
    bool                        sensor_fault;           /**< True if sensor bus error detected */
    float                       battery_voltage_v;      /**< Battery voltage in Volts [2.50 .. 3.74] */
} telemetry_alert_data_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Serializes periodic environmental and diagnostic data into a 12-byte LoRaWAN binary payload.
 * @param[in]  data        Pointer to source telemetry data structure with engineering units.
 * @param[out] buffer      Pointer to destination byte array to store serialized payload.
 * @param[in]  buffer_size Size of destination buffer in bytes (must be >= 12).
 * @param[out] encoded_len Pointer to store exact number of bytes written (will be 12).
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *         or STATUS_ERROR_BUFFER_TOO_SMALL if buffer_size < 12.
 */
status_t telemetry_encode_periodic(const telemetry_periodic_data_t *data,
                                  uint8_t *buffer,
                                  size_t buffer_size,
                                  size_t *encoded_len);

/**
 * @brief  Deserializes a 12-byte LoRaWAN binary payload into physical engineering units.
 * @param[in]  buffer      Pointer to raw 12-byte binary payload buffer.
 * @param[in]  buffer_size Size of raw payload buffer in bytes (must be >= 12).
 * @param[out] data        Pointer to destination telemetry data structure to populate.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *         or STATUS_ERROR_INVALID_PARAM if buffer_size < 12.
 */
status_t telemetry_decode_periodic(const uint8_t *buffer,
                                  size_t buffer_size,
                                  telemetry_periodic_data_t *data);

/**
 * @brief  Serializes urgent storm alert and event data into a compact 4-byte LoRaWAN binary payload.
 * @param[in]  data        Pointer to source alert data structure with engineering units.
 * @param[out] buffer      Pointer to destination byte array to store serialized payload.
 * @param[in]  buffer_size Size of destination buffer in bytes (must be >= 4).
 * @param[out] encoded_len Pointer to store exact number of bytes written (will be 4).
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *         or STATUS_ERROR_BUFFER_TOO_SMALL if buffer_size < 4.
 */
status_t telemetry_encode_alert(const telemetry_alert_data_t *data,
                               uint8_t *buffer,
                               size_t buffer_size,
                               size_t *encoded_len);

/**
 * @brief  Deserializes a 4-byte LoRaWAN urgent alert binary payload into engineering units.
 * @param[in]  buffer      Pointer to raw 4-byte binary payload buffer.
 * @param[in]  buffer_size Size of raw payload buffer in bytes (must be >= 4).
 * @param[out] data        Pointer to destination alert data structure to populate.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *         or STATUS_ERROR_INVALID_PARAM if buffer_size < 4.
 */
status_t telemetry_decode_alert(const uint8_t *buffer,
                               size_t buffer_size,
                               telemetry_alert_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_CODEC_H */
