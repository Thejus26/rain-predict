/**
 * @file    sdi12_driver.h
 * @brief   SDI-12 1200-Baud Half-Duplex Bus Driver & Timing Engine Interface.
 * @details Implements SDI-12 (v1.4) physical layer break/mark timing, direction control,
 *          command framing, and response collection for agricultural sensors on STM32WLE5.
 */

#ifndef SDI12_DRIVER_H
#define SDI12_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"
#include "board_config.h"

/* ============================================================================
 * SDI-12 Protocol & Timing Constants (SDI-12 Standard Version 1.4)
 * ============================================================================ */

#define SDI12_BAUD_RATE                 1200U   /**< Fixed 1200 bps transmission rate */
#define SDI12_DATA_BITS                 7U      /**< 7 Data bits */
#define SDI12_PARITY_EVEN               1U      /**< Even Parity (7E1 standard format) */
#define SDI12_STOP_BITS                 1U      /**< 1 Stop bit */

#define SDI12_BREAK_DURATION_MS         13U     /**< 13 ms Spacing state (+5V, spec min >= 12.0 ms) */
#define SDI12_MARK_DURATION_MS          9U      /**< 9 ms Marking state (0V, spec min >= 8.33 ms) */
#define SDI12_TOTAL_WAKEUP_MS           22U     /**< 22 ms total physical wakeup sequence */
#define SDI12_TURNAROUND_DELAY_US       500U    /**< 500 µs line turnaround guard delay */
#define SDI12_DEFAULT_TIMEOUT_MS        1000U   /**< 1000 ms default response timeout */
#define SDI12_MAX_BUFFER_SIZE           64U     /**< Max SDI-12 response buffer length */
#define SDI12_MAX_VALUES_PER_CMD        9U      /**< Max values returned in single aD0! data frame */
#define SDI12_MAX_ID_STRING_LEN         36U     /**< Max length of an aI! identification response */

/* ============================================================================
 * Type Definitions & Data Structures
 * ============================================================================ */

/**
 * @brief SDI-12 Physical Line Direction State.
 */
typedef enum {
    SDI12_DIR_RX = 0,   /**< Receiver Active (PC2 LOW, Transmit buffer High-Z / listening) */
    SDI12_DIR_TX = 1    /**< Transmitter Active (PC2 HIGH) */
} sdi12_dir_t;

/**
 * @brief Decoded SDI-12 Probe Identification Information (aI!).
 */
typedef struct {
    char        address;                        /**< Probe address character ('0'-'9', 'a'-'z', 'A'-'Z') */
    char        sdi_version[3];                 /**< 2-digit version string (e.g. "14" = v1.4) */
    char        vendor_id[9];                   /**< 8-character vendor name (null-terminated) */
    char        model_num[7];                   /**< 6-character model code (null-terminated) */
    char        fw_version[4];                  /**< 3-character firmware version (null-terminated) */
    char        serial_num[14];                 /**< Up to 13-character serial number (null-terminated) */
} sdi12_sensor_info_t;

/**
 * @brief Decoded Agricultural Soil Moisture & Canopy Temperature Telemetry.
 */
typedef struct {
    char        sensor_addr;                    /**< Sensor address ('0' to '9') */
    float       vwc_m3_m3;                      /**< Volumetric Water Content (0.0 to 1.0 m^3/m^3) */
    float       temperature_c;                  /**< Soil / Leaf Canopy Temperature in deg C */
    float       bulk_ec_ds_m;                   /**< Bulk Electrical Conductivity in dS/m */
    uint8_t     num_values;                     /**< Number of valid fields populated */
    uint32_t    timestamp_ms;                   /**< System tick timestamp of acquisition */
} sdi12_soil_reading_t;

/* ============================================================================
 * Public Physical Layer & Driver API Prototypes (Task S4-T5.1)
 * ============================================================================ */

/**
 * @brief  Initializes LPUART1 for 1200 baud, 7-E-1 format and defaults PC2 to RX listening mode.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t sdi12_init(void);

/**
 * @brief  De-initializes LPUART1 and direction control pin prior to Stop 2 low-power sleep.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t sdi12_deinit(void);

/**
 * @brief  Sets SDI-12 half-duplex direction pin (PC2).
 * @param[in] dir Direction state (SDI12_DIR_RX or SDI12_DIR_TX).
 */
void sdi12_set_direction(sdi12_dir_t dir);

/**
 * @brief  Executes deterministic physical break (13 ms spacing) and mark (9 ms marking) sequence.
 * @details Temporarily reconfigures PC0 as GPIO Output Push-Pull, generates the pulse timings,
 *          and restores PC0 to LPUART1 Alternate Function (AF8). Leaves line in TX mode.
 */
void sdi12_send_break_and_mark(void);

/**
 * @brief  Transmits a formatted SDI-12 ASCII command string (e.g. "0M!") over LPUART1.
 * @details Enforces trailing '!' delimiter, drives PC2 HIGH, awaits hardware transmission
 *          complete (TC), and releases line direction to RX listening mode.
 *
 * @param[in] p_cmd Null-terminated ASCII command string ending in '!'.
 * @return status_t STATUS_OK on success, STATUS_ERR_NULL_PTR if p_cmd is NULL,
 *                  STATUS_ERR_INVALID_PARAM if string empty or missing '!', or timeout/bus error.
 */
status_t sdi12_transmit_command(const char *p_cmd);

/**
 * @brief  Combined physical wakeup and command transmission sequence.
 * @details Flushes stale RX buffer, sends Break+Mark, transmits command, and releases line to RX.
 *
 * @param[in] p_cmd Null-terminated ASCII command string ending in '!'.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t sdi12_wake_and_transmit(const char *p_cmd);

/**
 * @brief  Waits for and collects a \r\n terminated ASCII response string from the SDI-12 bus.
 *
 * @param[out] p_out_buf   Destination buffer for the received ASCII string.
 * @param[in]  max_len     Maximum capacity of destination buffer (must be >= 3).
 * @param[out] p_rx_len    Pointer to store actual number of bytes received (excluding null terminator).
 * @param[in]  timeout_ms  Maximum reception timeout in milliseconds.
 * @return status_t        STATUS_OK on success, STATUS_ERR_NULL_PTR if pointers NULL,
 *                         STATUS_ERR_OVERFLOW if max_len < 3, STATUS_ERR_TIMEOUT if response
 *                         times out, or STATUS_ERR_DATA_CORRUPT if missing \r\n terminator.
 */
status_t sdi12_receive_response(char *p_out_buf,
                                uint16_t max_len,
                                uint16_t *p_rx_len,
                                uint32_t timeout_ms);

/* ============================================================================
 * Public Command Formatting & Response Parsing API Prototypes (Task S4-T5.2)
 * ============================================================================ */

/**
 * @brief Constructs a standard formatted SDI-12 command string (e.g. "0M!").
 *
 * @param[in]  addr       Sensor address character ('0'-'9', 'a'-'z', 'A'-'Z', or '?').
 * @param[in]  cmd_type   Command body string (e.g. "M", "D0", "I", "!", "Ab", "C").
 * @param[out] p_out_buf  Output buffer to store formatted command string.
 * @param[in]  max_len    Capacity of output buffer.
 * @return status_t       STATUS_OK on success,
 *                        STATUS_ERROR_NULL_POINTER if cmd_type or p_out_buf is NULL,
 *                        STATUS_ERROR_INVALID_PARAM if addr is invalid,
 *                        STATUS_ERROR_BUFFER_OVERFLOW if output buffer is insufficient.
 */
status_t sdi12_format_command(char addr,
                              const char *cmd_type,
                              char *p_out_buf,
                              uint16_t max_len);

/**
 * @brief Parses an SDI-12 measurement response string ("atttn\r\n") from an aM! / aC! command.
 *
 * @param[in]  p_resp         Null-terminated ASCII response string.
 * @param[in]  expected_addr  Expected sensor address character (or '?' to accept any address).
 * @param[out] p_wait_sec     Decoded measurement delay in seconds (0 to 999).
 * @param[out] p_val_count    Decoded number of values returned by subsequent aD0! commands (1 to 9).
 * @return status_t           STATUS_OK on success,
 *                            STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *                            STATUS_ERROR_INVALID_FRAME if response formatting or address is invalid.
 */
status_t sdi12_parse_measurement_info(const char *p_resp,
                                      char expected_addr,
                                      uint16_t *p_wait_sec,
                                      uint8_t *p_val_count);

/**
 * @brief Parses floating-point numeric tokens from an SDI-12 data response ("a+val1+val2...\r\n").
 * @details Extracts variable-length signed floats (including scientific notation) without heap allocation.
 *
 * @param[in]  p_resp          Null-terminated ASCII response string.
 * @param[in]  expected_addr   Expected sensor address character (or '?' to accept any address).
 * @param[out] p_values_out    Array to store parsed floating-point values.
 * @param[in]  max_values      Maximum capacity of p_values_out array.
 * @param[out] p_actual_count  Pointer to store the number of successfully extracted float values.
 * @return status_t            STATUS_OK on success,
 *                             STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *                             STATUS_ERROR_INVALID_FRAME if address mismatch or no values found.
 */
status_t sdi12_parse_data_response(const char *p_resp,
                                   char expected_addr,
                                   float *p_values_out,
                                   uint8_t max_values,
                                   uint8_t *p_actual_count);

/**
 * @brief Parses an SDI-12 sensor identification response ("allccccccccmmmmmmvvvxxx\r\n") from an aI! command.
 *
 * @param[in]  p_resp         Null-terminated ASCII response string.
 * @param[in]  expected_addr  Expected sensor address character (or '?' to accept any address).
 * @param[out] p_info         Pointer to output structure to store decoded probe identification.
 * @return status_t           STATUS_OK on success,
 *                            STATUS_ERROR_NULL_POINTER if any pointer is NULL,
 *                            STATUS_ERROR_INVALID_FRAME if response formatting or address is invalid.
 */
status_t sdi12_parse_identification(const char *p_resp,
                                    char expected_addr,
                                    sdi12_sensor_info_t *p_info);

/**
 * @brief Executes a complete 2-stage soil moisture/canopy probe acquisition sequence (aM! -> wait -> aD0!).
 *
 * @param[in]  addr        Sensor address character ('0'-'9', 'a'-'z', 'A'-'Z').
 * @param[out] p_reading   Pointer to output structure to store parsed agricultural telemetry.
 * @param[in]  timeout_ms  Bus response timeout in milliseconds.
 * @return status_t        STATUS_OK on success, or appropriate error code.
 */
status_t sdi12_query_soil_probe(char addr,
                                sdi12_soil_reading_t *p_reading,
                                uint32_t timeout_ms);

/**
 * @brief Issues an SDI-12 address discovery query ("?!") to find the address of a single connected probe.
 *
 * @param[out] p_found_addr  Pointer to store discovered probe address character.
 * @param[in]  timeout_ms    Bus response timeout in milliseconds.
 * @return status_t          STATUS_OK on success, or appropriate error code.
 */
status_t sdi12_query_address(char *p_found_addr, uint32_t timeout_ms);

#if !defined(HAVE_STM32WLXX_HAL)
/* ============================================================================
 * Host Test & Simulation API
 * ============================================================================ */

/**
 * @brief Gets current simulated direction state of SDI-12 driver.
 * @return sdi12_dir_t Current direction.
 */
sdi12_dir_t sdi12_test_get_direction(void);

/**
 * @brief Checks if break/mark sequence was triggered in simulation.
 * @return bool true if break was sent.
 */
bool sdi12_test_get_break_sent(void);

/**
 * @brief Resets simulated SDI-12 driver state.
 */
void sdi12_test_reset(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* SDI12_DRIVER_H */
