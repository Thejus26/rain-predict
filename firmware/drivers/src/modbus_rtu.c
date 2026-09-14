/**
 * @file    modbus_rtu.c
 * @brief   Modbus RTU Master frame generator, parser, CRC-16, and decoding implementation.
 */

#include "modbus_rtu.h"
#include "uart_bus.h"
#include <string.h>

#define MODBUS_CRC16_INIT_VAL   0xFFFFU
#define MODBUS_CRC16_POLYNOMIAL 0xA001U

/* ========================================================================== */
/* Precise Microsecond Delay Utility                                          */
/* ========================================================================== */

static void delay_us(uint32_t us)
{
    volatile uint32_t count = us * 12U;
    while (count > 0U) {
        count--;
    }
}

/* ========================================================================== */
/* Flash Lookup Table (512 Bytes in .rodata Flash)                            */
/* ========================================================================== */

static const uint16_t s_modbus_crc16_lut[256] = {
    0x0000U, 0xC0C1U, 0xC181U, 0x0140U, 0xC301U, 0x03C0U, 0x0280U, 0xC241U,
    0xC601U, 0x06C0U, 0x0780U, 0xC741U, 0x0500U, 0xC5C1U, 0xC481U, 0x0440U,
    0xCC01U, 0x0CC0U, 0x0D80U, 0xCD41U, 0x0F00U, 0xCFC1U, 0xCE81U, 0x0E40U,
    0x0A00U, 0xCAC1U, 0xCB81U, 0x0B40U, 0xC901U, 0x09C0U, 0x0880U, 0xC841U,
    0xD801U, 0x18C0U, 0x1980U, 0xD941U, 0x1B00U, 0xDBC1U, 0xDA81U, 0x1A40U,
    0x1E00U, 0xDEC1U, 0xDF81U, 0x1F40U, 0xDD01U, 0x1DC0U, 0x1C80U, 0xDC41U,
    0x1400U, 0xD4C1U, 0xD581U, 0x1540U, 0xD701U, 0x17C0U, 0x1680U, 0xD641U,
    0xD201U, 0x12C0U, 0x1380U, 0xD341U, 0x1100U, 0xD1C1U, 0xD081U, 0x1040U,
    0xF001U, 0x30C0U, 0x3180U, 0xF141U, 0x3300U, 0xF3C1U, 0xF281U, 0x3240U,
    0x3600U, 0xF6C1U, 0xF781U, 0x3740U, 0xF501U, 0x35C0U, 0x3480U, 0xF441U,
    0x3C00U, 0xFCC1U, 0xFD81U, 0x3D40U, 0xFF01U, 0x3FC0U, 0x3E80U, 0xFE41U,
    0xFA01U, 0x3AC0U, 0x3B80U, 0xFB41U, 0x3900U, 0xF9C1U, 0xF881U, 0x3840U,
    0x2800U, 0xE8C1U, 0xE981U, 0x2940U, 0xEB01U, 0x2BC0U, 0x2A80U, 0xEA41U,
    0xEE01U, 0x2EC0U, 0x2F80U, 0xEF41U, 0x2D00U, 0xEDC1U, 0xEC81U, 0x2C40U,
    0xE401U, 0x24C0U, 0x2580U, 0xE541U, 0x2700U, 0xE7C1U, 0xE681U, 0x2640U,
    0x2200U, 0xE2C1U, 0xE381U, 0x2340U, 0xE101U, 0x21C0U, 0x2080U, 0xE041U,
    0xA001U, 0x60C0U, 0x6180U, 0xA141U, 0x6300U, 0xA3C1U, 0xA281U, 0x6240U,
    0x6600U, 0xA6C1U, 0xA781U, 0x6740U, 0xA501U, 0x65C0U, 0x6480U, 0xA441U,
    0x6C00U, 0xACC1U, 0xAD81U, 0x6D40U, 0xAF01U, 0x6FC0U, 0x6E80U, 0xAE41U,
    0xAA01U, 0x6AC0U, 0x6B80U, 0xAB41U, 0x6900U, 0xA9C1U, 0xA881U, 0x6840U,
    0x7800U, 0xB8C1U, 0xB981U, 0x7940U, 0xBB01U, 0x7BC0U, 0x7A80U, 0xBA41U,
    0xBE01U, 0x7EC0U, 0x7F80U, 0xBF41U, 0x7D00U, 0xBDC1U, 0xBC81U, 0x7C40U,
    0xB401U, 0x74C0U, 0x7580U, 0xB541U, 0x7700U, 0xB7C1U, 0xB681U, 0x7640U,
    0x7200U, 0xB2C1U, 0xB381U, 0x7340U, 0xB101U, 0x71C0U, 0x7080U, 0xB041U,
    0x5000U, 0x90C1U, 0x9181U, 0x5140U, 0x9301U, 0x53C0U, 0x5280U, 0x9241U,
    0x9601U, 0x56C0U, 0x5780U, 0x9741U, 0x5500U, 0x95C1U, 0x9481U, 0x5440U,
    0x9C01U, 0x5CC0U, 0x5D80U, 0x9D41U, 0x5F00U, 0x9FC1U, 0x9E81U, 0x5E40U,
    0x5A00U, 0x9AC1U, 0x9B81U, 0x5B40U, 0x9901U, 0x59C0U, 0x5880U, 0x9841U,
    0x8801U, 0x48C0U, 0x4980U, 0x8941U, 0x4B00U, 0x8BC1U, 0x8A81U, 0x4A40U,
    0x4E00U, 0x8EC1U, 0x8F81U, 0x4F40U, 0x8D01U, 0x4DC0U, 0x4C80U, 0x8C41U,
    0x4400U, 0x84C1U, 0x8581U, 0x4540U, 0x8701U, 0x47C0U, 0x4680U, 0x8641U,
    0x8201U, 0x42C0U, 0x4380U, 0x8341U, 0x4100U, 0x81C1U, 0x8081U, 0x4040U
};

/* ========================================================================== */
/* Bitwise Calculation                                                        */
/* ========================================================================== */

uint16_t modbus_crc16_bitwise(const uint8_t *p_buffer, uint16_t length)
{
    if (p_buffer == NULL || length == 0U) {
        return 0x0000U;
    }

    uint16_t crc = MODBUS_CRC16_INIT_VAL;

    for (uint16_t pos = 0U; pos < length; pos++) {
        crc ^= (uint16_t)p_buffer[pos];

        for (uint8_t i = 8U; i != 0U; i--) {
            if ((crc & 0x0001U) != 0U) {
                crc >>= 1;
                crc ^= MODBUS_CRC16_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* ========================================================================== */
/* Table Lookup Calculation (Fast)                                            */
/* ========================================================================== */

uint16_t modbus_crc16_lut(const uint8_t *p_buffer, uint16_t length)
{
    if (p_buffer == NULL || length == 0U) {
        return 0x0000U;
    }

    uint16_t crc = MODBUS_CRC16_INIT_VAL;

    for (uint16_t i = 0U; i < length; i++) {
        uint8_t table_idx = (uint8_t)(crc ^ p_buffer[i]);
        crc = (crc >> 8) ^ s_modbus_crc16_lut[table_idx];
    }

    return crc;
}

/* ========================================================================== */
/* Default Wrapper (Maps to LUT for optimal performance)                       */
/* ========================================================================== */

uint16_t modbus_crc16(const uint8_t *p_buffer, uint16_t length)
{
    return modbus_crc16_lut(p_buffer, length);
}

/* ========================================================================== */
/* Streaming Update                                                           */
/* ========================================================================== */

uint16_t modbus_crc16_update(uint16_t current_crc, const uint8_t *p_data, uint16_t length)
{
    if (p_data == NULL || length == 0U) {
        return current_crc;
    }

    uint16_t crc = current_crc;

    for (uint16_t i = 0U; i < length; i++) {
        uint8_t table_idx = (uint8_t)(crc ^ p_data[i]);
        crc = (crc >> 8) ^ s_modbus_crc16_lut[table_idx];
    }

    return crc;
}

/* ========================================================================== */
/* Frame Validation Engine                                                    */
/* ========================================================================== */

bool modbus_validate_frame_crc(const uint8_t *p_frame, uint16_t frame_len)
{
    if (p_frame == NULL || frame_len < 3U) {
        return false;
    }

    /* Compute expected CRC over payload (excluding last 2 CRC bytes) */
    uint16_t payload_len  = frame_len - 2U;
    uint16_t expected_crc = modbus_crc16(p_frame, payload_len);

    /* Extract received Little-Endian CRC from trailing 2 bytes */
    uint16_t received_crc = (uint16_t)p_frame[payload_len] |
                            ((uint16_t)p_frame[payload_len + 1U] << 8);

    return (expected_crc == received_crc);
}

/* ========================================================================== */
/* Frame CRC Appender                                                         */
/* ========================================================================== */

status_t modbus_append_crc16(uint8_t *p_frame,
                             uint16_t data_len,
                             uint16_t max_buf_len,
                             uint16_t *p_total_len)
{
    if (p_frame == NULL || p_total_len == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (data_len == 0U) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    if (max_buf_len < (data_len + 2U)) {
        return STATUS_ERROR_BUFFER_OVERFLOW;
    }

    uint16_t crc = modbus_crc16(p_frame, data_len);

    /* Low-byte first (Little-Endian) */
    p_frame[data_len]      = (uint8_t)(crc & 0xFFU);
    p_frame[data_len + 1U] = (uint8_t)((crc >> 8) & 0xFFU);

    *p_total_len = data_len + 2U;

    return STATUS_OK;
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
    if (!modbus_validate_frame_crc(p_resp_buf, resp_len)) {
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

/* ========================================================================== */
/* Direction Pin Control (PA1)                                                */
/* ========================================================================== */

void modbus_set_direction_tx(void)
{
    (void)uart_bus_set_direction(UART_PORT_RS485, UART_DIR_TX);
}

void modbus_set_direction_rx(void)
{
    (void)uart_bus_set_direction(UART_PORT_RS485, UART_DIR_RX);
}

/* ========================================================================== */
/* Initialization                                                             */
/* ========================================================================== */

status_t modbus_rtu_init(void)
{
    /* Configure PA1 / default to RX listening mode */
    modbus_set_direction_rx();
    return STATUS_OK;
}

/* ========================================================================== */
/* Master Query Transaction Execution                                         */
/* ========================================================================== */

status_t modbus_query_slave_raw(uint8_t slave_addr,
                                uint16_t start_reg,
                                uint16_t reg_count,
                                uint16_t *p_reg_data_out,
                                uint32_t timeout_ms)
{
    if (p_reg_data_out == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    uint8_t  tx_frame[MODBUS_FC03_REQ_FRAME_SIZE];
    uint16_t tx_len = 0U;

    /* Step 1: Construct 8-byte FC03 Query Frame with CRC-16 */
    status_t status = modbus_build_read_holding_registers_req(slave_addr,
                                                              start_reg,
                                                              reg_count,
                                                              tx_frame,
                                                              sizeof(tx_frame),
                                                              &tx_len);
    if (status != STATUS_OK) {
        return status;
    }

    uint8_t  rx_buf[MODBUS_MAX_FRAME_SIZE];
    uint16_t rx_len = 0U;

#if defined(HAVE_STM32WLXX_HAL)
    /* Step 2: Flush UART RX Ring Buffer to discard stale noise */
    (void)uart_bus_flush(UART_PORT_RS485);
#endif

    /* Step 3: Assert DE=HIGH (Transmitter Mode) */
    modbus_set_direction_tx();

    /* Step 4: Pre-Transmission Guard Delay (25 µs) */
    delay_us(MODBUS_GUARD_TIME_PRE_US);

    /* Step 5: Transmit Request Frame */
    status = uart_bus_transmit(UART_PORT_RS485, tx_frame, tx_len, 50U);
    if (status != STATUS_OK) {
        modbus_set_direction_rx();
        return status;
    }

    /* Step 6: Post-Transmission Guard Delay (35 µs) */
    delay_us(MODBUS_GUARD_TIME_POST_US);

    /* Step 7: De-assert DE=LOW (Receiver Mode Active) */
    modbus_set_direction_rx();

    /* Step 8: Await Slave Response with Bounded Software Timeout */
    status = uart_bus_receive(UART_PORT_RS485,
                              rx_buf,
                              sizeof(rx_buf),
                              &rx_len,
                              timeout_ms);
    if (status != STATUS_OK) {
        return status;
    }

    /* Step 9: Parse Response Frame, Verify CRC, and Unpack 16-Bit Words */
    modbus_exception_t exception = MODBUS_EX_NONE;
    status = modbus_parse_read_holding_registers_resp(slave_addr,
                                                      reg_count,
                                                      rx_buf,
                                                      rx_len,
                                                      p_reg_data_out,
                                                      &exception);

    return status;
}

/* ========================================================================== */
/* High-Level Meteorological Telemetry Query                                  */
/* ========================================================================== */

status_t modbus_query_slave_thp(uint8_t slave_addr,
                                modbus_thp_reading_t *p_reading,
                                uint32_t timeout_ms)
{
    if (p_reading == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    uint16_t raw_regs[5] = {0};
    status_t status = STATUS_ERROR_TIMEOUT;

    /* Execute query with retry loop (up to 3 attempts) */
    for (uint8_t attempt = 0; attempt < MODBUS_MAX_RETRIES; attempt++) {
        /* Query 5 registers: Temp, Humidity, Pressure, Wind Speed, Wind Dir */
        status = modbus_query_slave_raw(slave_addr,
                                        MODBUS_REG_TEMPERATURE,
                                        5U,
                                        raw_regs,
                                        timeout_ms);
        if (status == STATUS_OK) {
            break;
        }

        /* Inter-frame delay before retry */
        delay_us(MODBUS_RETRY_DELAY_US);
    }

    if (status != STATUS_OK) {
        return status;
    }

    /* Decode raw 16-bit words into floating-point physical units */
    return modbus_decode_thp_registers(raw_regs, 5U, p_reading);
}
