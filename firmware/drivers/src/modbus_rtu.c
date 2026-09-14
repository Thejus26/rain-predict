/**
 * @file    modbus_rtu.c
 * @brief   Modbus RTU Master frame generator, parser, CRC-16, and decoding implementation.
 */

#include "modbus_rtu.h"
#include <string.h>

/* ========================================================================== */
/* CRC-16 Standard Modbus Calculation (Polynomial 0xA001)                     */
/* ========================================================================== */

uint16_t modbus_crc16(const uint8_t *p_buffer, uint16_t length)
{
    if (p_buffer == NULL || length == 0U) {
        return 0x0000U;
    }

    uint16_t crc = 0xFFFFU;

    for (uint16_t pos = 0U; pos < length; pos++) {
        crc ^= (uint16_t)p_buffer[pos];

        for (uint8_t i = 8U; i != 0U; i--) {
            if ((crc & 0x0001U) != 0U) {
                crc >>= 1;
                crc ^= 0xA001U;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* ========================================================================== */
/* Request Frame Construction                                                 */
/* ========================================================================== */

status_t modbus_build_read_holding_registers_req(uint8_t slave_addr,
                                                 uint16_t start_reg,
                                                 uint16_t reg_count,
                                                 uint8_t *p_out_buf,
                                                 uint16_t max_out_len,
                                                 uint16_t *p_frame_len)
{
    if (p_out_buf == NULL || p_frame_len == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    *p_frame_len = 0U;

    /* Validate Slave Address: Unicast 1..247 (Address 0 is Broadcast, invalid for reads) */
    if (slave_addr == MODBUS_BROADCAST_ADDR || slave_addr > MODBUS_MAX_SLAVE_ADDR) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    /* Validate Register Count: 1..125 */
    if (reg_count == 0U || reg_count > MODBUS_MAX_READ_REGISTERS) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    /* Ensure Destination Buffer Capacity */
    if (max_out_len < MODBUS_FC03_REQ_FRAME_SIZE) {
        return STATUS_ERROR_BUFFER_OVERFLOW;
    }

    /* Byte 0: Slave Address */
    p_out_buf[0] = slave_addr;

    /* Byte 1: Function Code 0x03 */
    p_out_buf[1] = MODBUS_FC03_READ_HOLDING_REGISTERS;

    /* Bytes 2-3: Starting Address (Big-Endian) */
    p_out_buf[2] = (uint8_t)((start_reg >> 8) & 0xFFU);
    p_out_buf[3] = (uint8_t)(start_reg & 0xFFU);

    /* Bytes 4-5: Number of Registers (Big-Endian) */
    p_out_buf[4] = (uint8_t)((reg_count >> 8) & 0xFFU);
    p_out_buf[5] = (uint8_t)(reg_count & 0xFFU);

    /* Bytes 6-7: CRC-16 Checksum (Low byte first in Modbus stream) */
    uint16_t crc = modbus_crc16(p_out_buf, 6U);
    p_out_buf[6] = (uint8_t)(crc & 0xFFU);         /* CRC Low */
    p_out_buf[7] = (uint8_t)((crc >> 8) & 0xFFU);  /* CRC High */

    *p_frame_len = MODBUS_FC03_REQ_FRAME_SIZE;

    return STATUS_OK;
}

/* ========================================================================== */
/* Response Frame Parsing & Unpacking                                         */
/* ========================================================================== */

status_t modbus_parse_read_holding_registers_resp(uint8_t expected_slave,
                                                  uint16_t expected_reg_count,
                                                  const uint8_t *p_resp_buf,
                                                  uint16_t resp_len,
                                                  uint16_t *p_reg_data_out,
                                                  modbus_exception_t *p_exception_out)
{
    if (p_resp_buf == NULL || p_reg_data_out == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (p_exception_out != NULL) {
        *p_exception_out = MODBUS_EX_NONE;
    }

    /* Minimum Modbus response frame length is 5 bytes (Exception frame) */
    if (resp_len < MODBUS_MIN_RESP_FRAME_SIZE) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify CRC Checksum over entire received frame */
    uint16_t expected_crc = modbus_crc16(p_resp_buf, resp_len - 2U);
    uint16_t received_crc = (uint16_t)p_resp_buf[resp_len - 2U] |
                            ((uint16_t)p_resp_buf[resp_len - 1U] << 8);

    if (expected_crc != received_crc) {
        return STATUS_ERROR_CRC;
    }

    /* Verify Slave Address */
    if (p_resp_buf[0] != expected_slave) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Check for Modbus Exception Response (Function Code | 0x80) */
    if (p_resp_buf[1] == (MODBUS_FC03_READ_HOLDING_REGISTERS | MODBUS_EXCEPTION_MASK)) {
        if (resp_len != MODBUS_MIN_RESP_FRAME_SIZE) {
            return STATUS_ERROR_INVALID_FRAME;
        }
        if (p_exception_out != NULL) {
            *p_exception_out = (modbus_exception_t)p_resp_buf[2];
        }
        return STATUS_ERROR_MODBUS_EXCEPTION;
    }

    /* Verify Normal Function Code 0x03 */
    if (p_resp_buf[1] != MODBUS_FC03_READ_HOLDING_REGISTERS) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify Byte Count: must equal expected_reg_count * 2 */
    uint8_t byte_count = p_resp_buf[2];
    uint16_t expected_bytes = expected_reg_count * 2U;

    if (byte_count != (uint8_t)expected_bytes) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Total expected frame size: 3 header bytes + byte_count + 2 CRC bytes */
    if (resp_len != (uint16_t)(3U + byte_count + 2U)) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Unpack 16-bit Big-Endian Register Words */
    for (uint16_t k = 0U; k < expected_reg_count; k++) {
        uint16_t byte_offset = 3U + (k * 2U);
        p_reg_data_out[k] = ((uint16_t)p_resp_buf[byte_offset] << 8) |
                            (uint16_t)p_resp_buf[byte_offset + 1U];
    }

    return STATUS_OK;
}

/* ========================================================================== */
/* Meteorological Engineering Units Decoding                                 */
/* ========================================================================== */

status_t modbus_decode_thp_registers(const uint16_t *p_reg_data,
                                     uint16_t reg_count,
                                     modbus_thp_reading_t *p_reading)
{
    if (p_reg_data == NULL || p_reading == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (reg_count < 3U) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    /* Decode Temperature: Signed 16-bit in 0.01 deg C */
    int16_t raw_temp = (int16_t)p_reg_data[0];
    p_reading->temperature_c = (float)raw_temp * MODBUS_SCALE_TEMP_C;

    /* Decode Humidity: Unsigned 16-bit in 0.01 % RH */
    p_reading->humidity_pct = (float)p_reg_data[1] * MODBUS_SCALE_HUMIDITY_PCT;

    /* Decode Pressure: Unsigned 16-bit in 0.1 hPa */
    p_reading->pressure_hpa = (float)p_reg_data[2] * MODBUS_SCALE_PRESSURE_HPA;

    /* Meteorological Boundary Clamping */
    if (p_reading->humidity_pct < 0.0f) {
        p_reading->humidity_pct = 0.0f;
    } else if (p_reading->humidity_pct > 100.0f) {
        p_reading->humidity_pct = 100.0f;
    }

    /* Optional Wind Speed & Direction (Registers 3 and 4) */
    if (reg_count >= 5U) {
        p_reading->wind_speed_mps     = (float)p_reg_data[3] * MODBUS_SCALE_WIND_SPEED_MPS;
        p_reading->wind_direction_deg = (float)p_reg_data[4] * MODBUS_SCALE_WIND_DIR_DEG;
        p_reading->has_wind_data      = true;
    } else {
        p_reading->wind_speed_mps     = 0.0f;
        p_reading->wind_direction_deg = 0.0f;
        p_reading->has_wind_data      = false;
    }

    return STATUS_OK;
}

const char *modbus_exception_to_str(modbus_exception_t exception_code)
{
    switch (exception_code) {
        case MODBUS_EX_NONE:                 return "No Exception";
        case MODBUS_EX_ILLEGAL_FUNCTION:     return "Illegal Function Code (0x01)";
        case MODBUS_EX_ILLEGAL_DATA_ADDRESS: return "Illegal Data Address (0x02)";
        case MODBUS_EX_ILLEGAL_DATA_VALUE:   return "Illegal Data Value (0x03)";
        case MODBUS_EX_SLAVE_DEVICE_FAILURE: return "Slave Device Failure (0x04)";
        case MODBUS_EX_ACKNOWLEDGE:          return "Acknowledge (0x05)";
        case MODBUS_EX_SLAVE_DEVICE_BUSY:    return "Slave Device Busy (0x06)";
        case MODBUS_EX_NEGATIVE_ACKNOWLEDGE: return "Negative Acknowledge (0x07)";
        case MODBUS_EX_MEMORY_PARITY_ERROR:  return "Memory Parity Error (0x08)";
        default:                             return "Unknown Exception";
    }
}
