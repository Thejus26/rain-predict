/**
 * @file    mock_uart_bus.c
 * @brief   Implementation of Mock UART Bus Subsystem with FIFO Buffering.
 * @details Conforms to MISRA-C and zero-dynamic-allocation embedded standards.
 */

#include "mock_uart_bus.h"
#include <string.h>

typedef struct {
    uint8_t rx_fifo[MOCK_UART_BUFFER_SIZE];
    uint16_t rx_head;
    uint16_t rx_tail;
    uint16_t rx_count;

    uint8_t tx_buffer[MOCK_UART_BUFFER_SIZE];
    uint16_t tx_count;

    uart_dir_t direction;
    mock_uart_fault_t active_fault;
    bool is_initialized;
} mock_uart_port_t;

static mock_uart_port_t s_ports[UART_PORT_MAX];

void mock_uart_init(void) {
    mock_uart_reset();
}

void mock_uart_reset(void) {
    (void)memset(s_ports, 0, sizeof(s_ports));
    for (size_t i = 0; i < (size_t)UART_PORT_MAX; i++) {
        s_ports[i].direction = UART_DIR_RX;
        s_ports[i].active_fault = MOCK_UART_FAULT_NONE;
        s_ports[i].is_initialized = true;
    }
}

status_t mock_uart_inject_rx_byte(uart_port_t port, uint8_t byte) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    mock_uart_port_t *p_port = &s_ports[port];

    if (p_port->rx_count >= MOCK_UART_BUFFER_SIZE) {
        return STATUS_ERR_BUSY;
    }

    p_port->rx_fifo[p_port->rx_head] = byte;
    p_port->rx_head = (uint16_t)((p_port->rx_head + 1U) % MOCK_UART_BUFFER_SIZE);
    p_port->rx_count++;
    return STATUS_OK;
}

status_t mock_uart_inject_rx(uart_port_t port, const uint8_t *p_data, uint16_t length) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    for (uint16_t i = 0; i < length; i++) {
        status_t status = mock_uart_inject_rx_byte(port, p_data[i]);
        if (status != STATUS_OK) {
            return status;
        }
    }
    return STATUS_OK;
}

uint16_t mock_uart_get_tx_bytes(uart_port_t port, uint8_t *p_out, uint16_t max_len) {
    if (port >= UART_PORT_MAX || p_out == NULL) {
        return 0U;
    }
    mock_uart_port_t *p_port = &s_ports[port];
    uint16_t copy_len = (p_port->tx_count < max_len) ? p_port->tx_count : max_len;
    (void)memcpy(p_out, p_port->tx_buffer, copy_len);
    return copy_len;
}

uint16_t mock_uart_get_tx_count(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_ports[port].tx_count;
}

uint16_t mock_uart_get_rx_count(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_ports[port].rx_count;
}

uart_dir_t mock_uart_get_direction(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return UART_DIR_RX;
    }
    return s_ports[port].direction;
}

void mock_uart_inject_fault(uart_port_t port, mock_uart_fault_t fault) {
    if (port < UART_PORT_MAX) {
        s_ports[port].active_fault = fault;
    }
}

void mock_uart_clear_faults(uart_port_t port) {
    if (port < UART_PORT_MAX) {
        s_ports[port].active_fault = MOCK_UART_FAULT_NONE;
    }
}

/* --- Implementation of Production UART Bus Abstraction --- */

status_t uart_bus_init(uart_port_t port,
                       uint32_t baud_rate,
                       uint8_t data_bits,
                       uint8_t parity,
                       uint8_t stop_bits) {
    (void)baud_rate;
    (void)data_bits;
    (void)parity;
    (void)stop_bits;
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_ports[port].is_initialized = true;
    return STATUS_OK;
}

status_t uart_bus_set_direction(uart_port_t port, uart_dir_t dir) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_ports[port].direction = dir;
    return STATUS_OK;
}

status_t uart_bus_transmit(uart_port_t port,
                           const uint8_t *p_data,
                           uint16_t length,
                           uint32_t timeout_ms) {
    (void)timeout_ms;
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    mock_uart_port_t *p_port = &s_ports[port];

    if (p_port->active_fault == MOCK_UART_FAULT_TIMEOUT) {
        return STATUS_ERR_TIMEOUT;
    }
    if (p_port->active_fault == MOCK_UART_FAULT_TX_COLLISION) {
        return STATUS_ERR_UART_BUS;
    }

    /* Verify RS-485 transceiver direction guard (PA1 DE pin must be high) */
    if (port == UART_PORT_RS485 && p_port->direction != UART_DIR_TX) {
        return STATUS_ERR_UART_BUS;
    }

    uint16_t available_space = (uint16_t)(MOCK_UART_BUFFER_SIZE - p_port->tx_count);
    uint16_t write_len = (length < available_space) ? length : available_space;

    (void)memcpy(&p_port->tx_buffer[p_port->tx_count], p_data, write_len);
    p_port->tx_count = (uint16_t)(p_port->tx_count + write_len);

    return (write_len == length) ? STATUS_OK : STATUS_ERR_BUSY;
}

status_t uart_bus_receive(uart_port_t port,
                          uint8_t *p_data,
                          uint16_t length,
                          uint16_t *p_bytes_received,
                          uint32_t timeout_ms) {
    (void)timeout_ms;
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL || p_bytes_received == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    *p_bytes_received = 0U;
    mock_uart_port_t *p_port = &s_ports[port];

    if (p_port->active_fault == MOCK_UART_FAULT_TIMEOUT || p_port->rx_count == 0U) {
        return STATUS_ERR_TIMEOUT;
    }
    if (p_port->active_fault == MOCK_UART_FAULT_FRAMING_ERROR ||
        p_port->active_fault == MOCK_UART_FAULT_PARITY_ERROR) {
        return STATUS_ERR_UART_BUS;
    }
    if (p_port->active_fault == MOCK_UART_FAULT_BUFFER_OVERFLOW) {
        return STATUS_ERR_OVERFLOW;
    }

    uint16_t bytes_to_read = (length < p_port->rx_count) ? length : p_port->rx_count;
    for (uint16_t i = 0; i < bytes_to_read; i++) {
        p_data[i] = p_port->rx_fifo[p_port->rx_tail];
        p_port->rx_tail = (uint16_t)((p_port->rx_tail + 1U) % MOCK_UART_BUFFER_SIZE);
        p_port->rx_count--;
    }

    *p_bytes_received = bytes_to_read;
    return STATUS_OK;
}

status_t uart_bus_flush(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    mock_uart_port_t *p_port = &s_ports[port];
    p_port->rx_head = 0U;
    p_port->rx_tail = 0U;
    p_port->rx_count = 0U;
    p_port->tx_count = 0U;
    return STATUS_OK;
}
