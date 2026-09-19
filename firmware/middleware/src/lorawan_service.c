/**
 * @file    lorawan_service.c
 * @brief   STM32WL Sub-GHz LoRaWAN Class A network service and state machine implementation.
 * @details Conforms to C99 and MISRA C standards with zero dynamic memory allocation.
 */

#include "lorawan_service.h"
#include "board_config.h"
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)
#include "stm32wlxx_hal.h"
#endif

/* ========================================================================== */
/* Static Module Variables & State Context                                    */
/* ========================================================================== */

static lorawan_credentials_t s_credentials;
static lorawan_status_t      s_status = {
    .state          = LORAWAN_STATE_UNJOINED,
    .fcnt_up        = 0U,
    .fcnt_down      = 0U,
    .dev_addr       = 0U,
    .last_rssi_dbm  = -120,
    .last_snr_db    = 0,
    .current_dr     = 5U,  /* DR5 (SF7 / 125 kHz) default */
    .tx_power_dbm   = 14,  /* +14 dBm default */
    .is_joined      = false
};

static lorawan_rf_mode_t     s_rf_mode = LORAWAN_RF_MODE_SHUTDOWN;
static lorawan_rx_callback_t s_rx_callback = NULL;
static bool                  s_is_initialized = false;
static uint8_t               s_tx_buffer[LORAWAN_MAX_PAYLOAD_SIZE];
static uint8_t               s_tx_length = 0U;
static uint8_t               s_tx_fport  = 0U;
static bool                  s_tx_confirmed = false;
static uint8_t               s_tx_retry_count = 0U;
static bool                  s_last_ack_received = false;

/* Mock simulation variables for host test harness & builds without vendor HAL */
#if !defined(HAVE_STM32WLXX_HAL)
static bool s_mock_join_accept_pending = false;
static bool s_mock_ack_pending = false;
#endif

/* ========================================================================== */
/* Private Helper Functions                                                   */
/* ========================================================================== */

/**
 * @brief  Derives DevEUI from on-chip STM32 96-bit Unique ID if not supplied.
 */
static void lorawan_derive_factory_deveui(uint8_t *eui) {
    if (eui == NULL) {
        return;
    }

#if defined(HAVE_STM32WLXX_HAL)
    /* Read hardware 96-bit Unique Device ID registers (UID64 / UID_BASE @ 0x1FFF7590) */
    const volatile uint32_t *p_uid = (const volatile uint32_t *)0x1FFF7590U;
    uint32_t uid0 = p_uid[0];
    uint32_t uid1 = p_uid[1];
    eui[0] = (uint8_t)(uid0 >> 24);
    eui[1] = (uint8_t)(uid0 >> 16);
    eui[2] = (uint8_t)(uid0 >> 8);
    eui[3] = (uint8_t)(uid0);
    eui[4] = (uint8_t)(uid1 >> 24);
    eui[5] = (uint8_t)(uid1 >> 16);
    eui[6] = (uint8_t)(uid1 >> 8);
    eui[7] = (uint8_t)(uid1);
#else
    /* Mock DevEUI for host unit testing & simulation without vendor HAL */
    const uint8_t mock_eui[8] = { 0x70U, 0xB3U, 0xD5U, 0x7EU, 0xD0U, 0x04U, 0x12U, 0x34U };
    (void)memcpy(eui, mock_eui, sizeof(mock_eui));
#endif
}

/* ========================================================================== */
/* Public API Implementation                                                  */
/* ========================================================================== */

status_t lorawan_set_rf_switch(lorawan_rf_mode_t mode) {
    s_rf_mode = mode;

    board_rf_mode_t bsp_mode;
    switch (mode) {
        case LORAWAN_RF_MODE_RX:
            bsp_mode = RF_SWITCH_RX;
            break;
        case LORAWAN_RF_MODE_TX_HP:
            bsp_mode = RF_SWITCH_TX_HP;
            break;
        case LORAWAN_RF_MODE_TX_LP:
            bsp_mode = RF_SWITCH_TX_LP;
            break;
        case LORAWAN_RF_MODE_SHUTDOWN:
        default:
            bsp_mode = RF_SWITCH_SHUTDOWN;
            break;
    }

    board_rf_switch_set(bsp_mode);
    return STATUS_OK;
}

lorawan_rf_mode_t lorawan_get_rf_mode(void) {
    return s_rf_mode;
}

status_t lorawan_service_init(const lorawan_credentials_t *credentials) {
    if (credentials != NULL) {
        s_credentials = *credentials;
    } else {
        lorawan_derive_factory_deveui(s_credentials.dev_eui);
        (void)memset(s_credentials.join_eui, 0x00, LORAWAN_EUI_LENGTH);
        (void)memset(s_credentials.app_key,  0x00, LORAWAN_KEY_LENGTH);
    }

    s_status.state         = LORAWAN_STATE_UNJOINED;
    s_status.fcnt_up       = 0U;
    s_status.fcnt_down     = 0U;
    s_status.dev_addr      = 0U;
    s_status.last_rssi_dbm = -120;
    s_status.last_snr_db   = 0;
    s_status.current_dr    = 5U;
    s_status.tx_power_dbm  = 14;
    s_status.is_joined     = false;

    (void)memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
    s_tx_length         = 0U;
    s_tx_fport          = 0U;
    s_tx_confirmed      = false;
    s_tx_retry_count    = 0U;
    s_last_ack_received = false;

#if !defined(HAVE_STM32WLXX_HAL)
    s_mock_join_accept_pending = false;
    s_mock_ack_pending         = false;
#endif

    (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_SHUTDOWN);
    s_is_initialized = true;

    return STATUS_OK;
}

status_t lorawan_register_rx_callback(lorawan_rx_callback_t callback) {
    if (callback == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    s_rx_callback = callback;
    return STATUS_OK;
}

status_t lorawan_join_otaa(void) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if ((s_status.state == LORAWAN_STATE_JOINING) || (s_status.state == LORAWAN_STATE_TX_UPLINK)) {
        return STATUS_ERROR_BUSY;
    }

    s_status.state = LORAWAN_STATE_JOINING;
    (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_TX_HP);

#if !defined(HAVE_STM32WLXX_HAL)
    /* Host simulation: arm automatic join accept */
    s_mock_join_accept_pending = true;
#endif

    return STATUS_OK;
}

status_t lorawan_send_unconfirmed(uint8_t fport, const uint8_t *payload, uint8_t length) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (!s_status.is_joined) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if ((length == 0U) || (length > LORAWAN_MAX_PAYLOAD_SIZE) || (fport == 0U) || (fport > 223U)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    if ((s_status.state == LORAWAN_STATE_TX_UPLINK) || (s_status.state == LORAWAN_STATE_JOINING)) {
        return STATUS_ERROR_BUSY;
    }

    (void)memcpy(s_tx_buffer, payload, length);
    s_tx_length         = length;
    s_tx_fport          = fport;
    s_tx_confirmed      = false;
    s_tx_retry_count    = 0U;
    s_last_ack_received = false;

    s_status.state = LORAWAN_STATE_TX_UPLINK;
    (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_TX_HP);

    return STATUS_OK;
}

status_t lorawan_send_confirmed(uint8_t fport, const uint8_t *payload, uint8_t length) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (!s_status.is_joined) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if ((length == 0U) || (length > LORAWAN_MAX_PAYLOAD_SIZE) || (fport == 0U) || (fport > 223U)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    if ((s_status.state == LORAWAN_STATE_TX_UPLINK) || (s_status.state == LORAWAN_STATE_JOINING)) {
        return STATUS_ERROR_BUSY;
    }

    (void)memcpy(s_tx_buffer, payload, length);
    s_tx_length         = length;
    s_tx_fport          = fport;
    s_tx_confirmed      = true;
    s_tx_retry_count    = 0U;
    s_last_ack_received = false;

    s_status.state = LORAWAN_STATE_TX_UPLINK;
    (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_TX_HP);

#if !defined(HAVE_STM32WLXX_HAL)
    s_mock_ack_pending = true;
#endif

    return STATUS_OK;
}

status_t lorawan_service_process_step(void) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    switch (s_status.state) {
        case LORAWAN_STATE_JOINING:
#if !defined(HAVE_STM32WLXX_HAL)
            if (s_mock_join_accept_pending) {
                s_status.is_joined         = true;
                s_status.state             = LORAWAN_STATE_JOINED;
                s_status.dev_addr          = 0x26011234U;
                s_status.fcnt_up           = 0U;
                s_status.fcnt_down         = 0U;
                s_mock_join_accept_pending = false;
                (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_SHUTDOWN);
            }
#endif
            break;

        case LORAWAN_STATE_TX_UPLINK:
            s_status.fcnt_up++;
            s_status.state = LORAWAN_STATE_WAIT_RX1;
            (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_RX);
            break;

        case LORAWAN_STATE_WAIT_RX1:
            s_status.state = LORAWAN_STATE_WAIT_RX2;
            break;

        case LORAWAN_STATE_WAIT_RX2:
#if !defined(HAVE_STM32WLXX_HAL)
            if (s_tx_confirmed && s_mock_ack_pending) {
                s_mock_ack_pending  = false;
                s_last_ack_received = true;
                if (s_rx_callback != NULL) {
                    lorawan_rx_packet_t ack_pkt;
                    (void)memset(&ack_pkt, 0, sizeof(ack_pkt));
                    ack_pkt.fport    = s_tx_fport;
                    ack_pkt.length   = 0U;
                    ack_pkt.rssi_dbm = -75;
                    ack_pkt.snr_db   = 9;
                    ack_pkt.is_ack   = true;
                    s_rx_callback(&ack_pkt);
                }
            }
#endif
            s_status.state = LORAWAN_STATE_JOINED;
            (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_SHUTDOWN);
            break;

        case LORAWAN_STATE_UNJOINED:
        case LORAWAN_STATE_JOIN_FAILED:
        case LORAWAN_STATE_JOINED:
        case LORAWAN_STATE_ERROR:
        default:
            break;
    }

    return STATUS_OK;
}

status_t lorawan_get_status(lorawan_status_t *out_status) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (out_status == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    *out_status = s_status;
    return STATUS_OK;
}

status_t lorawan_get_credentials(lorawan_credentials_t *out_credentials) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (out_credentials == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    *out_credentials = s_credentials;
    return STATUS_OK;
}

status_t lorawan_inject_downlink(const lorawan_rx_packet_t *packet) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (packet == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    s_status.fcnt_down++;
    s_status.last_rssi_dbm = packet->rssi_dbm;
    s_status.last_snr_db   = packet->snr_db;

    if (packet->is_ack) {
        s_last_ack_received = true;
#if !defined(HAVE_STM32WLXX_HAL)
        s_mock_ack_pending = false;
#endif
    }

    if (s_rx_callback != NULL) {
        s_rx_callback(packet);
    }

    return STATUS_OK;
}

bool lorawan_is_ack_received(void) {
    return s_last_ack_received;
}

status_t lorawan_reset(void) {
    s_status.state      = LORAWAN_STATE_UNJOINED;
    s_status.is_joined  = false;
    s_status.fcnt_up    = 0U;
    s_status.fcnt_down  = 0U;
    s_status.dev_addr   = 0U;

    s_tx_length         = 0U;
    s_tx_fport          = 0U;
    s_tx_confirmed      = false;
    s_tx_retry_count    = 0U;
    s_last_ack_received = false;

#if !defined(HAVE_STM32WLXX_HAL)
    s_mock_join_accept_pending = false;
    s_mock_ack_pending         = false;
#endif

    (void)lorawan_set_rf_switch(LORAWAN_RF_MODE_SHUTDOWN);
    return STATUS_OK;
}
