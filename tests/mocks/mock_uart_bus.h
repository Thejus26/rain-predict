/**
 * @file    mock_uart_bus.h
 * @brief   Host Mock UART Bus & Serial Protocol Injection Controller.
 * @details Simulates RS-485 Modbus RTU and SDI-12 serial buses with FIFO buffering,
 *          transceiver direction tracking, and fault injection.
 */

#ifndef MOCK_UART_BUS_H
#define MOCK_UART_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uart_bus.h"

/** Maximum static buffer capacity per UART port in bytes */
#define MOCK_UART_BUFFER_SIZE   512

/**
 * @brief Simulated serial communication fault conditions.
 */
typedef enum {
    MOCK_UART_FAULT_NONE = 0,        /**< Normal fault-free operation */
    MOCK_UART_FAULT_TIMEOUT,         /**< Simulates disconnected cable / unresponding sensor */
    MOCK_UART_FAULT_FRAMING_ERROR,   /**< Simulates stop-bit timing / baud mismatch */
    MOCK_UART_FAULT_PARITY_ERROR,    /**< Simulates parity bit error (e.g. SDI-12 7E1) */
    MOCK_UART_FAULT_BUFFER_OVERFLOW, /**< Simulates hardware RX overrun */
    MOCK_UART_FAULT_TX_COLLISION     /**< Simulates RS-485 bus contention or driver fault */
} mock_uart_fault_t;

/* --- Mock Control API for Unit Tests --- */

/**
 * @brief Initializes the mock UART subsystem.
 */
void mock_uart_init(void);

/**
 * @brief Resets all port FIFOs, TX capture buffers, direction states, and active faults.
 */
void mock_uart_reset(void);

/**
 * @brief Injects a byte sequence into the designated port's RX FIFO.
 *
 * @param[in] port   Target UART port identifier.
 * @param[in] p_data Pointer to source data bytes to inject.
 * @param[in] length Number of bytes to inject.
 * @return STATUS_OK on success, STATUS_ERR_NULL_PTR if p_data is NULL,
 *         STATUS_ERR_INVALID_PARAM on invalid port, or STATUS_ERR_BUSY if FIFO overflows.
 */
status_t mock_uart_inject_rx(uart_port_t port, const uint8_t *p_data, uint16_t length);

/**
 * @brief Injects a single byte into the designated port's RX FIFO.
 *
 * @param[in] port Target UART port identifier.
 * @param[in] byte Byte value to inject.
 * @return STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port,
 *         or STATUS_ERR_BUSY if FIFO is full.
 */
status_t mock_uart_inject_rx_byte(uart_port_t port, uint8_t byte);

/**
 * @brief Copies transmitted bytes captured from the driver into a test buffer.
 *
 * @param[in]  port    Target UART port identifier.
 * @param[out] p_out   Destination buffer to copy transmitted bytes into.
 * @param[in]  max_len Maximum number of bytes to copy.
 * @return Actual number of bytes copied.
 */
uint16_t mock_uart_get_tx_bytes(uart_port_t port, uint8_t *p_out, uint16_t max_len);

/**
 * @brief Returns the total number of bytes currently recorded in the port's TX buffer.
 *
 * @param[in] port Target UART port identifier.
 * @return Total bytes in TX buffer.
 */
uint16_t mock_uart_get_tx_count(uart_port_t port);

/**
 * @brief Returns the total number of unread bytes in the port's RX FIFO.
 *
 * @param[in] port Target UART port identifier.
 * @return Total unread bytes in RX FIFO.
 */
uint16_t mock_uart_get_rx_count(uart_port_t port);

/**
 * @brief Returns the current RS-485 transceiver direction state of the port.
 *
 * @param[in] port Target UART port identifier.
 * @return Current direction (UART_DIR_RX or UART_DIR_TX).
 */
uart_dir_t mock_uart_get_direction(uart_port_t port);

/**
 * @brief Injects a fault condition onto the specified UART port.
 *
 * @param[in] port  Target UART port identifier.
 * @param[in] fault Fault condition to activate.
 */
void mock_uart_inject_fault(uart_port_t port, mock_uart_fault_t fault);

/**
 * @brief Clears any active fault condition on the specified UART port.
 *
 * @param[in] port Target UART port identifier.
 */
void mock_uart_clear_faults(uart_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_UART_BUS_H */
