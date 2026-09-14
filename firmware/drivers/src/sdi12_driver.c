/**
 * @file    sdi12_driver.c
 * @brief   SDI-12 1200-Baud Half-Duplex Bus Driver & Timing Engine.
 * @details Implements SDI-12 break and mark timing sequences, line direction toggling,
 *          command transmission with TC synchronization, and response collection.
 */

#include "sdi12_driver.h"
#include "uart_bus.h"
#include "gpio_driver.h"
#include "board_config.h"
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware Physical Layer Timing & Direction
 * ============================================================================ */

void sdi12_set_direction(sdi12_dir_t dir) {
    HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN,
                      (dir == SDI12_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    (void)uart_bus_set_direction(UART_PORT_SDI12,
                                 (dir == SDI12_DIR_TX) ? UART_DIR_TX : UART_DIR_RX);
}

void sdi12_send_break_and_mark(void) {
    GPIO_InitTypeDef gpio_init = {0};

    /* Step 1: Enable TX direction on PC2 */
    sdi12_set_direction(SDI12_DIR_TX);

    /* Step 2: Reconfigure PC0 as GPIO Output Push-Pull */
    gpio_init.Pin   = PIN_SDI12_TX_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PIN_SDI12_TX_PORT, &gpio_init);

    /* Step 3: Break State (Spacing / +5V) for 13.0 ms (> 12.0 ms) */
    HAL_GPIO_WritePin(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN, GPIO_PIN_SET);
    HAL_Delay(SDI12_BREAK_DURATION_MS);

    /* Step 4: Mark State (Marking / 0V) for 9.0 ms (> 8.33 ms) */
    HAL_GPIO_WritePin(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN, GPIO_PIN_RESET);
    HAL_Delay(SDI12_MARK_DURATION_MS);

    /* Step 5: Restore PC0 to LPUART1 Alternate Function (AF8) */
    gpio_init.Pin       = PIN_SDI12_TX_PIN;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_PULLUP;
    gpio_init.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = PIN_SDI12_TX_AF;
    HAL_GPIO_Init(PIN_SDI12_TX_PORT, &gpio_init);
}

#else

/* ============================================================================
 * Host Simulation State & Mock Timing
 * ============================================================================ */

static sdi12_dir_t s_mock_sdi12_dir  = SDI12_DIR_RX;
static bool        s_mock_break_sent = false;

void sdi12_set_direction(sdi12_dir_t dir) {
    s_mock_sdi12_dir = dir;
    gpio_write_pin(GPIO_PORT_C, 2, (dir == SDI12_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    board_test_set_pin_state(GPIOC, GPIO_PIN_2, (dir == SDI12_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    (void)uart_bus_set_direction(UART_PORT_SDI12, (dir == SDI12_DIR_TX) ? UART_DIR_TX : UART_DIR_RX);
    uart_bus_test_set_direction(UART_PORT_SDI12, (dir == SDI12_DIR_TX) ? UART_DIR_TX : UART_DIR_RX);
}

void sdi12_send_break_and_mark(void) {
    sdi12_set_direction(SDI12_DIR_TX);
    s_mock_break_sent = true;
    gpio_write_pin(GPIO_PORT_C, 0, GPIO_PIN_SET);
    gpio_write_pin(GPIO_PORT_C, 0, GPIO_PIN_RESET);
    (void)uart_bus_sdi12_send_break();
}

sdi12_dir_t sdi12_test_get_direction(void) {
    return s_mock_sdi12_dir;
}

bool sdi12_test_get_break_sent(void) {
    return s_mock_break_sent;
}

void sdi12_test_reset(void) {
    s_mock_sdi12_dir  = SDI12_DIR_RX;
    s_mock_break_sent = false;
}

#endif /* HAVE_STM32WLXX_HAL */

/* ============================================================================
 * Public Driver Initialization & Lifecycle
 * ============================================================================ */

status_t sdi12_init(void) {
    /* Initialize direction pin PC2 to LOW (RX Listening Mode) */
    sdi12_set_direction(SDI12_DIR_RX);

    /* Initialize LPUART1 for 1200 baud, 7-E-1 format */
    return uart_bus_init(UART_PORT_SDI12,
                         SDI12_BAUD_RATE,
                         SDI12_DATA_BITS,
                         (uint8_t)UART_PARITY_EVEN,
                         SDI12_STOP_BITS);
}

status_t sdi12_deinit(void) {
    sdi12_set_direction(SDI12_DIR_RX);
    return uart_bus_deinit(UART_PORT_SDI12);
}

/* ============================================================================
 * Command Transmission & Physical Sequencing
 * ============================================================================ */

status_t sdi12_transmit_command(const char *p_cmd) {
    if (p_cmd == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    size_t len = strlen(p_cmd);
    if (len == 0U || len > (size_t)SDI12_MAX_BUFFER_SIZE) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    /* Verify command terminates with '!' */
    if (p_cmd[len - 1U] != '!') {
        return STATUS_ERROR_INVALID_PARAM;
    }

    /* Step 1: Assert TX direction */
    sdi12_set_direction(SDI12_DIR_TX);

    /* Step 2: Transmit 7-E-1 ASCII Stream */
    status_t status = uart_bus_transmit(UART_PORT_SDI12,
                                        (const uint8_t *)p_cmd,
                                        (uint16_t)len,
                                        100U);
    if (status != STATUS_OK) {
        sdi12_set_direction(SDI12_DIR_RX);
        return status;
    }

    /* Step 3: Line Turnaround - Release PC2 to RX listening mode */
    sdi12_set_direction(SDI12_DIR_RX);

    return STATUS_OK;
}

status_t sdi12_wake_and_transmit(const char *p_cmd) {
    if (p_cmd == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    size_t len = strlen(p_cmd);
    if (len == 0U || len > (size_t)SDI12_MAX_BUFFER_SIZE || p_cmd[len - 1U] != '!') {
        return STATUS_ERROR_INVALID_PARAM;
    }

    /* Flush stale RX buffer */
    (void)uart_bus_flush(UART_PORT_SDI12);

    /* Issue Break (13ms) and Mark (9ms) sequence */
    sdi12_send_break_and_mark();

    /* Transmit formatted ASCII command and transition to RX */
    status_t status = sdi12_transmit_command(p_cmd);
    if (status != STATUS_OK) {
        sdi12_set_direction(SDI12_DIR_RX);
    }
    return status;
}

/* ============================================================================
 * Response Acquisition
 * ============================================================================ */

status_t sdi12_receive_response(char *p_out_buf,
                                uint16_t max_len,
                                uint16_t *p_rx_len,
                                uint32_t timeout_ms) {
    if (p_out_buf == NULL || p_rx_len == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (max_len < 3U) {
        return STATUS_ERROR_BUFFER_OVERFLOW;
    }

    *p_rx_len = 0U;

    uint16_t bytes_received = 0U;
    status_t status = uart_bus_receive(UART_PORT_SDI12,
                                       (uint8_t *)p_out_buf,
                                       max_len - 1U,
                                       &bytes_received,
                                       timeout_ms);
    if (status != STATUS_OK) {
        return status;
    }

    /* Null-terminate response string */
    p_out_buf[bytes_received] = '\0';
    *p_rx_len = bytes_received;

    /* Verify response contains \r\n terminator */
    if (bytes_received < 3U || p_out_buf[bytes_received - 2U] != '\r' ||
        p_out_buf[bytes_received - 1U] != '\n') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    return STATUS_OK;
}
