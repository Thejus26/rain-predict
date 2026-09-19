/**
 * @file    lorawan_regional.h
 * @brief   LoRaWAN regional channel plans, duty-cycle tracker, and Adaptive Data Rate (ADR) header.
 * @details Conforms to C99 and MISRA C standards with zero dynamic memory allocation.
 */

#ifndef LORAWAN_REGIONAL_H
#define LORAWAN_REGIONAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Regional Identifiers & Constants                                           */
/* ========================================================================== */

/** @brief Supported LoRaWAN geographical regions */
typedef enum {
    LORAWAN_REGION_IN865 = 0,   /**< India 865-867 MHz (Nilgiris, Assam, Munnar) */
    LORAWAN_REGION_EU868,       /**< Europe / Sri Lanka / Kenya 863-870 MHz */
    LORAWAN_REGION_US915        /**< United States / Americas 902-928 MHz (Sub-Band 2) */
} lorawan_region_t;

/** @brief Maximum number of channels supported in channel table */
#define LORAWAN_MAX_CHANNELS                16U

/** @brief ADR limit before setting ADRACKReq bit */
#define LORAWAN_ADR_ACK_LIMIT               64U

/** @brief ADR delay before stepping down Data Rate */
#define LORAWAN_ADR_ACK_DELAY               32U

/* ========================================================================== */
/* Type Definitions & Structures                                              */
/* ========================================================================== */

/**
 * @brief  Individual LoRaWAN RF channel definition.
 */
typedef struct {
    uint32_t frequency_hz;      /**< Center frequency in Hz */
    uint8_t  min_dr;            /**< Minimum allowable Data Rate (e.g. DR0) */
    uint8_t  max_dr;            /**< Maximum allowable Data Rate (e.g. DR5) */
    bool     enabled;           /**< True if channel is enabled in channel mask */
} lorawan_channel_t;

/**
 * @brief  ADR runtime state tracking structure.
 */
typedef struct {
    uint16_t adr_ack_cnt;       /**< Counter incremented per uplink without downlink */
    bool     adr_enabled;       /**< True if ADR is enabled on end-device */
    bool     adr_ack_req;       /**< True if ADRACKReq bit must be asserted in next uplink */
    uint8_t  current_dr;        /**< Active Data Rate (DR0..DR5) */
    int8_t   current_tx_power;  /**< Active RF Transmit Power in dBm */
    uint8_t  nb_trans;          /**< Number of transmissions per uplink frame (default 1) */
} lorawan_adr_state_t;

/**
 * @brief  Duty-cycle tracking accumulator structure.
 */
typedef struct {
    uint32_t accumulated_toa_ms;/**< Total Time-on-Air in current 1-hour window in ms */
    uint32_t last_tx_timestamp_ms; /**< Timestamp of last transmission in ms */
    uint32_t min_off_time_ms;   /**< Minimum remaining off-time before next TX in ms */
    float    duty_cycle_max_pct;/**< Maximum allowable duty cycle percentage (e.g. 1.0f) */
} lorawan_duty_cycle_state_t;

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the regional channel plan and ADR engine.
 * @param[in] region   Target geographical region (IN865, EU868, US915).
 * @param[in] sub_band Target sub-band index (e.g., 2 for US915; 0 for IN865/EU868).
 * @return STATUS_OK on success, or STATUS_ERROR_INVALID_PARAM.
 */
status_t lorawan_regional_init(lorawan_region_t region, uint8_t sub_band);

/**
 * @brief  Selects the next pseudo-random transmit channel and active Data Rate.
 * @param[out] out_freq_hz Pointer to destination frequency in Hz.
 * @param[out] out_dr      Pointer to destination Data Rate.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_get_tx_channel(uint32_t *out_freq_hz, uint8_t *out_dr);

/**
 * @brief  Calculates RX1 receive window frequency and Data Rate.
 * @param[in]  tx_freq_hz   Uplink transmit frequency in Hz.
 * @param[in]  tx_dr        Uplink transmit Data Rate.
 * @param[out] out_rx1_freq Pointer to destination RX1 frequency in Hz.
 * @param[out] out_rx1_dr   Pointer to destination RX1 Data Rate.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_get_rx1_params(uint32_t tx_freq_hz, uint8_t tx_dr,
                                         uint32_t *out_rx1_freq, uint8_t *out_rx1_dr);

/**
 * @brief  Retrieves RX2 receive window default parameters.
 * @param[out] out_rx2_freq Pointer to destination RX2 frequency in Hz.
 * @param[out] out_rx2_dr   Pointer to destination RX2 Data Rate.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_get_rx2_params(uint32_t *out_rx2_freq, uint8_t *out_rx2_dr);

/**
 * @brief  Calculates exact physical Time-on-Air (ToA) in milliseconds for a given payload.
 * @param[in] payload_bytes Total physical payload size in bytes.
 * @param[in] dr            Active Data Rate.
 * @return Calculated Time-on-Air in milliseconds.
 */
uint32_t lorawan_calc_time_on_air_ms(uint8_t payload_bytes, uint8_t dr);

/**
 * @brief  Updates duty-cycle accumulator after a transmission and computes off-time.
 * @param[in] freq_hz Frequency of completed transmission in Hz.
 * @param[in] toa_ms  Time-on-Air duration in milliseconds.
 * @return STATUS_OK on success, or STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_update_duty_cycle(uint32_t freq_hz, uint32_t toa_ms);

/**
 * @brief  Retrieves remaining duty-cycle off-time before next transmission is permitted.
 * @param[in] freq_hz Frequency to check in Hz.
 * @return Remaining off-time in milliseconds (0 if permitted immediately).
 */
uint32_t lorawan_regional_get_off_time_ms(uint32_t freq_hz);

/**
 * @brief  Notifies ADR engine of an uplink transmission.
 * @param[out] out_adr_ack_req Pointer to bool asserting if ADRACKReq must be set in FCtrl.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_adr_on_uplink(bool *out_adr_ack_req);

/**
 * @brief  Notifies ADR engine of a valid downlink reception.
 * @param[in] rssi_dbm RSSI of received downlink in dBm.
 * @param[in] snr_db   SNR of received downlink in dB.
 * @return STATUS_OK on success, or STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_adr_on_downlink(int16_t rssi_dbm, int8_t snr_db);

/**
 * @brief  Processes incoming LinkADRReq MAC payload and generates LinkADRAns byte.
 * @param[in]  payload Pointer to 4-byte LinkADRReq payload.
 * @param[out] out_ans Pointer to destination 1-byte LinkADRAns response.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_adr_process_link_adr_req(const uint8_t *payload, uint8_t *out_ans);

/**
 * @brief  Sets active Data Rate manually.
 * @param[in] dr Target Data Rate (DR0..DR5).
 * @return STATUS_OK on success, or STATUS_ERROR_OUT_OF_BOUNDS / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_set_dr(uint8_t dr);

/**
 * @brief  Sets active RF transmit power.
 * @param[in] power_dbm Target power in dBm.
 * @return STATUS_OK on success, or STATUS_ERROR_OUT_OF_BOUNDS / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_set_tx_power(int8_t power_dbm);

/**
 * @brief  Retrieves current ADR runtime state.
 * @param[out] out_state Pointer to destination structure.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_get_adr_state(lorawan_adr_state_t *out_state);

/**
 * @brief  Retrieves current duty-cycle state.
 * @param[out] out_state Pointer to destination structure.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER / STATUS_ERROR_NOT_INITIALIZED.
 */
status_t lorawan_regional_get_duty_cycle_state(lorawan_duty_cycle_state_t *out_state);

/**
 * @brief  Retrieves channel info by index.
 * @param[in]  index       Channel index (0..LORAWAN_MAX_CHANNELS-1).
 * @param[out] out_channel Destination channel structure.
 * @return STATUS_OK on success, or STATUS_ERROR_OUT_OF_BOUNDS / STATUS_ERROR_NULL_POINTER.
 */
status_t lorawan_regional_get_channel(uint8_t index, lorawan_channel_t *out_channel);

/**
 * @brief  Retrieves count of configured channels.
 * @return Number of active channels configured in channel table.
 */
uint8_t lorawan_regional_get_channel_count(void);

/**
 * @brief  Retrieves active geographical region.
 * @return Active region enum.
 */
lorawan_region_t lorawan_regional_get_active_region(void);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGIONAL_H */
