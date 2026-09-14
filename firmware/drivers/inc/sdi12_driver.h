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
