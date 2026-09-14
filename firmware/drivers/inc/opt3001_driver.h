/**
 * @file    opt3001_driver.h
 * @brief   Texas Instruments OPT3001 Ambient Light Sensor driver header for STM32WLE5.
 * @details Handles I2C communication, single-shot acquisition, and raw register parsing.
 *          Adheres to C99 standards, MISRA-C guidelines, and zero-dynamic-memory allocation.
 */

#ifndef OPT3001_DRIVER_H
#define OPT3001_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Hardware & Register Definitions                                            */
/* ========================================================================== */

#define OPT3001_I2C_ADDR_DEFAULT            0x44U   /**< Default I2C 7-bit address (ADDR=GND) */

#define OPT3001_REG_RESULT                  0x00U   /**< Measurement result register */
#define OPT3001_REG_CONFIG                  0x01U   /**< Configuration register */
#define OPT3001_REG_LOW_LIMIT               0x02U   /**< Low limit register */
#define OPT3001_REG_HIGH_LIMIT              0x03U   /**< High limit register */
#define OPT3001_REG_MANUFACTURER_ID         0x7EU   /**< Manufacturer ID register */
#define OPT3001_REG_DEVICE_ID               0x7FU   /**< Device ID register */

#define OPT3001_EXPECTED_MFG_ID             0x5449U /**< ASCII 'TI' */
#define OPT3001_EXPECTED_DEV_ID             0x3001U /**< TI OPT3001 identifier */

/* Configuration Register Bitmasks */
#define OPT3001_CONFIG_RN_AUTO              (0x0CU << 12)   /**< Auto-range number (RN = 0b1100) */
#define OPT3001_CONFIG_CT_100MS             (0x00U << 11)   /**< Conversion time 100 ms */
#define OPT3001_CONFIG_CT_800MS             (0x01U << 11)   /**< Conversion time 800 ms */
#define OPT3001_CONFIG_MODE_SHUTDOWN        (0x00U << 9)    /**< Shutdown mode */
#define OPT3001_CONFIG_MODE_SINGLE_SHOT     (0x01U << 9)    /**< Single-shot conversion trigger */
#define OPT3001_CONFIG_MODE_CONTINUOUS      (0x02U << 9)    /**< Continuous conversion mode */
#define OPT3001_CONFIG_OVF_BIT              (1U << 8)       /**< Overflow status bit */
#define OPT3001_CONFIG_CRF_BIT              (1U << 7)       /**< Conversion Ready Flag bit */
#define OPT3001_CONFIG_FH_BIT               (1U << 6)       /**< Flag High comparison bit */
#define OPT3001_CONFIG_FL_BIT               (1U << 5)       /**< Flag Low comparison bit */
#define OPT3001_CONFIG_LATCH_BIT            (1U << 4)       /**< Latch comparison window bit */
#define OPT3001_CONFIG_POL_BIT              (1U << 3)       /**< Interrupt Polarity (0=Active Low) */
#define OPT3001_CONFIG_ME_BIT               (1U << 2)       /**< Mask Exponent bit */
#define OPT3001_CONFIG_FC_1                 (0x00U << 0)    /**< Fault count 1 */

/** @brief Default single-shot configuration trigger word (Auto-range, 100ms, Single-shot, Latch) */
#define OPT3001_CONFIG_SINGLE_SHOT_CMD      (OPT3001_CONFIG_RN_AUTO | \
                                             OPT3001_CONFIG_CT_100MS | \
                                             OPT3001_CONFIG_MODE_SINGLE_SHOT | \
                                             OPT3001_CONFIG_LATCH_BIT)

#define OPT3001_CONVERSION_TIMEOUT_MS       150U    /**< Maximum conversion wait timeout */
#define OPT3001_I2C_TIMEOUT_MS              50U     /**< Maximum I2C bus transaction timeout */

/* ========================================================================== */
/* Mathematical & Telemetry Constants                                        */
/* ========================================================================== */

#define OPT3001_MAX_EXPONENT                11U         /**< Maximum valid exponent E[3:0] */
#define OPT3001_MAX_LUX                     83865.60f   /**< Maximum full-scale lux output */
#define OPT3001_MIN_LUX                     0.0f        /**< Minimum valid lux reading */

#define OPT3001_SOLAR_LUMINOUS_EFFICACY     120.0f      /**< Daylight efficacy in lumens/Watt */
#define OPT3001_TELEMETRY_LUX_SCALE         2.0f        /**< LoRaWAN payload scale: 2.0 Lux/LSB */
#define OPT3001_TELEMETRY_MAX_RAW           41500U      /**< Max value for 83,000 Lux */

/* Day/Night & Cloud Attenuation Thresholds */
#define OPT3001_NIGHT_THRESHOLD_LUX         10.0f       /**< Lux below which night is asserted */
#define OPT3001_DAWN_THRESHOLD_LUX          15.0f       /**< Lux required to exit night state */
#define OPT3001_DAYLIGHT_CONFIRM_LUX        50.0f       /**< Lux required to confirm daylight */
#define OPT3001_DUSK_THRESHOLD_LUX          40.0f       /**< Lux below which daylight is lost */

#define OPT3001_ATTENUATION_MIN_HIST_LUX    5000.0f     /**< Minimum 30m average lux to score drop */
#define OPT3001_ATTENUATION_SEVERE_MAX_LUX  3000.0f     /**< Max current lux for severe storm score */

#define OPT3001_DROP_RATIO_SEVERE           0.70f       /**< 70% drop threshold */
#define OPT3001_DROP_RATIO_MODERATE         0.50f       /**< 50% drop threshold */
#define OPT3001_DROP_RATIO_MINOR            0.30f       /**< 30% drop threshold */

/* ========================================================================== */
/* Data Types                                                                 */
/* ========================================================================== */

/**
 * @brief OPT3001 I2C Slave 7-bit addresses.
 */
typedef enum {
    OPT3001_I2C_ADDR_GND = 0x44U,   /**< ADDR pin connected to GND (Default) */
    OPT3001_I2C_ADDR_VDD = 0x45U,   /**< ADDR pin connected to VDD */
    OPT3001_I2C_ADDR_SDA = 0x46U,   /**< ADDR pin connected to SDA */
    OPT3001_I2C_ADDR_SCL = 0x47U    /**< ADDR pin connected to SCL */
} opt3001_i2c_addr_t;

/**
 * @brief Categorized ambient daylight operational states.
 */
typedef enum {
    OPT3001_STATE_NIGHT     = 0,    /**< Lux < 10.0 Lux: Nocturnal darkness */
    OPT3001_STATE_TWILIGHT  = 1,    /**< 10.0 <= Lux < 50.0 Lux: Dawn / Dusk */
    OPT3001_STATE_DAYLIGHT  = 2     /**< Lux >= 50.0 Lux: Active daylight */
} opt3001_day_state_t;

/**
 * @brief Categorized daylight storm cloud attenuation levels.
 */
typedef enum {
    OPT3001_ATTENUATION_NONE     = 0,   /**< Score 0: Clear / steady sky */
    OPT3001_ATTENUATION_MINOR    = 1,   /**< Score 30: 30% - 49% drop */
    OPT3001_ATTENUATION_MODERATE = 2,   /**< Score 65: 50% - 69% drop (Alarm Flag = 1) */
    OPT3001_ATTENUATION_SEVERE   = 3    /**< Score 100: >= 70% drop to < 3000 Lux (Alarm Flag = 1) */
} opt3001_attenuation_level_t;

/**
 * @brief Consolidated solar environment context structure.
 */
typedef struct {
    opt3001_day_state_t         day_state;          /**< Current day/night classification */
    bool                        is_daylight;        /**< True if state is OPT3001_STATE_DAYLIGHT */
    opt3001_attenuation_level_t attenuation_level;  /**< Categorized cloud attenuation severity */
    uint8_t                     attenuation_score;  /**< Attenuation score (0 to 100) */
    float                       drop_ratio;         /**< Fractional drop ratio (0.0 to 1.0) */
    bool                        solar_drop_alarm;   /**< True if drop ratio >= 50% (Byte 10 Bit 7) */
} opt3001_solar_context_t;

/**
 * @brief OPT3001 raw 16-bit register contents and unpacked fields.
 */
typedef struct {
    uint16_t raw_result;    /**< Full 16-bit register 0x00 contents */
    uint8_t  exponent;      /**< 4-bit exponent field E[3:0] (0 to 11) */
    uint16_t mantissa;      /**< 12-bit mantissa field R[11:0] (0 to 4095) */
} opt3001_raw_data_t;

/**
 * @brief Complete physical optical measurement and telemetry structure.
 */
typedef struct {
    float       lux;                /**< Optical illuminance in Lux (0.00 to 83865.60 Lux) */
    float       irradiance_w_m2;    /**< Solar irradiance in W/m² (0.0 to ~700.0 W/m²) */
    uint32_t    centi_lux;          /**< Scaled integer illuminance in 0.01 Lux units */
    uint16_t    telemetry_raw;      /**< 16-bit scaled integer (2.0 Lux/LSB) for LoRaWAN Bytes 6-7 */
    bool        is_valid;           /**< True if measurement is within valid physical bounds */
} opt3001_reading_t;

/**
 * @brief OPT3001 device state handle.
 */
typedef struct {
    uint8_t             i2c_address;        /**< I2C 7-bit slave address */
    uint16_t            manufacturer_id;    /**< Detected Manufacturer ID (0x5449) */
    uint16_t            device_id;          /**< Detected Device ID (0x3001) */
    bool                is_initialized;     /**< True if initialized and verified */
} opt3001_dev_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes OPT3001 device handle and validates Manufacturer & Device IDs.
 * @param  dev Pointer to OPT3001 device structure.
 * @param  i2c_addr 7-bit I2C slave address (e.g. OPT3001_I2C_ADDR_DEFAULT).
 * @return STATUS_OK on success, STATUS_ERR_HARDWARE on ID mismatch, or bus error.
 */
status_t opt3001_init(opt3001_dev_t *dev, uint8_t i2c_addr);

/**
 * @brief  Reads 16-bit Manufacturer and Device identification registers.
 * @param  dev Pointer to OPT3001 device structure.
 * @param[out] p_mfg_id Pointer to store Manufacturer ID.
 * @param[out] p_dev_id Pointer to store Device ID.
 * @return STATUS_OK on success, or bus error status code.
 */
status_t opt3001_read_device_id(opt3001_dev_t *dev, uint16_t *p_mfg_id, uint16_t *p_dev_id);

/**
 * @brief  Arms and triggers a 100ms auto-range single-shot measurement.
 * @param  dev Pointer to OPT3001 device structure.
 * @return STATUS_OK on success, or bus error status code.
 */
status_t opt3001_trigger_single_shot(opt3001_dev_t *dev);

/**
 * @brief  Polls the Configuration Register to check if conversion is complete (CRF == 1).
 * @param  dev Pointer to OPT3001 device structure.
 * @param[out] p_is_ready Pointer set to true if conversion is ready.
 * @return STATUS_OK on success, or bus error status code.
 */
status_t opt3001_is_conversion_ready(opt3001_dev_t *dev, bool *p_is_ready);

/**
 * @brief  Blocks with polling until conversion completes or timeout expires.
 * @param  dev Pointer to OPT3001 device structure.
 * @param  timeout_ms Maximum timeout in milliseconds (e.g. 150 ms).
 * @return STATUS_OK if complete, STATUS_ERR_TIMEOUT if elapsed.
 */
status_t opt3001_wait_for_completion(opt3001_dev_t *dev, uint32_t timeout_ms);

/**
 * @brief  Reads the 16-bit Result Register (0x00) and unpacks Exponent and Mantissa.
 * @param  dev Pointer to OPT3001 device structure.
 * @param[out] p_raw Pointer to store unpacked raw result.
 * @return STATUS_OK on success, or error status code.
 */
status_t opt3001_read_raw_result(opt3001_dev_t *dev, opt3001_raw_data_t *p_raw);

/**
 * @brief  Master wrapper: triggers single-shot conversion, waits, and reads raw result.
 * @param  dev Pointer to OPT3001 device structure.
 * @param[out] p_raw Pointer to store unpacked raw result.
 * @return STATUS_OK on success, or error status code.
 */
status_t opt3001_sample_forced_raw(opt3001_dev_t *dev, opt3001_raw_data_t *p_raw);

/**
 * @brief  Converts a 16-bit raw register word to floating-point Lux value.
 * @param  raw_result 16-bit contents of register 0x00.
 * @return Ambient illuminance in Lux.
 */
float opt3001_raw_to_lux(uint16_t raw_result);

/**
 * @brief  Exact 32-bit fixed-point integer conversion (0.01 Lux / count).
 * @param  raw_result 16-bit contents of register 0x00.
 * @return Scaled illuminance in centi-lux.
 */
uint32_t opt3001_raw_to_centi_lux(uint16_t raw_result);

/**
 * @brief  Approximates broadband solar irradiance in W/m² from illuminance.
 * @param  lux Ambient optical illuminance in Lux.
 * @return Solar irradiance in W/m².
 */
float opt3001_lux_to_irradiance(float lux);

/**
 * @brief  Scales and clamps float Lux into 16-bit integer for LoRaWAN payload.
 * @param  lux Ambient optical illuminance in Lux.
 * @return 16-bit unsigned integer formatted for telemetry payload Bytes 6-7.
 */
uint16_t opt3001_lux_to_telemetry_u16(float lux);

/**
 * @brief  Converts unpacked raw ADC data into complete physical reading structure.
 * @param  p_raw Pointer to unpacked raw structure.
 * @param[out] p_out Pointer to output reading structure.
 * @return STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t opt3001_convert_raw(const opt3001_raw_data_t *p_raw, opt3001_reading_t *p_out);

/**
 * @brief  High-level API: triggers conversion, waits, reads, and converts into reading structure.
 * @param  dev Pointer to initialized OPT3001 device context.
 * @param[out] p_out Pointer to output reading structure.
 * @return STATUS_OK on success, or error status code.
 */
status_t opt3001_read_lux(opt3001_dev_t *dev, opt3001_reading_t *p_out);

/**
 * @brief  Classifies ambient daylight state using hysteresis band.
 * @param  current_lux Ambient illuminance in Lux.
 * @param  prev_state Previous state from last sample cycle.
 * @return OPT3001_STATE_NIGHT, _TWILIGHT, or _DAYLIGHT.
 */
opt3001_day_state_t opt3001_classify_day_state(float current_lux, opt3001_day_state_t prev_state);

/**
 * @brief  Returns true if ambient illuminance represents confirmed daylight.
 * @param  current_lux Ambient illuminance in Lux.
 * @param  prev_is_daylight Previous daylight flag.
 * @return True if in daylight state.
 */
bool opt3001_is_daylight(float current_lux, bool prev_is_daylight);

/**
 * @brief  Evaluates 30-minute cloud drop ratio and returns storm attenuation score.
 * @param  current_lux Current optical reading in Lux.
 * @param  history_30m_lux Historical 30-minute moving average in Lux.
 * @param  is_daylight True if daytime.
 * @param[out] p_drop_ratio Pointer to store computed fractional drop (0.0 to 1.0).
 * @param[out] p_solar_alarm Pointer to store solar alarm flag (true if drop >= 50%).
 * @return Attenuation score (0, 30, 65, or 100).
 */
uint8_t opt3001_evaluate_solar_attenuation(float current_lux,
                                           float history_30m_lux,
                                           bool is_daylight,
                                           float *p_drop_ratio,
                                           bool *p_solar_alarm);

/**
 * @brief  Updates complete solar context structure for nowcasting engine.
 * @param  current_lux Current optical reading in Lux.
 * @param  history_30m_lux Historical 30-minute moving average in Lux.
 * @param[in,out] p_ctx Pointer to solar context structure to update.
 * @return STATUS_OK on success, or STATUS_ERR_NULL_PTR.
 */
status_t opt3001_update_solar_context(float current_lux, float history_30m_lux, opt3001_solar_context_t *p_ctx);

#ifdef __cplusplus
}
#endif

#endif /* OPT3001_DRIVER_H */
