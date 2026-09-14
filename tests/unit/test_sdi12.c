/**
 * @file    test_sdi12.c
 * @brief   ThrowTheSwitch Unity unit test suite for SDI-12 Agricultural Bus Driver (S4-T5.1).
 * @details Validates break/mark timing, direction toggling, command transmission,
 *          and ASCII response acquisition under host simulation with mock UART & GPIO.
 */

#include "unity.h"
#include "sdi12_driver.h"
#include "uart_bus.h"
#include "gpio_driver.h"
#include "mock_uart_bus.h"
#include "mock_gpio.h"
#include "board_config.h"
#include "status.h"
#include <string.h>

/* ============================================================================
 * Test Setup & Teardown
 * ============================================================================ */

void setUp(void) {
    mock_uart_reset();
    mock_gpio_reset();
    uart_bus_test_reset();
    sdi12_test_reset();
    (void)sdi12_init();
}

void tearDown(void) {
    /* Verify direction pin is always restored to RX mode (PC2 = LOW) */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
}

/* ============================================================================
 * Physical Layer & Direction Control Tests (Task S4-T5.1)
 * ============================================================================ */

static void test_sdi12_init_defaults(void) {
    /* Verify direction pin PC2 is initialized to LOW (RX Mode) */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
    TEST_ASSERT_TRUE(uart_bus_test_is_initialized(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT32(1200U, uart_bus_test_get_baud_rate(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(7U, uart_bus_test_get_data_bits(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)UART_PARITY_EVEN, uart_bus_test_get_parity(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(1U, uart_bus_test_get_stop_bits(UART_PORT_SDI12));
}

static void test_sdi12_direction_control(void) {
    sdi12_set_direction(SDI12_DIR_TX);
    TEST_ASSERT_EQUAL_INT(1, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_TX, sdi12_test_get_direction());

    sdi12_set_direction(SDI12_DIR_RX);
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_send_break_and_mark_execution(void) {
    sdi12_send_break_and_mark();

    /* Verify break/mark leaves direction pin in TX mode before command TX */
    TEST_ASSERT_EQUAL_INT(1, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_TX, sdi12_test_get_direction());
    TEST_ASSERT_TRUE(sdi12_test_get_break_sent());
    TEST_ASSERT_TRUE(uart_bus_test_get_break_sent());
    TEST_ASSERT_EQUAL_UINT32(13U, uart_bus_test_get_break_duration_ms());
    TEST_ASSERT_EQUAL_UINT32(9U, uart_bus_test_get_mark_duration_ms());

    /* Clean up to RX mode for tearDown check */
    sdi12_set_direction(SDI12_DIR_RX);
}

static void test_sdi12_transmit_command_valid(void) {
    status_t status = sdi12_transmit_command("0M!");
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify "0M!" was transmitted to mock UART */
    TEST_ASSERT_EQUAL_UINT16(3, mock_uart_get_tx_count(UART_PORT_SDI12));
    uint8_t tx_buf[8] = {0};
    (void)mock_uart_get_tx_bytes(UART_PORT_SDI12, tx_buf, sizeof(tx_buf));
    TEST_ASSERT_EQUAL_STRING("0M!", (char *)tx_buf);

    /* Verify line direction returned to RX listening mode */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_transmit_command_missing_exclamation(void) {
    /* Missing trailing '!' */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command("0M"));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command("0D0"));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_transmit_command(NULL));
}

static void test_sdi12_transmit_command_empty_and_overflow(void) {
    /* Empty command */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command(""));

    /* Command exceeding SDI12_MAX_BUFFER_SIZE */
    char long_cmd[80];
    (void)memset(long_cmd, 'A', sizeof(long_cmd) - 1);
    long_cmd[sizeof(long_cmd) - 2] = '!';
    long_cmd[sizeof(long_cmd) - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command(long_cmd));
}

static void test_sdi12_wake_and_transmit_valid(void) {
    status_t status = sdi12_wake_and_transmit("0D0!");
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify break sequence was emitted */
    TEST_ASSERT_TRUE(sdi12_test_get_break_sent());
    TEST_ASSERT_TRUE(uart_bus_test_get_break_sent());

    /* Verify command was transmitted */
    TEST_ASSERT_EQUAL_UINT16(4, mock_uart_get_tx_count(UART_PORT_SDI12));
    uint8_t tx_buf[8] = {0};
    (void)mock_uart_get_tx_bytes(UART_PORT_SDI12, tx_buf, sizeof(tx_buf));
    TEST_ASSERT_EQUAL_STRING("0D0!", (char *)tx_buf);

    /* Verify line direction returned to RX listening mode */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_wake_and_transmit_null_and_invalid(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_wake_and_transmit(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_wake_and_transmit("0D0"));
}

static void test_sdi12_receive_response_valid(void) {
    const char *mock_resp = "00023\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)mock_resp, (uint16_t)strlen(mock_resp));

    char rx_buf[16] = {0};
    uint16_t rx_len = 0;

    status_t status = sdi12_receive_response(rx_buf, sizeof(rx_buf), &rx_len, 1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(7U, rx_len);
    TEST_ASSERT_EQUAL_STRING("00023\r\n", rx_buf);
}

static void test_sdi12_receive_response_missing_crlf(void) {
    const char *bad_resp = "00023";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)bad_resp, (uint16_t)strlen(bad_resp));

    char rx_buf[16] = {0};
    uint16_t rx_len = 0;
    status_t status = sdi12_receive_response(rx_buf, sizeof(rx_buf), &rx_len, 1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, status);
}

static void test_sdi12_receive_response_timeout(void) {
    mock_uart_inject_fault(UART_PORT_SDI12, MOCK_UART_FAULT_TIMEOUT);

    char rx_buf[16] = {0};
    uint16_t rx_len = 0;
    status_t status = sdi12_receive_response(rx_buf, sizeof(rx_buf), &rx_len, 1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_TIMEOUT, status);
}

static void test_sdi12_receive_response_null_and_overflow(void) {
    char rx_buf[16];
    uint16_t rx_len;

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_receive_response(NULL, sizeof(rx_buf), &rx_len, 1000U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_receive_response(rx_buf, sizeof(rx_buf), NULL, 1000U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_BUFFER_OVERFLOW,
                          sdi12_receive_response(rx_buf, 2U, &rx_len, 1000U));
}

static void test_sdi12_deinit(void) {
    sdi12_set_direction(SDI12_DIR_TX);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_deinit());
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

/* ============================================================================
 * Main Test Runner Entry Point
 * ============================================================================ */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_sdi12_init_defaults);
    RUN_TEST(test_sdi12_direction_control);
    RUN_TEST(test_sdi12_send_break_and_mark_execution);
    RUN_TEST(test_sdi12_transmit_command_valid);
    RUN_TEST(test_sdi12_transmit_command_missing_exclamation);
    RUN_TEST(test_sdi12_transmit_command_empty_and_overflow);
    RUN_TEST(test_sdi12_wake_and_transmit_valid);
    RUN_TEST(test_sdi12_wake_and_transmit_null_and_invalid);
    RUN_TEST(test_sdi12_receive_response_valid);
    RUN_TEST(test_sdi12_receive_response_missing_crlf);
    RUN_TEST(test_sdi12_receive_response_timeout);
    RUN_TEST(test_sdi12_receive_response_null_and_overflow);
    RUN_TEST(test_sdi12_deinit);

    return UNITY_END();
}
