/**
 * @file    uart_bus.h
 * @brief   Hardware-independent UART / Serial Master Bus Driver Interface.
 * @details Declares bounded serial communication operations for RS-485 Modbus RTU
 *          and SDI-12 agricultural sensor bus transceivers.
 */

#ifndef UART_BUS_H
#define UART_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

/**
 * @brief Hardware UART/Serial communication port identifiers.
 */
typedef enum {
    UART_PORT_RS485 = 0,   /**< USART1: RS-485 Modbus RTU Master Bus (9600-115200 baud, 8N1) */
    UART_PORT_SDI12 = 1,   /**< LPUART1: SDI-12 Single-Wire Sensor Bus (1200 baud, 7E1) */
    UART_PORT_MAX
} uart_port_t;

/**
 * @brief RS-485 transceiver direction control states (Driver Enable / Receiver Enable).
 */
typedef enum {
    UART_DIR_RX = 0,       /**< Receiver Enabled (DE low, /RE low) - Listening mode */
    UART_DIR_TX = 1        /**< Transmitter Enabled (DE high, /RE high) - Drive mode */
} uart_dir_t;

/**
 * @brief Initializes a designated UART peripheral port.
 *
 * @param[in] port       Target UART port identifier.
 * @param[in] baud_rate  Serial communication baud rate in bps (e.g. 9600, 19200, 115200).
 * @param[in] data_bits  Number of data bits (7 or 8).
 * @param[in] parity     Parity configuration (0: None, 1: Odd, 2: Even).
 * @param[in] stop_bits  Number of stop bits (1 or 2).
 * @return STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port or configuration.
 */
status_t uart_bus_init(uart_port_t port,
                       uint32_t baud_rate,
                       uint8_t data_bits,
                       uint8_t parity,
                       uint8_t stop_bits);

/**
 * @brief Configures transceiver direction for half-duplex RS-485 / SDI-12 bus drivers.
 *
 * @param[in] port Target UART port identifier.
 * @param[in] dir  Transceiver direction (UART_DIR_RX or UART_DIR_TX).
 * @return STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port.
 */
status_t uart_bus_set_direction(uart_port_t port, uart_dir_t dir);

/**
 * @brief Transmits a byte sequence over the designated UART port.
 *
 * @param[in] port       Target UART port identifier.
 * @param[in] p_data     Pointer to data buffer to transmit.
 * @param[in] length     Number of bytes to transmit.
 * @param[in] timeout_ms Maximum allowable transmission duration in milliseconds.
 * @return STATUS_OK on success, STATUS_ERR_NULL_PTR if p_data is NULL,
 *         STATUS_ERR_INVALID_PARAM on invalid port, STATUS_ERR_UART_BUS if RS-485
 *         transceiver is in RX mode when transmit is attempted, or STATUS_ERR_TIMEOUT.
 */
status_t uart_bus_transmit(uart_port_t port,
                           const uint8_t *p_data,
                           uint16_t length,
                           uint32_t timeout_ms);

/**
 * @brief Receives a byte sequence from the designated UART port with bounded timeout.
 *
 * @param[in]  port              Target UART port identifier.
 * @param[out] p_data            Pointer to destination buffer to store received bytes.
 * @param[in]  length            Maximum number of bytes to receive.
 * @param[out] p_bytes_received  Pointer to variable populated with actual count of received bytes.
 * @param[in]  timeout_ms        Maximum allowable receive duration in milliseconds.
 * @return STATUS_OK on success, STATUS_ERR_NULL_PTR if p_data or p_bytes_received is NULL,
 *         STATUS_ERR_INVALID_PARAM on invalid port, or STATUS_ERR_TIMEOUT if no data received.
 */
status_t uart_bus_receive(uart_port_t port,
                          uint8_t *p_data,
                          uint16_t length,
                          uint16_t *p_bytes_received,
                          uint32_t timeout_ms);

/**
 * @brief Flushes all pending RX and TX buffers on the designated UART port.
 *
 * @param[in] port Target UART port identifier.
 * @return STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port.
 */
status_t uart_bus_flush(uart_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* UART_BUS_H */
