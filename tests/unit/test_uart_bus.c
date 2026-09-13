/**
 * @file    test_uart_bus.c
 * @brief   Unit test verification suite for Bounded Dual-Port UART & SDI-12 Driver.
 * @details Validates USART1 RS-485 Modbus RTU, LPUART1 SDI-12, DE/RE direction guard timing,
 *          SDI-12 break/mark sequencing, static circular RX ring buffering, bounded timeouts,
 *          pre-sleep deinitialization, and defensive NULL pointer safety.
 */

#include "unity.h"
#include "uart_bus.h"
#include <string.h>

void setUp(void) {
    uart_bus_test_reset();
    (void)uart_bus_init(UART_PORT_RS485, 9600U, 8U, (uint8_t)UART_PARITY_NONE, 1U);
    (void)uart_bus_init(UART_PORT_SDI12, 1200U, 7U, (uint8_t)UART_PARITY_EVEN, 1U);
}

void tearDown(void) {
    uart_bus_test_clear_faults(UART_PORT_RS485);
    uart_bus_test_clear_faults(UART_PORT_SDI12);
}

/**
 * @brief TC-S3-T2.2-01 & TC-S3-T2.2-02: Dual-Port Initialization & Configuration Validation.
 */
static void test_uart_bus_dual_port_init(void) {
    /* 1. RS-485 Port Initialization (USART1, 9600 baud, 8N1) */
    status_t status = uart_bus_init(UART_PORT_RS485, 9600U, 8U, (uint8_t)UART_PARITY_NONE, 1U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(uart_bus_test_is_initialized(UART_PORT_RS485));
    TEST_ASSERT_EQUAL_UINT32(9600U, uart_bus_test_get_baud_rate(UART_PORT_RS485));
    TEST_ASSERT_EQUAL_UINT8(8U, uart_bus_test_get_data_bits(UART_PORT_RS485));
    TEST_ASSERT_EQUAL_UINT8(UART_PARITY_NONE, uart_bus_test_get_parity(UART_PORT_RS485));
    TEST_ASSERT_EQUAL_UINT8(1U, uart_bus_test_get_stop_bits(UART_PORT_RS485));

    /* 2. SDI-12 Port Initialization (LPUART1, 1200 baud, 7E1) */
    status = uart_bus_init(UART_PORT_SDI12, 1200U, 7U, (uint8_t)UART_PARITY_EVEN, 1U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(uart_bus_test_is_initialized(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT32(1200U, uart_bus_test_get_baud_rate(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(7U, uart_bus_test_get_data_bits(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(UART_PARITY_EVEN, uart_bus_test_get_parity(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(1U, uart_bus_test_get_stop_bits(UART_PORT_SDI12));

    /* 3. Parameter Boundary Violations */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_MAX, 9600U, 8U, 0U, 1U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_RS485, 0U, 8U, 0U, 1U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_RS485, 9600U, 5U, 0U, 1U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_RS485, 9600U, 9U, 0U, 1U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_RS485, 9600U, 8U, 3U, 1U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_init(UART_PORT_RS485, 9600U, 8U, 0U, 3U));
}

/**
 * @brief TC-S3-T2.2-03: RS-485 DE Direction Sequencing & Guard Timing.
 */
static void test_uart_bus_rs485_de_guard_timing(void) {
    const uint8_t modbus_query[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    uint8_t captured[16] = {0};

    uart_bus_test_set_auto_direction(true);

    status_t status = uart_bus_transmit(UART_PORT_RS485, modbus_query, sizeof(modbus_query), 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify DE guard timing values */
    TEST_ASSERT_EQUAL_UINT32(UART_BUS_RS485_PRE_DELAY_US, uart_bus_test_get_pre_delay_us());
    TEST_ASSERT_EQUAL_UINT32(UART_BUS_RS485_POST_DELAY_US, uart_bus_test_get_post_delay_us());

    /* Verify TX byte capture and contents */
    TEST_ASSERT_EQUAL_UINT16(sizeof(modbus_query), uart_bus_test_get_tx_count(UART_PORT_RS485));
    uint16_t copied = uart_bus_test_get_tx_bytes(UART_PORT_RS485, captured, sizeof(captured));
    TEST_ASSERT_EQUAL_UINT16(sizeof(modbus_query), copied);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(modbus_query, captured, sizeof(modbus_query));

    /* Verify post-transmission direction returned to RX listening mode */
    TEST_ASSERT_EQUAL_INT(UART_DIR_RX, uart_bus_test_get_direction(UART_PORT_RS485));
}

/**
 * @brief TC-S3-T2.2-04: Interrupt-Driven Circular RX Ring Buffering & Retrieval.
 */
static void test_uart_bus_rx_ring_buffering(void) {
    uint8_t test_stream[64];
    for (uint16_t i = 0; i < 64U; i++) {
        test_stream[i] = (uint8_t)(0xA0U + (uint8_t)i);
    }

    /* Inject 64 bytes into RS-485 ring buffer */
    status_t status = uart_bus_test_inject_rx(UART_PORT_RS485, test_stream, sizeof(test_stream));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(64U, uart_bus_get_available(UART_PORT_RS485));

    /* Read first chunk of 24 bytes */
    uint8_t rx_chunk1[32] = {0};
    uint16_t bytes_read1 = 0;
    status = uart_bus_receive(UART_PORT_RS485, rx_chunk1, 24U, &bytes_read1, 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(24U, bytes_read1);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(test_stream, rx_chunk1, 24U);
    TEST_ASSERT_EQUAL_UINT16(40U, uart_bus_get_available(UART_PORT_RS485));

    /* Read remaining chunk of 40 bytes */
    uint8_t rx_chunk2[48] = {0};
    uint16_t bytes_read2 = 0;
    status = uart_bus_receive(UART_PORT_RS485, rx_chunk2, 40U, &bytes_read2, 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(40U, bytes_read2);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&test_stream[24], rx_chunk2, 40U);
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_get_available(UART_PORT_RS485));
}

/**
 * @brief Circular Ring Buffer Pointer Wrap-Around Verification.
 */
static void test_uart_bus_ring_buffer_wrap_around(void) {
    uint8_t pattern[200];
    for (uint16_t i = 0; i < 200U; i++) {
        pattern[i] = (uint8_t)(i & 0xFFU);
    }

    /* 1. Inject 200 bytes */
    status_t status = uart_bus_test_inject_rx(UART_PORT_SDI12, pattern, 200U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* 2. Read 150 bytes, advancing tail to index 150 */
    uint8_t rx_temp[150];
    uint16_t read_len = 0;
    status = uart_bus_receive(UART_PORT_SDI12, rx_temp, 150U, &read_len, 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(150U, read_len);
    TEST_ASSERT_EQUAL_UINT16(50U, uart_bus_get_available(UART_PORT_SDI12));

    /* 3. Inject 150 bytes, wrapping head pointer across boundary (200 + 150) % 256 = 94 */
    uint8_t wrap_pattern[150];
    for (uint16_t i = 0; i < 150U; i++) {
        wrap_pattern[i] = (uint8_t)((i + 200U) & 0xFFU);
    }
    status = uart_bus_test_inject_rx(UART_PORT_SDI12, wrap_pattern, 150U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(200U, uart_bus_get_available(UART_PORT_SDI12));

    /* 4. Read all 200 bytes and verify continuous integrity across boundary */
    uint8_t rx_final[200];
    status = uart_bus_receive(UART_PORT_SDI12, rx_final, 200U, &read_len, 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(200U, read_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&pattern[150], rx_final, 50U);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(wrap_pattern, &rx_final[50], 150U);
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_get_available(UART_PORT_SDI12));
}

/**
 * @brief TC-S3-T2.2-05: SDI-12 Break and Mark Physical Wakeup Signaling.
 */
static void test_uart_bus_sdi12_break_and_mark_sequencing(void) {
    status_t status = uart_bus_sdi12_send_break();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(uart_bus_test_get_break_sent());
    TEST_ASSERT_EQUAL_UINT32(UART_BUS_SDI12_BREAK_MS, uart_bus_test_get_break_duration_ms());
    TEST_ASSERT_EQUAL_UINT32(UART_BUS_SDI12_MARK_MS, uart_bus_test_get_mark_duration_ms());

    /* Transmit standard SDI-12 measurement command frame */
    const char *sdi12_cmd = "0M!\r\n";
    status = uart_bus_transmit(UART_PORT_SDI12, (const uint8_t *)sdi12_cmd, (uint16_t)strlen(sdi12_cmd), 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    uint8_t captured[16] = {0};
    uint16_t copied = uart_bus_test_get_tx_bytes(UART_PORT_SDI12, captured, sizeof(captured));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(sdi12_cmd), copied);
    TEST_ASSERT_EQUAL_STRING(sdi12_cmd, (char *)captured);
}

/**
 * @brief TC-S3-T2.2-06: Bounded Non-Blocking Timeouts & Serial Fault Simulation.
 */
static void test_uart_bus_timeouts_and_faults(void) {
    uint8_t buf[16] = {0};
    uint16_t bytes_received = 0;

    /* 1. Empty buffer returns STATUS_ERR_TIMEOUT */
    status_t status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &bytes_received, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
    TEST_ASSERT_EQUAL_UINT16(0U, bytes_received);

    /* 2. Injected Timeout fault */
    (void)uart_bus_test_inject_rx_byte(UART_PORT_RS485, 0x11U);
    uart_bus_test_inject_fault(UART_PORT_RS485, 1U); /* TIMEOUT */
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &bytes_received, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);

    /* 3. Injected Framing Error fault */
    uart_bus_test_inject_fault(UART_PORT_RS485, 2U); /* FRAMING_ERROR */
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &bytes_received, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);

    /* 4. Injected Parity Error fault */
    uart_bus_test_inject_fault(UART_PORT_RS485, 3U); /* PARITY_ERROR */
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &bytes_received, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);

    /* 5. Injected Buffer Overflow fault */
    uart_bus_test_inject_fault(UART_PORT_RS485, 4U); /* BUFFER_OVERFLOW */
    status = uart_bus_receive(UART_PORT_RS485, buf, sizeof(buf), &bytes_received, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_OVERFLOW, status);

    /* 6. Injected TX Collision fault */
    uart_bus_test_clear_faults(UART_PORT_RS485);
    uart_bus_test_inject_fault(UART_PORT_RS485, 5U); /* TX_COLLISION */
    status = uart_bus_transmit(UART_PORT_RS485, buf, 4U, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_UART_BUS, status);

    /* 7. Clear faults restored */
    uart_bus_test_clear_faults(UART_PORT_RS485);
    status = uart_bus_transmit(UART_PORT_RS485, buf, 4U, 50U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
}

/**
 * @brief TC-S3-T2.2-07 & TC-S3-T2.2-08: Ring Buffer Flush & Low-Power Deinitialization.
 */
static void test_uart_bus_flush_and_deinit(void) {
    const uint8_t test_bytes[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    (void)uart_bus_test_inject_rx(UART_PORT_RS485, test_bytes, sizeof(test_bytes));
    TEST_ASSERT_EQUAL_UINT16(sizeof(test_bytes), uart_bus_get_available(UART_PORT_RS485));

    /* Flush ring buffer */
    status_t status = uart_bus_flush(UART_PORT_RS485);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_get_available(UART_PORT_RS485));

    /* De-initialize serial port for low-power sleep */
    status = uart_bus_deinit(UART_PORT_RS485);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(uart_bus_test_is_initialized(UART_PORT_RS485));

    /* Operations on deinitialized port return STATUS_ERR_INVALID_PARAM */
    uint8_t buf[8] = {0};
    uint16_t rx_len = 0;
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_transmit(UART_PORT_RS485, buf, 4U, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_receive(UART_PORT_RS485, buf, 4U, &rx_len, 50U));

    /* Re-initialize restores port */
    status = uart_bus_init(UART_PORT_RS485, 9600U, 8U, (uint8_t)UART_PARITY_NONE, 1U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(uart_bus_test_is_initialized(UART_PORT_RS485));
}

/**
 * @brief Defensive NULL Pointer & Boundary Safety Defense.
 */
static void test_uart_bus_null_ptr_and_boundary_defense(void) {
    uint8_t buf[8] = {0};
    uint16_t rx_len = 0;

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_transmit(UART_PORT_RS485, NULL, 4U, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_transmit(UART_PORT_RS485, buf, 0U, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_receive(UART_PORT_RS485, NULL, 4U, &rx_len, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_receive(UART_PORT_RS485, buf, 4U, NULL, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, uart_bus_test_inject_rx(UART_PORT_RS485, NULL, 4U));
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_test_get_tx_bytes(UART_PORT_RS485, NULL, 4U));

    /* Length 0 receive returns STATUS_OK with 0 bytes read */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, uart_bus_receive(UART_PORT_RS485, buf, 0U, &rx_len, 50U));
    TEST_ASSERT_EQUAL_UINT16(0U, rx_len);

    /* Invalid port bounds */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_deinit(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_set_direction(UART_PORT_MAX, UART_DIR_TX));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_transmit(UART_PORT_MAX, buf, 4U, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_receive(UART_PORT_MAX, buf, 4U, &rx_len, 50U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_flush(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_get_available(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_test_inject_rx(UART_PORT_MAX, buf, 4U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, uart_bus_test_inject_rx_byte(UART_PORT_MAX, 0x11U));
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_test_get_tx_bytes(UART_PORT_MAX, buf, 4U));
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_test_get_tx_count(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_UINT16(0U, uart_bus_test_get_rx_count(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_INT(UART_DIR_RX, uart_bus_test_get_direction(UART_PORT_MAX));
    TEST_ASSERT_FALSE(uart_bus_test_is_initialized(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_UINT32(0U, uart_bus_test_get_baud_rate(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_UINT8(0U, uart_bus_test_get_data_bits(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_UINT8(0U, uart_bus_test_get_parity(UART_PORT_MAX));
    TEST_ASSERT_EQUAL_UINT8(0U, uart_bus_test_get_stop_bits(UART_PORT_MAX));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uart_bus_dual_port_init);
    RUN_TEST(test_uart_bus_rs485_de_guard_timing);
    RUN_TEST(test_uart_bus_rx_ring_buffering);
    RUN_TEST(test_uart_bus_ring_buffer_wrap_around);
    RUN_TEST(test_uart_bus_sdi12_break_and_mark_sequencing);
    RUN_TEST(test_uart_bus_timeouts_and_faults);
    RUN_TEST(test_uart_bus_flush_and_deinit);
    RUN_TEST(test_uart_bus_null_ptr_and_boundary_defense);
    return UNITY_END();
}
