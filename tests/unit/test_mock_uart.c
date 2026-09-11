/**
 * @file    test_mock_uart.c
 * @brief   Unit test verification suite for Mock UART Bus and Serial Fault Engine.
 * @details Validates Modbus RTU, SDI-12 emulation, RS-485 direction guards, and fault injection.
 */

#include "unity.h"
#include "mock_uart_bus.h"
#include <string.h>

void setUp(void) {
    mock_uart_reset();
}

void tearDown(void) {
    mock_uart_clear_faults(UART_PORT_RS485);
    mock_uart_clear_faults(UART_PORT_SDI12);
}

/**
 * @brief TC-S1-T2.3-01: Verify Modbus RTU RX frame injection and retrieval fidelity.
 */
static void test_mock_uart_rx_injection_and_retrieval(void) {
    /* Modbus RTU FC03 Response: Slave 0x01, FC 0x03, 4 bytes data, CRC-16 */
    const uint8_t modbus_resp[] = {0x01, 0x03, 0x04, 0x01, 0x2C, 0x03, 0xE8, 0x7B, 0x92};
    uint8_t rx_buf[16] = {0};
    uint16_t bytes_received = 0;

    status_t status = mock_uart_inject_rx(UART_PORT_RS485, modbus_resp, sizeof(modbus_resp));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(sizeof(modbus_resp), mock_uart_get_rx_count(UART_PORT_RS485));

    status = uart_bus_receive(UART_PORT_RS485, rx_buf, sizeof(rx_buf), &bytes_received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(sizeof(modbus_resp), bytes_received);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(modbus_resp, rx_buf, sizeof(modbus_resp));
    TEST_ASSERT_EQUAL_UINT16(0, mock_uart_get_rx_count(UART_PORT_RS485));
}

/**
 * @brief TC-S1-T2.3-02: Verify TX query frame capture with RS-485 transmitter enabled.
 */
static void test_mock_uart_tx_capture(void) {
    /* Modbus RTU FC03 Query: Slave 0x01, FC 0x03, Start 0x0000, 2 Regs, CRC-16 */
    const uint8_t query_frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    uint8_t captured_frame[16] = {0};

    /* Enable transmitter (DE/RE high) */
    status_t status = uart_bus_set_direction(UART_PORT_RS485, UART_DIR_TX);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(UART_DIR_TX, mock_uart_get_direction(UART_PORT_RS485));

    status = uart_bus_transmit(UART_PORT_RS485, query_frame, sizeof(query_frame), 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(sizeof(query_frame), mock_uart_get_tx_count(UART_PORT_RS485));

    uint16_t copied = mock_uart_get_tx_bytes(UART_PORT_RS485, captured_frame, sizeof(captured_frame));
    TEST_ASSERT_EQUAL_UINT16(sizeof(query_frame), copied);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(query_frame, captured_frame, sizeof(query_frame));
}

/**
 * @brief TC-S1-T2.3-03: Verify RS-485 transmission rejection when transceiver DE is low (RX mode).
 */
static void test_mock_uart_rs485_direction_guard(void) {
    const uint8_t query_frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};

    /* Default direction is UART_DIR_RX */
    TEST_ASSERT_EQUAL_INT(UART_DIR_RX, mock_uart_get_direction(UART_PORT_RS485));

    /* Attempt transmit without setting UART_DIR_TX */
    status_t status = uart_bus_transmit(UART_PORT_RS485, query_frame, sizeof(query_frame), 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);
    TEST_ASSERT_EQUAL_UINT16(0, mock_uart_get_tx_count(UART_PORT_RS485));
}

/**
 * @brief TC-S1-T2.3-04: Verify SDI-12 ASCII protocol command/response emulation.
 */
static void test_mock_uart_sdi12_ascii_framing(void) {
    const char *sdi12_response = "0+24.50+88.20+1013.25\r\n";
    uint16_t resp_len = (uint16_t)strlen(sdi12_response);
    uint8_t rx_buf[64] = {0};
    uint16_t bytes_received = 0;

    status_t status = mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)sdi12_response, resp_len);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    status = uart_bus_receive(UART_PORT_SDI12, rx_buf, sizeof(rx_buf) - 1, &bytes_received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(resp_len, bytes_received);
    TEST_ASSERT_EQUAL_STRING(sdi12_response, (char *)rx_buf);
}

/**
 * @brief TC-S1-T2.3-05: Verify timeout and hardware fault simulation engine.
 */
static void test_mock_uart_fault_injection(void) {
    uint8_t buf[16] = {0};
    uint16_t received = 0;

    /* 1. Unpopulated RX FIFO returns STATUS_ERR_TIMEOUT */
    status_t status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);

    /* 2. Explicit TIMEOUT fault */
    (void)mock_uart_inject_rx_byte(UART_PORT_RS485, 0x55);
    mock_uart_inject_fault(UART_PORT_RS485, MOCK_UART_FAULT_TIMEOUT);
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);

    /* 3. Framing error fault */
    mock_uart_inject_fault(UART_PORT_RS485, MOCK_UART_FAULT_FRAMING_ERROR);
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);

    /* 4. Parity error fault */
    mock_uart_inject_fault(UART_PORT_RS485, MOCK_UART_FAULT_PARITY_ERROR);
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);

    /* 5. Buffer overflow fault */
    mock_uart_inject_fault(UART_PORT_RS485, MOCK_UART_FAULT_BUFFER_OVERFLOW);
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &received, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_OVERFLOW, status);

    /* 6. TX Collision fault */
    mock_uart_clear_faults(UART_PORT_RS485);
    (void)uart_bus_set_direction(UART_PORT_RS485, UART_DIR_TX);
    mock_uart_inject_fault(UART_PORT_RS485, MOCK_UART_FAULT_TX_COLLISION);
    status = uart_bus_transmit(UART_PORT_RS485, buf, 4, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);
}

/**
 * @brief TC-S1-T2.3-06: Verify buffer flushing, reset isolation, and port parameter bounds.
 */
static void test_mock_uart_flush_and_reset_isolation(void) {
    const uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};

    /* Inject on RS485 and SDI12 */
    (void)mock_uart_inject_rx(UART_PORT_RS485, test_data, sizeof(test_data));
    (void)mock_uart_inject_rx(UART_PORT_SDI12, test_data, sizeof(test_data));
    (void)uart_bus_set_direction(UART_PORT_RS485, UART_DIR_TX);

    TEST_ASSERT_EQUAL_UINT16(sizeof(test_data), mock_uart_get_rx_count(UART_PORT_RS485));
    TEST_ASSERT_EQUAL_UINT16(sizeof(test_data), mock_uart_get_rx_count(UART_PORT_SDI12));

    /* Flush RS485 only */
    status_t status = uart_bus_flush(UART_PORT_RS485);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(0, mock_uart_get_rx_count(UART_PORT_RS485));
    TEST_ASSERT_EQUAL_UINT16(sizeof(test_data), mock_uart_get_rx_count(UART_PORT_SDI12));

    /* Full reset */
    mock_uart_reset();
    TEST_ASSERT_EQUAL_UINT16(0, mock_uart_get_rx_count(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_INT(UART_DIR_RX, mock_uart_get_direction(UART_PORT_RS485));
}

/**
 * @brief Verify pointer safety and parameter boundary enforcement.
 */
static void test_mock_uart_null_ptr_and_bounds_guards(void) {
    uint8_t buf[4] = {0};
    uint16_t received = 0;

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_transmit(UART_PORT_RS485, NULL, 4, 100));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_receive(UART_PORT_RS485, NULL, 4, &received, 100));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_receive(UART_PORT_RS485, buf, 4, NULL, 100));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, mock_uart_inject_rx(UART_PORT_RS485, NULL, 4));

    /* Invalid Port */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_MAX, 9600, 8, 0, 1));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_set_direction(UART_PORT_MAX, UART_DIR_TX));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_transmit(UART_PORT_MAX, buf, 4, 100));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_receive(UART_PORT_MAX, buf, 4, &received, 100));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_flush(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, mock_uart_inject_rx_byte(UART_PORT_MAX, 0x00));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, mock_uart_inject_rx(UART_PORT_MAX, buf, 4));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mock_uart_rx_injection_and_retrieval);
    RUN_TEST(test_mock_uart_tx_capture);
    RUN_TEST(test_mock_uart_rs485_direction_guard);
    RUN_TEST(test_mock_uart_sdi12_ascii_framing);
    RUN_TEST(test_mock_uart_fault_injection);
    RUN_TEST(test_mock_uart_flush_and_reset_isolation);
    RUN_TEST(test_mock_uart_null_ptr_and_bounds_guards);
    return UNITY_END();
}
