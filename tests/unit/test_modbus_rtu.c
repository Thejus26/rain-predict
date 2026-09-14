/**
 * @file    test_modbus_rtu.c
 * @brief   ThrowTheSwitch Unity unit test suite for Modbus RTU Driver (S4-T4.1).
 * @details Validates Modbus RTU FC03 frame generation, CRC-16 computation,
 *          response parsing, exception handling, parameter bounds checking,
 *          and meteorological unit decoding.
 */

#include "unity.h"
#include "modbus_rtu.h"
#include <string.h>

void setUp(void) {
    /* No hardware state to reset for pure protocol tests */
}

void tearDown(void) {
    /* Cleanup */
}

/**
 * @brief TC-S4-T4.1-01: Protocol Constants and Enum Definitions.
 */
static void test_modbus_constants_and_types(void) {
    TEST_ASSERT_EQUAL_HEX8(0x03U, MODBUS_FC03_READ_HOLDING_REGISTERS);
    TEST_ASSERT_EQUAL_UINT16(8U, MODBUS_FC03_REQ_FRAME_SIZE);
    TEST_ASSERT_EQUAL_UINT16(5U, MODBUS_MIN_RESP_FRAME_SIZE);
    TEST_ASSERT_EQUAL_UINT16(125U, MODBUS_MAX_READ_REGISTERS);
    TEST_ASSERT_EQUAL_UINT16(256U, MODBUS_MAX_FRAME_SIZE);
    TEST_ASSERT_EQUAL_HEX8(0x01U, MODBUS_DEFAULT_SLAVE_ADDR);
    TEST_ASSERT_EQUAL_HEX8(0x00U, MODBUS_BROADCAST_ADDR);
    TEST_ASSERT_EQUAL_HEX8(247U, MODBUS_MAX_SLAVE_ADDR);
    TEST_ASSERT_EQUAL_HEX8(0x80U, MODBUS_EXCEPTION_MASK);

    TEST_ASSERT_EQUAL_HEX16(0x0000U, MODBUS_REG_TEMPERATURE);
    TEST_ASSERT_EQUAL_HEX16(0x0001U, MODBUS_REG_HUMIDITY);
    TEST_ASSERT_EQUAL_HEX16(0x0002U, MODBUS_REG_PRESSURE);
    TEST_ASSERT_EQUAL_HEX16(0x0003U, MODBUS_REG_WIND_SPEED);
    TEST_ASSERT_EQUAL_HEX16(0x0004U, MODBUS_REG_WIND_DIRECTION);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.01f, MODBUS_SCALE_TEMP_C);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.01f, MODBUS_SCALE_HUMIDITY_PCT);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.10f, MODBUS_SCALE_PRESSURE_HPA);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.01f, MODBUS_SCALE_WIND_SPEED_MPS);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.10f, MODBUS_SCALE_WIND_DIR_DEG);
}

/**
 * @brief TC-S4-T4.1-02: CRC-16 Calculation with Known Modbus Test Vectors.
 */
static void test_modbus_crc16_calculation(void) {
    /* NULL pointer and 0 length defensive checks */
    TEST_ASSERT_EQUAL_HEX16(0x0000U, modbus_crc16(NULL, 10U));
    const uint8_t dummy = 0x01U;
    TEST_ASSERT_EQUAL_HEX16(0x0000U, modbus_crc16(&dummy, 0U));

    /* Vector 1: Standard 6-byte FC03 query [0x01, 0x03, 0x00, 0x00, 0x00, 0x03] -> CRC 0xCB05 */
    const uint8_t req1[] = {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x03U};
    uint16_t crc1 = modbus_crc16(req1, sizeof(req1));
    TEST_ASSERT_EQUAL_HEX16(0xCB05U, crc1);
    TEST_ASSERT_EQUAL_HEX8(0x05U, (uint8_t)(crc1 & 0xFFU));        /* CRC Lo */
    TEST_ASSERT_EQUAL_HEX8(0xCBU, (uint8_t)((crc1 >> 8) & 0xFFU)); /* CRC Hi */

    /* Vector 2: Multi-register FC03 query [0x02, 0x03, 0x00, 0x10, 0x00, 0x0A] -> CRC 0x3BC4 */
    const uint8_t req2[] = {0x02U, 0x03U, 0x00U, 0x10U, 0x00U, 0x0AU};
    uint16_t crc2 = modbus_crc16(req2, sizeof(req2));
    TEST_ASSERT_EQUAL_HEX16(0x3BC4U, crc2);
    TEST_ASSERT_EQUAL_HEX8(0xC4U, (uint8_t)(crc2 & 0xFFU));        /* CRC Lo */
    TEST_ASSERT_EQUAL_HEX8(0x3BU, (uint8_t)((crc2 >> 8) & 0xFFU)); /* CRC Hi */

    /* Vector 3: Exception response header [0x01, 0x83, 0x02] -> CRC 0xF1C0 */
    const uint8_t ex1[] = {0x01U, 0x83U, 0x02U};
    uint16_t crce = modbus_crc16(ex1, sizeof(ex1));
    TEST_ASSERT_EQUAL_HEX16(0xF1C0U, crce);
    TEST_ASSERT_EQUAL_HEX8(0xC0U, (uint8_t)(crce & 0xFFU));        /* CRC Lo */
    TEST_ASSERT_EQUAL_HEX8(0xF1U, (uint8_t)((crce >> 8) & 0xFFU)); /* CRC Hi */

    /* Vector 4: Normal 3-register response [0x01, 0x03, 0x06, 0x09, 0x94, 0x22, 0x92, 0x27, 0x94] -> CRC 0xFBA1 */
    const uint8_t resp1[] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x94U};
    uint16_t crcr = modbus_crc16(resp1, sizeof(resp1));
    TEST_ASSERT_EQUAL_HEX16(0xFBA1U, crcr);
    TEST_ASSERT_EQUAL_HEX8(0xA1U, (uint8_t)(crcr & 0xFFU));        /* CRC Lo */
    TEST_ASSERT_EQUAL_HEX8(0xFBU, (uint8_t)((crcr >> 8) & 0xFFU)); /* CRC Hi */
}

/**
 * @brief TC-S4-T4.1-03: Request Frame Builder NULL Pointer Checks.
 */
static void test_modbus_build_req_null_guards(void) {
    uint8_t buf[16];
    uint16_t frame_len = 0U;

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          modbus_build_read_holding_registers_req(1U, 0U, 3U, NULL, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          modbus_build_read_holding_registers_req(1U, 0U, 3U, buf, sizeof(buf), NULL));
}

/**
 * @brief TC-S4-T4.1-04: Request Frame Builder Slave Address Validation.
 */
static void test_modbus_build_req_slave_addr_validation(void) {
    uint8_t buf[16];
    uint16_t frame_len = 99U;

    /* Broadcast address 0x00 is prohibited for unicast read */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_build_read_holding_registers_req(0x00U, 0U, 3U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_UINT16(0U, frame_len);

    /* Address > 247 is prohibited */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_build_read_holding_registers_req(248U, 0U, 3U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_build_read_holding_registers_req(255U, 0U, 3U, buf, sizeof(buf), &frame_len));

    /* Valid boundary addresses 1 and 247 */
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(1U, 0U, 3U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_UINT16(8U, frame_len);

    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(247U, 0U, 3U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_UINT16(8U, frame_len);
}

/**
 * @brief TC-S4-T4.1-05: Request Frame Builder Register Count Validation.
 */
static void test_modbus_build_req_reg_count_validation(void) {
    uint8_t buf[16];
    uint16_t frame_len = 0U;

    /* Count = 0 is invalid */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_build_read_holding_registers_req(1U, 0U, 0U, buf, sizeof(buf), &frame_len));

    /* Count > 125 is invalid */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_build_read_holding_registers_req(1U, 0U, 126U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_build_read_holding_registers_req(1U, 0U, 500U, buf, sizeof(buf), &frame_len));

    /* Valid boundary counts 1 and 125 */
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(1U, 0U, 1U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(1U, 0U, 125U, buf, sizeof(buf), &frame_len));
}

/**
 * @brief TC-S4-T4.1-06: Request Frame Builder Buffer Capacity Bounds.
 */
static void test_modbus_build_req_buffer_size_guard(void) {
    uint8_t buf[16];
    uint16_t frame_len = 0U;

    for (uint16_t len = 0U; len < 8U; len++) {
        TEST_ASSERT_EQUAL_INT(STATUS_ERROR_BUFFER_OVERFLOW,
                              modbus_build_read_holding_registers_req(1U, 0U, 3U, buf, len, &frame_len));
    }

    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(1U, 0U, 3U, buf, 8U, &frame_len));
    TEST_ASSERT_EQUAL_UINT16(8U, frame_len);
}

/**
 * @brief TC-S4-T4.1-07: Valid Request Frame Generation (Exact Bytes and CRC).
 */
static void test_modbus_build_req_valid_frames(void) {
    uint8_t buf[16];
    uint16_t frame_len = 0U;

    /* TC-MB-01: Slave 1, Start 0x0000, Count 3 */
    memset(buf, 0, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(0x01U, 0x0000U, 3U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_UINT16(8U, frame_len);
    const uint8_t expected1[8] = {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x03U, 0x05U, 0xCBU};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected1, buf, 8U);

    /* TC-MB-02: Slave 2, Start 0x0010, Count 10 */
    memset(buf, 0, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(0x02U, 0x0010U, 10U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_UINT16(8U, frame_len);
    const uint8_t expected2[8] = {0x02U, 0x03U, 0x00U, 0x10U, 0x00U, 0x0AU, 0xC4U, 0x3BU};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected2, buf, 8U);

    /* High start address test: Slave 0x05, Start 0x1234, Count 1 */
    memset(buf, 0, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_build_read_holding_registers_req(0x05U, 0x1234U, 1U, buf, sizeof(buf), &frame_len));
    TEST_ASSERT_EQUAL_UINT16(8U, frame_len);
    TEST_ASSERT_EQUAL_HEX8(0x05U, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03U, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12U, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x34U, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00U, buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x01U, buf[5]);
    uint16_t crc = modbus_crc16(buf, 6U);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)(crc & 0xFFU), buf[6]);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)((crc >> 8) & 0xFFU), buf[7]);
}

/**
 * @brief TC-S4-T4.1-08: Response Parser NULL Pointer & Buffer Size Checks.
 */
static void test_modbus_parse_resp_null_and_bounds(void) {
    const uint8_t valid_resp[11] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x94U, 0xA1U, 0xFBU};
    uint16_t reg_data[8];
    modbus_exception_t ex = MODBUS_EX_NONE;

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          modbus_parse_read_holding_registers_resp(1U, 3U, NULL, sizeof(valid_resp), reg_data, &ex));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          modbus_parse_read_holding_registers_resp(1U, 3U, valid_resp, sizeof(valid_resp), NULL, &ex));

    /* NULL exception pointer is valid */
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_parse_read_holding_registers_resp(1U, 3U, valid_resp, sizeof(valid_resp), reg_data, NULL));

    /* Short frames (< 5 bytes) */
    for (uint16_t len = 0U; len < 5U; len++) {
        TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                              modbus_parse_read_holding_registers_resp(1U, 3U, valid_resp, len, reg_data, &ex));
    }
}

/**
 * @brief TC-S4-T4.1-09: Response Parser CRC Corruption Detection.
 */
static void test_modbus_parse_resp_crc_corruption(void) {
    uint8_t resp[11] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x94U, 0xA1U, 0xFBU};
    uint16_t reg_data[8];
    modbus_exception_t ex = MODBUS_EX_NONE;

    /* Valid check */
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, sizeof(resp), reg_data, &ex));

    /* Corrupt byte in payload */
    resp[4] ^= 0x01U;
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_CRC,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, sizeof(resp), reg_data, &ex));

    /* Corrupt CRC low byte */
    resp[4] ^= 0x01U; /* Restore */
    resp[9] ^= 0x55U;
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_CRC,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, sizeof(resp), reg_data, &ex));
}

/**
 * @brief TC-S4-T4.1-10: Response Parser Slave Address & Function Code Validation.
 */
static void test_modbus_parse_resp_address_and_fc_mismatch(void) {
    uint8_t resp[11] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x94U, 0x00U, 0x00U};
    uint16_t reg_data[8];
    modbus_exception_t ex = MODBUS_EX_NONE;

    /* Compute correct CRC */
    uint16_t crc = modbus_crc16(resp, 9U);
    resp[9] = (uint8_t)(crc & 0xFFU);
    resp[10] = (uint8_t)((crc >> 8) & 0xFFU);

    /* Expected slave mismatch (expected 2, got 1) */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          modbus_parse_read_holding_registers_resp(2U, 3U, resp, sizeof(resp), reg_data, &ex));

    /* Wrong function code (FC04 instead of FC03) */
    resp[1] = 0x04U;
    crc = modbus_crc16(resp, 9U);
    resp[9] = (uint8_t)(crc & 0xFFU);
    resp[10] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, sizeof(resp), reg_data, &ex));
}

/**
 * @brief TC-S4-T4.1-11: Response Parser Byte Count & Frame Length Mismatches.
 */
static void test_modbus_parse_resp_byte_count_mismatches(void) {
    uint8_t resp[11];
    uint16_t reg_data[8];
    modbus_exception_t ex = MODBUS_EX_NONE;

    /* Case A: Request was 3 registers (6 bytes expected), but response byte count says 4 */
    uint8_t resp_wrong_bc[9] = {0x01U, 0x03U, 0x04U, 0x09U, 0x94U, 0x22U, 0x92U, 0x00U, 0x00U};
    uint16_t crc = modbus_crc16(resp_wrong_bc, 7U);
    resp_wrong_bc[7] = (uint8_t)(crc & 0xFFU);
    resp_wrong_bc[8] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp_wrong_bc, sizeof(resp_wrong_bc), reg_data, &ex));

    /* Case B: Byte count is 6, but total buffer length is only 10 (truncated frame) */
    memcpy(resp, "\x01\x03\x06\x09\x94\x22\x92\x27\x94", 9);
    crc = modbus_crc16(resp, 9U);
    resp[9] = (uint8_t)(crc & 0xFFU);
    resp[10] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, 10U, reg_data, &ex));
}

/**
 * @brief TC-S4-T4.1-12: Modbus Exception Response Parsing (0x83).
 */
static void test_modbus_parse_resp_exception_frames(void) {
    uint16_t reg_data[8];
    modbus_exception_t ex = MODBUS_EX_NONE;

    /* TC-MB-07: Slave 1, Exception 0x02 (Illegal Data Address) [0x01, 0x83, 0x02, 0xC0, 0xF1] */
    const uint8_t ex_frame[5] = {0x01U, 0x83U, 0x02U, 0xC0U, 0xF1U};
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_MODBUS_EXCEPTION,
                          modbus_parse_read_holding_registers_resp(1U, 3U, ex_frame, sizeof(ex_frame), reg_data, &ex));
    TEST_ASSERT_EQUAL_INT(MODBUS_EX_ILLEGAL_DATA_ADDRESS, ex);

    /* Illegal Function Exception (0x01) */
    uint8_t ex_f1[5] = {0x02U, 0x83U, 0x01U, 0x00U, 0x00U};
    uint16_t crc = modbus_crc16(ex_f1, 3U);
    ex_f1[3] = (uint8_t)(crc & 0xFFU);
    ex_f1[4] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_MODBUS_EXCEPTION,
                          modbus_parse_read_holding_registers_resp(2U, 1U, ex_f1, sizeof(ex_f1), reg_data, &ex));
    TEST_ASSERT_EQUAL_INT(MODBUS_EX_ILLEGAL_FUNCTION, ex);

    /* Illegal Data Value Exception (0x03) */
    uint8_t ex_f3[5] = {0x01U, 0x83U, 0x03U, 0x00U, 0x00U};
    crc = modbus_crc16(ex_f3, 3U);
    ex_f3[3] = (uint8_t)(crc & 0xFFU);
    ex_f3[4] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_MODBUS_EXCEPTION,
                          modbus_parse_read_holding_registers_resp(1U, 3U, ex_f3, sizeof(ex_f3), reg_data, &ex));
    TEST_ASSERT_EQUAL_INT(MODBUS_EX_ILLEGAL_DATA_VALUE, ex);

    /* Slave Device Failure Exception (0x04) */
    uint8_t ex_f4[5] = {0x01U, 0x83U, 0x04U, 0x00U, 0x00U};
    crc = modbus_crc16(ex_f4, 3U);
    ex_f4[3] = (uint8_t)(crc & 0xFFU);
    ex_f4[4] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_MODBUS_EXCEPTION,
                          modbus_parse_read_holding_registers_resp(1U, 3U, ex_f4, sizeof(ex_f4), reg_data, &ex));
    TEST_ASSERT_EQUAL_INT(MODBUS_EX_SLAVE_DEVICE_FAILURE, ex);

    /* Malformed Exception frame length (6 bytes instead of 5) */
    uint8_t malformed_ex[6] = {0x01U, 0x83U, 0x02U, 0x00U, 0x00U, 0x00U};
    crc = modbus_crc16(malformed_ex, 4U);
    malformed_ex[4] = (uint8_t)(crc & 0xFFU);
    malformed_ex[5] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          modbus_parse_read_holding_registers_resp(1U, 3U, malformed_ex, sizeof(malformed_ex), reg_data, &ex));
}

/**
 * @brief TC-S4-T4.1-13: Normal Response Parsing and Big-Endian Unpacking.
 */
static void test_modbus_parse_resp_success_unpacking(void) {
    /* TC-MB-06: 3 registers response */
    const uint8_t resp[11] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x94U, 0xA1U, 0xFBU};
    uint16_t reg_data[8] = {0};
    modbus_exception_t ex = MODBUS_EX_ILLEGAL_FUNCTION;

    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, sizeof(resp), reg_data, &ex));
    TEST_ASSERT_EQUAL_INT(MODBUS_EX_NONE, ex);
    TEST_ASSERT_EQUAL_HEX16(0x0994U, reg_data[0]); /* 2452 */
    TEST_ASSERT_EQUAL_HEX16(0x2292U, reg_data[1]); /* 8850 */
    TEST_ASSERT_EQUAL_HEX16(0x2794U, reg_data[2]); /* 10132 */

    /* 5 registers response (THP + Wind) */
    uint8_t resp5[15] = {
        0x01U, 0x03U, 0x0AU,
        0x08U, 0x34U,   /* T = 2100 (21.00 C) */
        0x1BU, 0x58U,   /* RH = 7000 (70.00 %) */
        0x27U, 0x10U,   /* P = 10000 (1000.0 hPa) */
        0x01U, 0xF4U,   /* Wind Speed = 500 (5.00 m/s) */
        0x0AU, 0x8CU,   /* Wind Dir = 2700 (270.0 deg) */
        0x00U, 0x00U    /* CRC */
    };
    uint16_t crc = modbus_crc16(resp5, 13U);
    resp5[13] = (uint8_t)(crc & 0xFFU);
    resp5[14] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          modbus_parse_read_holding_registers_resp(1U, 5U, resp5, sizeof(resp5), reg_data, &ex));
    TEST_ASSERT_EQUAL_HEX16(0x0834U, reg_data[0]);
    TEST_ASSERT_EQUAL_HEX16(0x1B58U, reg_data[1]);
    TEST_ASSERT_EQUAL_HEX16(0x2710U, reg_data[2]);
    TEST_ASSERT_EQUAL_HEX16(0x01F4U, reg_data[3]);
    TEST_ASSERT_EQUAL_HEX16(0x0A8CU, reg_data[4]);
}

/**
 * @brief TC-S4-T4.1-14: Meteorological Register Decoding Defensive Checks.
 */
static void test_modbus_decode_thp_null_and_bounds(void) {
    const uint16_t reg_data[5] = {2452U, 8850U, 10132U, 450U, 1800U};
    modbus_thp_reading_t reading;

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          modbus_decode_thp_registers(NULL, 5U, &reading));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          modbus_decode_thp_registers(reg_data, 5U, NULL));

    /* Fewer than 3 registers is invalid */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_decode_thp_registers(reg_data, 0U, &reading));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_decode_thp_registers(reg_data, 1U, &reading));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          modbus_decode_thp_registers(reg_data, 2U, &reading));
}

/**
 * @brief TC-S4-T4.1-15: Meteorological Decoding (TC-MB-10 Standard Vector).
 */
static void test_modbus_decode_thp_standard_values(void) {
    const uint16_t reg_data[5] = {2452U, 8850U, 10132U, 450U, 1800U};
    modbus_thp_reading_t reading;
    memset(&reading, 0, sizeof(reading));

    TEST_ASSERT_EQUAL_INT(STATUS_OK, modbus_decode_thp_registers(reg_data, 5U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.52f, reading.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 88.50f, reading.humidity_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.2f, reading.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.50f, reading.wind_speed_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, reading.wind_direction_deg);
    TEST_ASSERT_TRUE(reading.has_wind_data);
}

/**
 * @brief TC-S4-T4.1-16: Meteorological Decoding Sub-Zero Temperature & Clamping.
 */
static void test_modbus_decode_thp_subzero_and_clamping(void) {
    /* Negative temperature: -10.50 C = -1050 = 0xFBE6 */
    uint16_t reg_data[5] = {(uint16_t)-1050, 9500U, 10250U, 0U, 0U};
    modbus_thp_reading_t reading;

    TEST_ASSERT_EQUAL_INT(STATUS_OK, modbus_decode_thp_registers(reg_data, 3U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.50f, reading.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 95.00f, reading.humidity_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1025.0f, reading.pressure_hpa);
    TEST_ASSERT_FALSE(reading.has_wind_data);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, reading.wind_speed_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, reading.wind_direction_deg);

    /* Extreme Sub-Zero: -40.00 C = -4000 = 0xF060 */
    reg_data[0] = (uint16_t)-4000;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, modbus_decode_thp_registers(reg_data, 3U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.00f, reading.temperature_c);

    /* Extreme High Temp: +85.00 C = +8500 */
    reg_data[0] = 8500U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, modbus_decode_thp_registers(reg_data, 3U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 85.00f, reading.temperature_c);

    /* Humidity over-range clamping (12000 -> 120.00% -> clamped to 100.0%) */
    reg_data[1] = 12000U;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, modbus_decode_thp_registers(reg_data, 3U, &reading));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, reading.humidity_pct);
}

/**
 * @brief TC-S4-T4.1-17: Exception String Lookups.
 */
static void test_modbus_exception_to_str(void) {
    TEST_ASSERT_EQUAL_STRING("No Exception", modbus_exception_to_str(MODBUS_EX_NONE));
    TEST_ASSERT_EQUAL_STRING("Illegal Function Code (0x01)", modbus_exception_to_str(MODBUS_EX_ILLEGAL_FUNCTION));
    TEST_ASSERT_EQUAL_STRING("Illegal Data Address (0x02)", modbus_exception_to_str(MODBUS_EX_ILLEGAL_DATA_ADDRESS));
    TEST_ASSERT_EQUAL_STRING("Illegal Data Value (0x03)", modbus_exception_to_str(MODBUS_EX_ILLEGAL_DATA_VALUE));
    TEST_ASSERT_EQUAL_STRING("Slave Device Failure (0x04)", modbus_exception_to_str(MODBUS_EX_SLAVE_DEVICE_FAILURE));
    TEST_ASSERT_EQUAL_STRING("Acknowledge (0x05)", modbus_exception_to_str(MODBUS_EX_ACKNOWLEDGE));
    TEST_ASSERT_EQUAL_STRING("Slave Device Busy (0x06)", modbus_exception_to_str(MODBUS_EX_SLAVE_DEVICE_BUSY));
    TEST_ASSERT_EQUAL_STRING("Negative Acknowledge (0x07)", modbus_exception_to_str(MODBUS_EX_NEGATIVE_ACKNOWLEDGE));
    TEST_ASSERT_EQUAL_STRING("Memory Parity Error (0x08)", modbus_exception_to_str(MODBUS_EX_MEMORY_PARITY_ERROR));
    TEST_ASSERT_EQUAL_STRING("Unknown Exception", modbus_exception_to_str((modbus_exception_t)0x99));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_modbus_constants_and_types);
    RUN_TEST(test_modbus_crc16_calculation);
    RUN_TEST(test_modbus_build_req_null_guards);
    RUN_TEST(test_modbus_build_req_slave_addr_validation);
    RUN_TEST(test_modbus_build_req_reg_count_validation);
    RUN_TEST(test_modbus_build_req_buffer_size_guard);
    RUN_TEST(test_modbus_build_req_valid_frames);
    RUN_TEST(test_modbus_parse_resp_null_and_bounds);
    RUN_TEST(test_modbus_parse_resp_crc_corruption);
    RUN_TEST(test_modbus_parse_resp_address_and_fc_mismatch);
    RUN_TEST(test_modbus_parse_resp_byte_count_mismatches);
    RUN_TEST(test_modbus_parse_resp_exception_frames);
    RUN_TEST(test_modbus_parse_resp_success_unpacking);
    RUN_TEST(test_modbus_decode_thp_null_and_bounds);
    RUN_TEST(test_modbus_decode_thp_standard_values);
    RUN_TEST(test_modbus_decode_thp_subzero_and_clamping);
    RUN_TEST(test_modbus_exception_to_str);

    return UNITY_END();
}
