/**
 * @file    modbus_rtu.h
 * @brief   Modbus RTU Master protocol definitions and frame generator.
 * @details Implements Function Code 0x03 (Read Holding Registers), CRC-16,
 *          response parsing, exception handling, and meteorological decoding.
 */

#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stdint.h>
#include <stdbool.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Protocol Constants                                                         */
/* ========================================================================== */

#define MODBUS_FC03_READ_HOLDING_REGISTERS  0x03U
#define MODBUS_FC03_REQ_FRAME_SIZE          8U
#define MODBUS_MIN_RESP_FRAME_SIZE          5U
#define MODBUS_MAX_READ_REGISTERS           125U
#define MODBUS_MAX_FRAME_SIZE               256U
#define MODBUS_DEFAULT_SLAVE_ADDR           0x01U
#define MODBUS_BROADCAST_ADDR               0x00U
#define MODBUS_MAX_SLAVE_ADDR               247U
#define MODBUS_EXCEPTION_MASK               0x80U

/* Standard Microclimate Weather Probe Register Addresses */
#define MODBUS_REG_TEMPERATURE              0x0000U /**< 0.01 deg C (signed int16) */
#define MODBUS_REG_HUMIDITY                 0x0001U /**< 0.01 % RH (uint16) */
#define MODBUS_REG_PRESSURE                 0x0002U /**< 0.1 hPa (uint16) */
#define MODBUS_REG_WIND_SPEED               0x0003U /**< 0.01 m/s (uint16) */
#define MODBUS_REG_WIND_DIRECTION           0x0004U /**< 0.1 deg (uint16) */

/* Timing and Retry Constants */
#define MODBUS_DEFAULT_TIMEOUT_MS           150U    /**< Standard response timeout in ms */
#define MODBUS_MAX_RETRIES                  3U      /**< Max query attempts on timeout/CRC error */
#define MODBUS_GUARD_TIME_PRE_US            25U     /**< 25 µs pre-transmission guard delay */
#define MODBUS_GUARD_TIME_POST_US           35U     /**< 35 µs post-transmission guard delay */
#define MODBUS_RETRY_DELAY_US               4000U   /**< 4 ms inter-frame retry delay */

/* Scaling Factors */
#define MODBUS_SCALE_TEMP_C                 0.01f
#define MODBUS_SCALE_HUMIDITY_PCT           0.01f
#define MODBUS_SCALE_PRESSURE_HPA           0.10f
#define MODBUS_SCALE_WIND_SPEED_MPS         0.01f
#define MODBUS_SCALE_WIND_DIR_DEG           0.10f

/* ========================================================================== */
/* Data Structures & Enums                                                    */
/* ========================================================================== */

typedef enum {
    MODBUS_EX_NONE                  = 0x00,
    MODBUS_EX_ILLEGAL_FUNCTION      = 0x01,
    MODBUS_EX_ILLEGAL_DATA_ADDRESS  = 0x02,
    MODBUS_EX_ILLEGAL_DATA_VALUE    = 0x03,
    MODBUS_EX_SLAVE_DEVICE_FAILURE  = 0x04,
    MODBUS_EX_ACKNOWLEDGE           = 0x05,
    MODBUS_EX_SLAVE_DEVICE_BUSY     = 0x06,
    MODBUS_EX_NEGATIVE_ACKNOWLEDGE  = 0x07,
    MODBUS_EX_MEMORY_PARITY_ERROR   = 0x08
} modbus_exception_t;

typedef struct {
    float               temperature_c;      /**< Ambient Temperature in degrees Celsius */
    float               humidity_pct;       /**< Relative Humidity in % RH */
    float               pressure_hpa;       /**< Barometric Pressure in hPa */
    float               wind_speed_mps;     /**< Wind Speed in m/s (if equipped) */
    float               wind_direction_deg; /**< Wind Direction in degrees (0-360) */
    bool                has_wind_data;      /**< True if wind registers were included */
    uint32_t            timestamp_ms;       /**< System tick timestamp of acquisition */
} modbus_thp_reading_t;

/* ========================================================================== */
/* Public Function Prototypes                                                 */
/* ========================================================================== */

/**
 * @brief  Computes standard Modbus RTU CRC-16 (Polynomial: 0xA001, Init: 0xFFFF).
 * @note   Maps directly to modbus_crc16_lut() for maximum throughput.
 * @param  p_buffer Pointer to data buffer.
 * @param  length Number of bytes to calculate.
 * @return 16-bit CRC checksum (Low-byte in lower 8 bits).
 */
uint16_t modbus_crc16(const uint8_t *p_buffer, uint16_t length);

/**
 * @brief  Computes Modbus RTU CRC-16 using bitwise shift method.
 * @param  p_buffer Pointer to byte buffer.
 * @param  length Number of bytes to compute over.
 * @return 16-bit CRC checksum.
 */
uint16_t modbus_crc16_bitwise(const uint8_t *p_buffer, uint16_t length);

/**
 * @brief  Computes Modbus RTU CRC-16 using 256-entry Flash lookup table.
 * @param  p_buffer Pointer to byte buffer.
 * @param  length Number of bytes to compute over.
 * @return 16-bit CRC checksum.
 */
uint16_t modbus_crc16_lut(const uint8_t *p_buffer, uint16_t length);

/**
 * @brief  Incremental/streaming CRC-16 update function.
 * @param  current_crc Running CRC accumulator (initialize to 0xFFFF for first block).
 * @param  p_data Pointer to next chunk of data bytes.
 * @param  length Number of bytes in this chunk.
 * @return Updated 16-bit CRC value.
 */
uint16_t modbus_crc16_update(uint16_t current_crc, const uint8_t *p_data, uint16_t length);

/**
 * @brief  Validates whether the trailing 2 bytes of a Modbus frame match its computed CRC-16.
 * @param  p_frame Pointer to complete Modbus frame (including trailing 2 CRC bytes).
 * @param  frame_len Total frame length (must be >= 3).
 * @return true if CRC matches perfectly, false otherwise.
 */
bool modbus_validate_frame_crc(const uint8_t *p_frame, uint16_t frame_len);

/**
 * @brief  Appends 2-byte CRC-16 (Low-byte first) to the end of a payload buffer.
 * @param[in,out] p_frame Pointer to buffer containing payload.
 * @param  data_len Number of payload bytes currently in buffer.
 * @param  max_buf_len Total allocated capacity of p_frame.
 * @param[out] p_total_len Pointer to store resulting total length (data_len + 2).
 * @return STATUS_OK on success, or structured error code.
 */
status_t modbus_append_crc16(uint8_t *p_frame,
                             uint16_t data_len,
                             uint16_t max_buf_len,
                             uint16_t *p_total_len);

/**
 * @brief  Constructs an 8-byte Modbus RTU FC03 Read Holding Registers query frame.
 * @param  slave_addr Slave device address (1 to 247).
 * @param  start_reg Starting register address (0x0000 to 0xFFFF).
 * @param  reg_count Number of 16-bit registers to read (1 to 125).
 * @param[out] p_out_buf Destination buffer for generated 8-byte frame.
 * @param  max_out_len Maximum capacity of destination buffer (must be >= 8).
 * @param[out] p_frame_len Pointer to store resulting frame length (always 8 on success).
 * @return STATUS_OK on success, or structured error code.
 */
status_t modbus_build_read_holding_registers_req(uint8_t slave_addr,
                                                 uint16_t start_reg,
                                                 uint16_t reg_count,
                                                 uint8_t *p_out_buf,
                                                 uint16_t max_out_len,
                                                 uint16_t *p_frame_len);

/**
 * @brief  Parses and validates a Modbus RTU FC03 response frame.
 * @param  expected_slave Slave address expected in response.
 * @param  expected_reg_count Number of registers requested in original query.
 * @param  p_resp_buf Pointer to received response frame buffer.
 * @param  resp_len Number of bytes in received response buffer.
 * @param[out] p_reg_data_out Array to store unpacked 16-bit register words (size >= expected_reg_count).
 * @param[out] p_exception_out Pointer to store Modbus exception code if encountered (can be NULL).
 * @return STATUS_OK on success, STATUS_ERROR_MODBUS_EXCEPTION if slave returned error,
 *         STATUS_ERROR_CRC if checksum fails, or STATUS_ERROR_INVALID_FRAME.
 */
status_t modbus_parse_read_holding_registers_resp(uint8_t expected_slave,
                                                  uint16_t expected_reg_count,
                                                  const uint8_t *p_resp_buf,
                                                  uint16_t resp_len,
                                                  uint16_t *p_reg_data_out,
                                                  modbus_exception_t *p_exception_out);

/**
 * @brief  Decodes standard weather probe holding registers into calibrated units.
 * @param  p_reg_data Array of raw 16-bit register values.
 * @param  reg_count Number of registers present (must be >= 3 for THP, >= 5 for THP+Wind).
 * @param[out] p_reading Pointer to modbus_thp_reading_t structure to populate.
 * @return STATUS_OK on success, or STATUS_ERROR_INVALID_PARAM.
 */
status_t modbus_decode_thp_registers(const uint16_t *p_reg_data,
                                     uint16_t reg_count,
                                     modbus_thp_reading_t *p_reading);

/**
 * @brief  Converts a Modbus exception enum into a diagnostic string.
 * @param  exception_code Exception enum value.
 * @return Constant pointer to descriptive string.
 */
const char *modbus_exception_to_str(modbus_exception_t exception_code);

/* ========================================================================== */
/* Master Transaction & Direction Control Prototypes                          */
/* ========================================================================== */

/**
 * @brief  Initializes the Modbus RTU driver subsystem and configures direction GPIO.
 * @return STATUS_OK on success, or structured error code.
 */
status_t modbus_rtu_init(void);

/**
 * @brief  Drives PA1 HIGH to enable the SP3485 differential transmitter (DE=1, /RE=1).
 */
void modbus_set_direction_tx(void);

/**
 * @brief  Drives PA1 LOW to enable the SP3485 differential receiver (DE=0, /RE=0).
 */
void modbus_set_direction_rx(void);

/**
 * @brief  Executes a complete master query transaction for raw holding registers.
 * @param  slave_addr Unicast address of remote Modbus slave (1 to 247).
 * @param  start_reg Starting 16-bit register address.
 * @param  reg_count Number of registers to read (1 to 125).
 * @param[out] p_reg_data_out Buffer to store received 16-bit register words.
 * @param  timeout_ms Maximum response timeout in milliseconds (e.g. 150 ms).
 * @return STATUS_OK on success, STATUS_ERROR_TIMEOUT, STATUS_ERROR_CRC,
 *         STATUS_ERROR_MODBUS_EXCEPTION, or STATUS_ERROR_INVALID_FRAME.
 */
status_t modbus_query_slave_raw(uint8_t slave_addr,
                                uint16_t start_reg,
                                uint16_t reg_count,
                                uint16_t *p_reg_data_out,
                                uint32_t timeout_ms);

/**
 * @brief  Executes a standard microclimate mast query and decodes meteorological data.
 * @param  slave_addr Slave address of the weather station mast (default 0x01).
 * @param[out] p_reading Pointer to modbus_thp_reading_t structure to populate.
 * @param  timeout_ms Maximum response timeout in milliseconds.
 * @return STATUS_OK on success, or structured error code.
 */
status_t modbus_query_slave_thp(uint8_t slave_addr,
                                modbus_thp_reading_t *p_reading,
                                uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H */
