/**
 * @file    uart_bus.h
 * @brief   Hardware-independent UART / Serial Master Bus Driver Interface.
 * @details Non-blocking bounded serial driver supporting RS-485 Modbus RTU (USART1)
 *          and SDI-12 agricultural sensor bus (LPUART1) for STM32WLE5 SoC.
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
#include "board_config.h"

/* ============================================================================
 * Serial Port Identifiers & Constants
 * ============================================================================ */

/**
 * @brief Hardware UART/Serial communication port identifiers.
 */
typedef enum {
    UART_PORT_RS485 = 0,   /**< USART1: RS-485 Modbus RTU Master Bus (PA2 TX, PA3 RX, PA1 DE/RE) */
    UART_PORT_SDI12 = 1,   /**< LPUART1: SDI-12 Single-Wire Sensor Bus (PC0 TX, PC1 RX, PC2 DIR) */
    UART_PORT_MAX
} uart_port_t;

/**
 * @brief Serial Parity Configuration Modes.
 */
typedef enum {
    UART_PARITY_NONE = 0,  /**< No parity (8N1) */
    UART_PARITY_EVEN = 1,  /**< Even parity (7E1 for SDI-12) */
    UART_PARITY_ODD  = 2   /**< Odd parity */
} uart_parity_t;

/**
 * @brief RS-485 / SDI-12 transceiver direction control states.
 */
typedef enum {
    UART_DIR_RX = 0,       /**< Receiver Enabled (DE low, /RE low) - Listening mode */
    UART_DIR_TX = 1        /**< Transmitter Enabled (DE high, /RE high) - Drive mode */
} uart_dir_t;

/** Per-port static circular RX ring buffer capacity in bytes */
#define UART_BUS_RX_RING_BUFFER_SIZE    256U

/** Default transmission and reception bounded timeout in milliseconds */
#define UART_BUS_DEFAULT_TIMEOUT_MS     100U

/** Pre-transmission Driver Enable (DE) guard delay in microseconds (SP3485) */
#define UART_BUS_RS485_PRE_DELAY_US     25U

/** Post-transmission Driver Enable (DE) guard delay in microseconds (SP3485) */
#define UART_BUS_RS485_POST_DELAY_US    35U

/** SDI-12 break spacing duration in milliseconds (> 12.0 ms per v1.4 spec) */
#define UART_BUS_SDI12_BREAK_MS         13U

/** SDI-12 mark marking duration in milliseconds (> 8.33 ms per v1.4 spec) */
#define UART_BUS_SDI12_MARK_MS          9U

/* ============================================================================
 * Public Driver API Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes the specified serial bus peripheral, baud rate, and RX ring buffer.
 *
 * @param[in] port       Serial port identifier (UART_PORT_RS485 or UART_PORT_SDI12).
 * @param[in] baud_rate  Baud rate in bps (e.g. 9600, 19200, 115200 for RS-485; 1200 for SDI-12).
 * @param[in] data_bits  Number of data bits (8 for Modbus RTU, 7 for SDI-12).
 * @param[in] parity     Parity mode (UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD).
 * @param[in] stop_bits  Number of stop bits (1 or 2).
 * @return status_t      STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid argument.
 */
status_t uart_bus_init(uart_port_t port,
                       uint32_t baud_rate,
                       uint8_t data_bits,
                       uint8_t parity,
                       uint8_t stop_bits);

/**
 * @brief  De-initializes the specified serial port prior to Stop 2 low-power sleep.
 * @details Gates peripheral clocks and releases GPIO lines for low-leakage conditioning.
 *
 * @param[in] port Serial port identifier.
 * @return status_t STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port.
 */
status_t uart_bus_deinit(uart_port_t port);

/**
 * @brief  Configures transceiver direction for half-duplex RS-485 / SDI-12 bus drivers.
 *
 * @param[in] port Target UART port identifier.
 * @param[in] dir  Transceiver direction (UART_DIR_RX or UART_DIR_TX).
 * @return status_t STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port.
 */
status_t uart_bus_set_direction(uart_port_t port, uart_dir_t dir);

/**
 * @brief  Transmits a byte buffer with automatic DE/RE direction gating and bounded timeouts.
 *
 * @param[in] port       Serial port identifier.
 * @param[in] p_data     Pointer to data bytes to transmit.
 * @param[in] length     Number of bytes to transmit.
 * @param[in] timeout_ms Maximum transaction timeout in milliseconds.
 * @return status_t      STATUS_OK on success, STATUS_ERR_NULL_PTR if p_data is NULL,
 *                       STATUS_ERR_INVALID_PARAM on invalid port, STATUS_ERR_UART_BUS on
 *                       driver collision, or STATUS_ERR_TIMEOUT.
 */
status_t uart_bus_transmit(uart_port_t port,
                           const uint8_t *p_data,
                           uint16_t length,
                           uint32_t timeout_ms);

/**
 * @brief  Reads received bytes from the port's internal circular RX ring buffer.
 *
 * @param[in]  port             Serial port identifier.
 * @param[out] p_data           Destination buffer to copy received bytes into.
 * @param[in]  length           Maximum bytes to read.
 * @param[out] p_bytes_received Actual number of bytes extracted.
 * @param[in]  timeout_ms       Maximum time to wait for bytes.
 * @return status_t             STATUS_OK on success, STATUS_ERR_NULL_PTR if pointers NULL,
 *                              STATUS_ERR_INVALID_PARAM on invalid port, STATUS_ERR_TIMEOUT
 *                              if no data received within timeout window.
 */
status_t uart_bus_receive(uart_port_t port,
                          uint8_t *p_data,
                          uint16_t length,
                          uint16_t *p_bytes_received,
                          uint32_t timeout_ms);

/**
 * @brief  Generates SDI-12 standard >12ms break and >8.3ms mark sequence on LPUART1.
 * @details Drives PC0 HIGH for 13 ms spacing, then LOW for 9 ms marking before restoring AF.
 *
 * @return status_t STATUS_OK on success.
 */
status_t uart_bus_sdi12_send_break(void);

/**
 * @brief  Flushes the port's internal circular RX ring buffer and clears error flags.
 *
 * @param[in] port Serial port identifier.
 * @return status_t STATUS_OK on success, STATUS_ERR_INVALID_PARAM on invalid port.
 */
status_t uart_bus_flush(uart_port_t port);

/**
 * @brief  Returns the number of unread bytes currently available in the RX ring buffer.
 *
 * @param[in] port Serial port identifier.
 * @return uint16_t Unread byte count (0 to 256).
 */
uint16_t uart_bus_get_available(uart_port_t port);

/**
 * @brief  Internal ISR callback invoked upon byte arrival or IDLE line detection.
 *
 * @param[in] port Serial port identifier.
 */
void uart_bus_rx_isr_handler(uart_port_t port);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated UART driver states, FIFOs, and fault injections.
 */
void uart_bus_test_reset(void);

/**
 * @brief Injects a byte sequence into the simulated port's RX ring buffer.
 * @param[in] port   Target port identifier.
 * @param[in] p_data Pointer to data bytes.
 * @param[in] length Number of bytes.
 * @return STATUS_OK on success.
 */
status_t uart_bus_test_inject_rx(uart_port_t port, const uint8_t *p_data, uint16_t length);

/**
 * @brief Injects a single byte into the simulated port's RX ring buffer.
 * @param[in] port Target port identifier.
 * @param[in] byte Byte value.
 * @return STATUS_OK on success.
 */
status_t uart_bus_test_inject_rx_byte(uart_port_t port, uint8_t byte);

/**
 * @brief Copies transmitted bytes captured from the driver into a test buffer.
 * @param[in]  port    Target port identifier.
 * @param[out] p_out   Destination buffer.
 * @param[in]  max_len Maximum bytes to copy.
 * @return uint16_t Actual bytes copied.
 */
uint16_t uart_bus_test_get_tx_bytes(uart_port_t port, uint8_t *p_out, uint16_t max_len);

/**
 * @brief Returns the total count of bytes recorded in the port's TX buffer.
 * @param[in] port Target port identifier.
 * @return uint16_t TX byte count.
 */
uint16_t uart_bus_test_get_tx_count(uart_port_t port);

/**
 * @brief Returns the total number of unread bytes in the port's RX FIFO.
 * @param[in] port Target port identifier.
 * @return uint16_t RX byte count.
 */
uint16_t uart_bus_test_get_rx_count(uart_port_t port);

/**
 * @brief Returns the current transceiver direction of the port.
 * @param[in] port Target port identifier.
 * @return uart_dir_t Current direction.
 */
uart_dir_t uart_bus_test_get_direction(uart_port_t port);

/**
 * @brief Sets the transceiver direction on host simulation.
 * @param[in] port Target port identifier.
 * @param[in] dir  Direction mode.
 */
void uart_bus_test_set_direction(uart_port_t port, uart_dir_t dir);

/**
 * @brief Injects a fault condition onto the specified UART port.
 * @param[in] port  Target port identifier.
 * @param[in] fault Fault identifier (mock_uart_fault_t compatible).
 */
void uart_bus_test_inject_fault(uart_port_t port, uint32_t fault);

/**
 * @brief Clears active fault conditions on the specified UART port.
 * @param[in] port Target port identifier.
 */
void uart_bus_test_clear_faults(uart_port_t port);

/**
 * @brief Checks if the port is currently initialized.
 * @param[in] port Target port identifier.
 * @return bool true if initialized.
 */
bool uart_bus_test_is_initialized(uart_port_t port);

/**
 * @brief Gets configured baud rate for the port.
 * @param[in] port Target port identifier.
 * @return uint32_t Baud rate.
 */
uint32_t uart_bus_test_get_baud_rate(uart_port_t port);

/**
 * @brief Gets configured data bits for the port.
 * @param[in] port Target port identifier.
 * @return uint8_t Data bits (7 or 8).
 */
uint8_t uart_bus_test_get_data_bits(uart_port_t port);

/**
 * @brief Gets configured parity for the port.
 * @param[in] port Target port identifier.
 * @return uint8_t Parity.
 */
uint8_t uart_bus_test_get_parity(uart_port_t port);

/**
 * @brief Gets configured stop bits for the port.
 * @param[in] port Target port identifier.
 * @return uint8_t Stop bits (1 or 2).
 */
uint8_t uart_bus_test_get_stop_bits(uart_port_t port);

/**
 * @brief Checks if SDI-12 break sequence was triggered.
 * @return bool true if break was sent.
 */
bool uart_bus_test_get_break_sent(void);

/**
 * @brief Gets recorded duration of SDI-12 break spacing.
 * @return uint32_t Duration in milliseconds.
 */
uint32_t uart_bus_test_get_break_duration_ms(void);

/**
 * @brief Gets recorded duration of SDI-12 mark marking.
 * @return uint32_t Duration in milliseconds.
 */
uint32_t uart_bus_test_get_mark_duration_ms(void);

/**
 * @brief Gets recorded pre-transmission DE guard delay.
 * @return uint32_t Delay in microseconds.
 */
uint32_t uart_bus_test_get_pre_delay_us(void);

/**
 * @brief Gets recorded post-transmission DE guard delay.
 * @return uint32_t Delay in microseconds.
 */
uint32_t uart_bus_test_get_post_delay_us(void);

/**
 * @brief Configures automatic DE direction gating mode in simulation.
 * @param[in] enable true to enable automatic DE assertion in transmit, false for manual mode.
 */
void uart_bus_test_set_auto_direction(bool enable);

/**
 * @brief Queries automatic DE direction gating mode in simulation.
 * @return bool true if auto-direction is active.
 */
bool uart_bus_test_get_auto_direction(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* UART_BUS_H */
