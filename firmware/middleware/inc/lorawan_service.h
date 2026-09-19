/**
 * @file    lorawan_service.h
 * @brief   STM32WL Sub-GHz LoRaWAN Class A network service and state machine header.
 * @details Provides OTAA activation, unconfirmed/confirmed uplinks, downlink dispatching,
 *          and Sub-GHz RF front-end power switch control.
 */

#ifndef LORAWAN_SERVICE_H
#define LORAWAN_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Protocol & Hardware Constants                                              */
/* ========================================================================== */

/** @brief Length of LoRaWAN IEEE EUI-64 DevEUI and JoinEUI in bytes */
#define LORAWAN_EUI_LENGTH                  8U

/** @brief Length of LoRaWAN 128-bit AES root/session keys in bytes */
#define LORAWAN_KEY_LENGTH                  16U

/** @brief Maximum user payload size supported by LoRaWAN service */
#define LORAWAN_MAX_PAYLOAD_SIZE            242U

/** @brief Standard application port for periodic environmental telemetry */
#define LORAWAN_FPORT_PERIODIC              1U

/** @brief Standard application port for urgent storm warning alerts */
#define LORAWAN_FPORT_ALERT                 2U

/** @brief Standard application port for historical backlog playback batches */
#define LORAWAN_FPORT_PLAYBACK              3U

/** @brief Standard application port for remote downlink node configuration */
#define LORAWAN_FPORT_CONFIG                10U

/** @brief Maximum confirmed uplink re-transmission attempts */
#define LORAWAN_DEFAULT_MAX_RETRIES         3U

/* ========================================================================== */
/* Enumerations & Type Definitions                                            */
/* ========================================================================== */

/**
 * @brief  Operational states for the LoRaWAN service state machine.
 */
typedef enum {
    LORAWAN_STATE_UNJOINED = 0,     /**< Quiescent state; network session inactive */
    LORAWAN_STATE_JOINING,          /**< OTAA Join-Request transmitted; awaiting Join-Accept */
    LORAWAN_STATE_JOIN_FAILED,      /**< Join attempt failed or timed out; backoff active */
    LORAWAN_STATE_JOINED,           /**< Network session active; ready for uplink/downlink */
    LORAWAN_STATE_TX_UPLINK,        /**< Physical RF transmission in progress */
    LORAWAN_STATE_WAIT_RX1,         /**< Awaiting RX1 receive window (+1s after TX) */
    LORAWAN_STATE_WAIT_RX2,         /**< Awaiting RX2 receive window (+2s after TX) */
    LORAWAN_STATE_ERROR             /**< Hardware transceiver fault */
} lorawan_state_t;

/**
 * @brief  RF Front-End power amplifier and switch routing mode.
 */
typedef enum {
    LORAWAN_RF_MODE_SHUTDOWN = 0,   /**< All RF switch paths isolated; radio asleep */
    LORAWAN_RF_MODE_RX,             /**< Antenna routed to RFI input for reception */
    LORAWAN_RF_MODE_TX_HP,          /**< High-Power PA output routed to antenna (+22 dBm) */
    LORAWAN_RF_MODE_TX_LP           /**< Low-Power PA output routed to antenna (+14 dBm) */
} lorawan_rf_mode_t;

/**
 * @brief  LoRaWAN OTAA root credentials configuration structure.
 */
typedef struct {
    uint8_t dev_eui[LORAWAN_EUI_LENGTH];    /**< IEEE EUI-64 unique device identifier */
    uint8_t join_eui[LORAWAN_EUI_LENGTH];   /**< IEEE EUI-64 application/join identifier */
    uint8_t app_key[LORAWAN_KEY_LENGTH];    /**< 128-bit AES application root key */
} lorawan_credentials_t;

/**
 * @brief  LoRaWAN downlink packet reception event structure.
 */
typedef struct {
    uint8_t  fport;                             /**< LoRaWAN application port */
    uint8_t  payload[LORAWAN_MAX_PAYLOAD_SIZE]; /**< Downlink payload buffer */
    uint8_t  length;                            /**< Downlink payload length in bytes */
    int16_t  rssi_dbm;                          /**< Received Signal Strength Indicator in dBm */
    int8_t   snr_db;                            /**< Signal-to-Noise Ratio in dB */
    bool     is_ack;                            /**< True if downlink contains gateway ACK */
} lorawan_rx_packet_t;

/**
 * @brief  Callback function pointer signature for downlink packet reception.
 */
typedef void (*lorawan_rx_callback_t)(const lorawan_rx_packet_t *rx_packet);

/**
 * @brief  Live diagnostic status of the LoRaWAN service.
 */
typedef struct {
    lorawan_state_t state;                  /**< Current state machine status */
    uint32_t        fcnt_up;                /**< Current uplink frame counter */
    uint32_t        fcnt_down;              /**< Current downlink frame counter */
    uint32_t        dev_addr;               /**< 32-bit allocated network device address */
    int16_t         last_rssi_dbm;          /**< RSSI of last received downlink */
    int8_t          last_snr_db;            /**< SNR of last received downlink */
    uint8_t         current_dr;             /**< Active LoRaWAN Data Rate (DR0..DR5) */
    int8_t          tx_power_dbm;           /**< Active RF transmit power in dBm */
    bool            is_joined;              /**< True if device has completed OTAA join */
} lorawan_status_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the LoRaWAN service, hardware radio HAL, and RF switch GPIOs.
 * @param[in] credentials Pointer to device OTAA credentials (or NULL to use factory UID).
 * @return STATUS_OK on success, or hardware initialization error.
 */
status_t lorawan_service_init(const lorawan_credentials_t *credentials);

/**
 * @brief  Registers an application callback for incoming downlink packets.
 * @param[in] callback Function pointer to downlink handler.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER.
 */
status_t lorawan_register_rx_callback(lorawan_rx_callback_t callback);

/**
 * @brief  Initiates an Over-The-Air Activation (OTAA) Join-Request sequence.
 * @return STATUS_OK if join sequence started, STATUS_ERROR_BUSY if busy,
 *         STATUS_ERROR_NOT_INITIALIZED if uninitialized,
 *         or STATUS_ERROR_HARDWARE on radio error.
 */
status_t lorawan_join_otaa(void);

/**
 * @brief  Transmits an unconfirmed LoRaWAN uplink frame.
 * @param[in] fport   Application port (1..223).
 * @param[in] payload Pointer to binary data buffer.
 * @param[in] length  Number of bytes to transmit (1..242).
 * @return STATUS_OK on success, STATUS_ERROR_NOT_INITIALIZED if unjoined/uninitialized,
 *         STATUS_ERROR_NULL_POINTER if payload is NULL, STATUS_ERROR_BUSY if busy,
 *         or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_send_unconfirmed(uint8_t fport, const uint8_t *payload, uint8_t length);

/**
 * @brief  Transmits a confirmed LoRaWAN uplink frame requiring gateway ACK.
 * @param[in] fport   Application port (1..223).
 * @param[in] payload Pointer to binary data buffer.
 * @param[in] length  Number of bytes to transmit (1..242).
 * @return STATUS_OK on success, STATUS_ERROR_NOT_INITIALIZED if unjoined/uninitialized,
 *         STATUS_ERROR_NULL_POINTER if payload is NULL, STATUS_ERROR_BUSY if busy,
 *         or STATUS_ERROR_OUT_OF_BOUNDS.
 */
status_t lorawan_send_confirmed(uint8_t fport, const uint8_t *payload, uint8_t length);

/**
 * @brief  Non-blocking background tick / step for the LoRaWAN service.
 * @details Manages timers, RX1/RX2 window polling, and state transitions.
 * @return STATUS_OK on active progress, STATUS_ERROR_IDLE if quiescent,
 *         or STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_service_process_step(void);

/**
 * @brief  Configures the discrete RF switch front-end pins (PC3, PC4, PC5).
 * @param[in] mode Desired RF front-end routing mode.
 * @return STATUS_OK on success.
 */
status_t lorawan_set_rf_switch(lorawan_rf_mode_t mode);

/**
 * @brief  Returns current active RF front-end routing mode.
 * @return lorawan_rf_mode_t current mode.
 */
lorawan_rf_mode_t lorawan_get_rf_mode(void);

/**
 * @brief  Retrieves the live diagnostic status of the LoRaWAN service.
 * @param[out] out_status Pointer to destination lorawan_status_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NOT_INITIALIZED, or STATUS_ERROR_NULL_POINTER.
 */
status_t lorawan_get_status(lorawan_status_t *out_status);

/**
 * @brief  Retrieves the active OTAA credentials.
 * @param[out] out_credentials Pointer to destination lorawan_credentials_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NOT_INITIALIZED, or STATUS_ERROR_NULL_POINTER.
 */
status_t lorawan_get_credentials(lorawan_credentials_t *out_credentials);

/**
 * @brief  Simulates or injects reception of a downlink packet.
 * @details Updates diagnostic frame counters, SNR/RSSI, and dispatches to registered callback.
 * @param[in] packet Pointer to downlink packet structure.
 * @return STATUS_OK on success, STATUS_ERROR_NOT_INITIALIZED, or STATUS_ERROR_NULL_POINTER.
 */
status_t lorawan_inject_downlink(const lorawan_rx_packet_t *packet);

/**
 * @brief  Checks whether the last confirmed uplink was acknowledged by gateway.
 * @return true if acknowledged, false otherwise.
 */
bool lorawan_is_ack_received(void);

/**
 * @brief  Resets the LoRaWAN stack state machine and clears session keys.
 * @return STATUS_OK on success.
 */
status_t lorawan_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_SERVICE_H */
