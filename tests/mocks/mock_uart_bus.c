/**
 * @file    mock_uart_bus.c
 * @brief   Implementation of Mock UART Bus Subsystem with FIFO Buffering.
 * @details Delegates to uart_bus driver simulation backend without redefining driver APIs.
 */

#include "mock_uart_bus.h"

void mock_uart_init(void) {
    mock_uart_reset();
}

void mock_uart_reset(void) {
    uart_bus_test_reset();
    /* Default in mock tests: manual direction checking is enforced for RS-485 */
    uart_bus_test_set_auto_direction(false);
}

status_t mock_uart_inject_rx(uart_port_t port, const uint8_t *p_data, uint16_t length) {
    return uart_bus_test_inject_rx(port, p_data, length);
}

status_t mock_uart_inject_rx_byte(uart_port_t port, uint8_t byte) {
    return uart_bus_test_inject_rx_byte(port, byte);
}

uint16_t mock_uart_get_tx_bytes(uart_port_t port, uint8_t *p_out, uint16_t max_len) {
    return uart_bus_test_get_tx_bytes(port, p_out, max_len);
}

uint16_t mock_uart_get_tx_count(uart_port_t port) {
    return uart_bus_test_get_tx_count(port);
}

uint16_t mock_uart_get_rx_count(uart_port_t port) {
    return uart_bus_test_get_rx_count(port);
}

uart_dir_t mock_uart_get_direction(uart_port_t port) {
    return uart_bus_test_get_direction(port);
}

void mock_uart_inject_fault(uart_port_t port, mock_uart_fault_t fault) {
    uart_bus_test_inject_fault(port, (uint32_t)fault);
}

void mock_uart_clear_faults(uart_port_t port) {
    uart_bus_test_clear_faults(port);
}
