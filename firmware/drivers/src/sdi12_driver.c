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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#if defined(HAVE_STM32WLXX_HAL)
    /* Flush stale RX buffer */
    (void)uart_bus_flush(UART_PORT_SDI12);
#endif

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

    while (bytes_received < max_len - 1U) {
        uint8_t byte = 0U;
        uint16_t byte_len = 0U;
        status_t status = uart_bus_receive(UART_PORT_SDI12,
                                           &byte,
                                           1U,
                                           &byte_len,
                                           timeout_ms);
        if (status != STATUS_OK) {
            if (bytes_received > 0U) {
                p_out_buf[bytes_received] = '\0';
                *p_rx_len = bytes_received;
                return STATUS_ERROR_INVALID_FRAME;
            }
            return status;
        }

        if (byte_len == 0U) {
            break;
        }

        p_out_buf[bytes_received++] = (char)byte;

        if (bytes_received >= 2U &&
            p_out_buf[bytes_received - 2U] == '\r' &&
            p_out_buf[bytes_received - 1U] == '\n') {
            break;
        }
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

/* ============================================================================
 * Public Command Formatting & Response Parsing API (Task S4-T5.2)
 * ============================================================================ */

status_t sdi12_format_command(char addr,
                              const char *cmd_type,
                              char *p_out_buf,
                              uint16_t max_len) {
    if (cmd_type == NULL || p_out_buf == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    /* Valid SDI-12 address range: '0'-'9', 'a'-'z', 'A'-'Z', or '?' */
    if (!isalnum((unsigned char)addr) && (addr != '?')) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    int written = snprintf(p_out_buf, (size_t)max_len, "%c%s!", addr, cmd_type);
    if (written < 0 || (uint16_t)written >= max_len) {
        return STATUS_ERROR_BUFFER_OVERFLOW;
    }

    return STATUS_OK;
}

status_t sdi12_parse_measurement_info(const char *p_resp,
                                      char expected_addr,
                                      uint16_t *p_wait_sec,
                                      uint8_t *p_val_count) {
    if (p_resp == NULL || p_wait_sec == NULL || p_val_count == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    uint16_t len = (uint16_t)strlen(p_resp);
    /* Minimum length for "atttn\r\n" is 7 characters (e.g. "00023\r\n") */
    if (len < 7U) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify response ends with \r\n */
    if (p_resp[len - 2U] != '\r' || p_resp[len - 1U] != '\n') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify address character matches */
    if (p_resp[0] != expected_addr && expected_addr != '?') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Extract 3-digit preparation time 'ttt' */
    char ttt_str[4] = {p_resp[1], p_resp[2], p_resp[3], '\0'};
    for (int i = 0; i < 3; i++) {
        if (!isdigit((unsigned char)ttt_str[i])) {
            return STATUS_ERROR_INVALID_FRAME;
        }
    }
    *p_wait_sec = (uint16_t)atoi(ttt_str);

    /* Extract 1-digit count 'n' */
    char n_char = p_resp[4];
    if (!isdigit((unsigned char)n_char)) {
        return STATUS_ERROR_INVALID_FRAME;
    }
    *p_val_count = (uint8_t)(n_char - '0');

    return STATUS_OK;
}

status_t sdi12_parse_data_response(const char *p_resp,
                                   char expected_addr,
                                   float *p_values_out,
                                   uint8_t max_values,
                                   uint8_t *p_actual_count) {
    if (p_resp == NULL || p_values_out == NULL || p_actual_count == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (max_values == 0U) {
        *p_actual_count = 0U;
        return STATUS_ERROR_INVALID_PARAM;
    }

    uint16_t len = (uint16_t)strlen(p_resp);
    /* Minimum length for "a\r\n" is 3 characters */
    if (len < 3U) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify response ends with \r\n */
    if (p_resp[len - 2U] != '\r' || p_resp[len - 1U] != '\n') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify address character */
    if (p_resp[0] != expected_addr && expected_addr != '?') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    uint8_t count = 0U;
    const char *p_curr = &p_resp[1]; /* Start after address character */

    while (*p_curr != '\0' && *p_curr != '\r' && *p_curr != '\n' && count < max_values) {
        /* Every valid numeric token begins with '+' or '-' */
        if (*p_curr != '+' && *p_curr != '-') {
            p_curr++;
            continue;
        }

        char *p_end = NULL;
        float val = strtof(p_curr, &p_end);

        if (p_end == p_curr) {
            /* Parsing failed to consume any digits */
            break;
        }

        p_values_out[count++] = val;
        p_curr = p_end;
    }

    *p_actual_count = count;

    return (count > 0U) ? STATUS_OK : STATUS_ERROR_INVALID_FRAME;
}

status_t sdi12_parse_identification(const char *p_resp,
                                    char expected_addr,
                                    sdi12_sensor_info_t *p_info) {
    if (p_resp == NULL || p_info == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    uint16_t len = (uint16_t)strlen(p_resp);
    /* Standard SDI-12 aI! response length: 1(addr) + 2(ver) + 8(vendor) + 6(model) + 3(fw) + 2(\r\n) = 22 bytes */
    if (len < 22U) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    /* Verify response ends with \r\n */
    if (p_resp[len - 2U] != '\r' || p_resp[len - 1U] != '\n') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    if (p_resp[0] != expected_addr && expected_addr != '?') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    (void)memset(p_info, 0, sizeof(sdi12_sensor_info_t));
    p_info->address = p_resp[0];

    (void)strncpy(p_info->sdi_version, &p_resp[1], 2);
    p_info->sdi_version[2] = '\0';

    (void)strncpy(p_info->vendor_id,   &p_resp[3], 8);
    p_info->vendor_id[8] = '\0';

    (void)strncpy(p_info->model_num,   &p_resp[11], 6);
    p_info->model_num[6] = '\0';

    (void)strncpy(p_info->fw_version,  &p_resp[17], 3);
    p_info->fw_version[3] = '\0';

    /* Optional serial number up to \r\n */
    if (len > 22U) {
        uint16_t sn_len = len - 22U;
        if (sn_len > 13U) {
            sn_len = 13U;
        }
        (void)strncpy(p_info->serial_num, &p_resp[20], sn_len);
        p_info->serial_num[sn_len] = '\0';
    }

    return STATUS_OK;
}

status_t sdi12_query_soil_probe(char addr,
                                sdi12_soil_reading_t *p_reading,
                                uint32_t timeout_ms) {
    if (p_reading == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    char cmd[16];
    char resp[SDI12_MAX_BUFFER_SIZE];
    uint16_t rx_len = 0U;

    /* Step 1: Format and send Start Measurement Command "aM!" */
    status_t status = sdi12_format_command(addr, "M", cmd, (uint16_t)sizeof(cmd));
    if (status != STATUS_OK) {
        return status;
    }

    status = sdi12_wake_and_transmit(cmd);
    if (status != STATUS_OK) {
        return status;
    }

    /* Step 2: Receive measurement acknowledgment "atttn\r\n" */
    status = sdi12_receive_response(resp, (uint16_t)sizeof(resp), &rx_len, timeout_ms);
    if (status != STATUS_OK) {
        return status;
    }

    uint16_t wait_sec = 0U;
    uint8_t  val_count = 0U;
    status = sdi12_parse_measurement_info(resp, addr, &wait_sec, &val_count);
    if (status != STATUS_OK || val_count == 0U) {
        return (status != STATUS_OK) ? status : STATUS_ERROR_INVALID_FRAME;
    }

    /* Step 3: Wait for sensor measurement duration */
    if (wait_sec > 0U) {
#if defined(HAVE_STM32WLXX_HAL)
        HAL_Delay((uint32_t)wait_sec * 1000U);
#endif
    }

    /* Step 4: Format and send Data Request Command "aD0!" */
    status = sdi12_format_command(addr, "D0", cmd, (uint16_t)sizeof(cmd));
    if (status != STATUS_OK) {
        return status;
    }

    status = sdi12_transmit_command(cmd);
    if (status != STATUS_OK) {
        return status;
    }

    /* Step 5: Receive data response "a+values\r\n" */
    status = sdi12_receive_response(resp, (uint16_t)sizeof(resp), &rx_len, timeout_ms);
    if (status != STATUS_OK) {
        return status;
    }

    /* Step 6: Parse floating-point values */
    float parsed_values[SDI12_MAX_VALUES_PER_CMD] = {0};
    uint8_t actual_count = 0U;
    status = sdi12_parse_data_response(resp, addr, parsed_values,
                                       SDI12_MAX_VALUES_PER_CMD, &actual_count);
    if (status != STATUS_OK) {
        return status;
    }

    /* Step 7: Map parsed values into soil reading structure */
    p_reading->sensor_addr   = addr;
    p_reading->num_values    = actual_count;
    p_reading->vwc_m3_m3     = (actual_count >= 1U) ? parsed_values[0] : 0.0f;
    p_reading->temperature_c = (actual_count >= 2U) ? parsed_values[1] : 0.0f;
    p_reading->bulk_ec_ds_m  = (actual_count >= 3U) ? parsed_values[2] : 0.0f;
    p_reading->timestamp_ms  = 0U;

    /* Agronomic Sanity Clamping */
    if (p_reading->vwc_m3_m3 < 0.0f) {
        p_reading->vwc_m3_m3 = 0.0f;
    } else if (p_reading->vwc_m3_m3 > 1.0f) {
        p_reading->vwc_m3_m3 = 1.0f;
    }

    if (p_reading->bulk_ec_ds_m < 0.0f) {
        p_reading->bulk_ec_ds_m = 0.0f;
    }

    return STATUS_OK;
}

status_t sdi12_query_address(char *p_found_addr, uint32_t timeout_ms) {
    if (p_found_addr == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    char resp[SDI12_MAX_BUFFER_SIZE];
    uint16_t rx_len = 0U;

    status_t status = sdi12_wake_and_transmit("?!");
    if (status != STATUS_OK) {
        return status;
    }

    status = sdi12_receive_response(resp, (uint16_t)sizeof(resp), &rx_len, timeout_ms);
    if (status != STATUS_OK) {
        return status;
    }

    if (rx_len < 3U || resp[1] != '\r' || resp[2] != '\n') {
        return STATUS_ERROR_INVALID_FRAME;
    }

    if (!isalnum((unsigned char)resp[0])) {
        return STATUS_ERROR_INVALID_FRAME;
    }

    *p_found_addr = resp[0];
    return STATUS_OK;
}
