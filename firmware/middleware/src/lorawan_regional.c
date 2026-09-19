/**
 * @file    lorawan_regional.c
 * @brief   LoRaWAN regional channel plans, duty-cycle tracker, and ADR implementation.
 * @details Conforms to C99 and MISRA C standards with zero dynamic memory allocation.
 */

#include "lorawan_regional.h"
#include <math.h>
#include <string.h>

/* ========================================================================== */
/* Static Module State Context                                                */
/* ========================================================================== */

static lorawan_region_t          s_active_region = LORAWAN_REGION_IN865;
static lorawan_channel_t         s_channel_table[LORAWAN_MAX_CHANNELS];
static uint8_t                   s_channel_count = 0U;
static uint8_t                   s_current_channel_idx = 0U;
static bool                      s_is_initialized = false;

static lorawan_adr_state_t        s_adr_state = {
    .adr_ack_cnt      = 0U,
    .adr_enabled      = true,
    .adr_ack_req      = false,
    .current_dr       = 5U,
    .current_tx_power = 14,
    .nb_trans         = 1U
};

static lorawan_duty_cycle_state_t s_duty_state = {
    .accumulated_toa_ms   = 0U,
    .last_tx_timestamp_ms = 0U,
    .min_off_time_ms      = 0U,
    .duty_cycle_max_pct   = 1.0f
};

/* ========================================================================== */
/* Public API Implementation                                                  */
/* ========================================================================== */

status_t lorawan_regional_init(lorawan_region_t region, uint8_t sub_band) {
    if ((region != LORAWAN_REGION_IN865) &&
        (region != LORAWAN_REGION_EU868) &&
        (region != LORAWAN_REGION_US915)) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    s_active_region = region;
    (void)memset(s_channel_table, 0, sizeof(s_channel_table));
    s_channel_count = 0U;
    s_current_channel_idx = 0U;

    switch (region) {
        case LORAWAN_REGION_IN865:
            /* 3 Default Indian Channels */
            s_channel_table[0] = (lorawan_channel_t){ 865062500U, 0U, 5U, true };
            s_channel_table[1] = (lorawan_channel_t){ 865402500U, 0U, 5U, true };
            s_channel_table[2] = (lorawan_channel_t){ 865985000U, 0U, 5U, true };
            s_channel_count = 3U;
            s_adr_state.current_tx_power = 22; /* High-Power PA */
            s_duty_state.duty_cycle_max_pct = 1.0f;
            s_adr_state.current_dr = 5U;
            break;

        case LORAWAN_REGION_EU868:
            /* 3 Default EU/Sri Lanka/Kenya Channels */
            s_channel_table[0] = (lorawan_channel_t){ 868100000U, 0U, 5U, true };
            s_channel_table[1] = (lorawan_channel_t){ 868300000U, 0U, 5U, true };
            s_channel_table[2] = (lorawan_channel_t){ 868500000U, 0U, 5U, true };
            s_channel_count = 3U;
            s_adr_state.current_tx_power = 14; /* 14 dBm ERP (+16 dBm EIRP) */
            s_duty_state.duty_cycle_max_pct = 1.0f;
            s_adr_state.current_dr = 5U;
            break;

        case LORAWAN_REGION_US915:
            /* Sub-Band 2 (Channels 8..15) */
            (void)sub_band;
            for (uint8_t i = 0U; i < 8U; i++) {
                s_channel_table[i] = (lorawan_channel_t){ 903900000U + ((uint32_t)i * 200000U), 0U, 3U, true };
            }
            s_channel_count = 8U;
            s_adr_state.current_tx_power = 20;
            s_duty_state.duty_cycle_max_pct = 100.0f; /* 400ms dwell time */
            s_adr_state.current_dr = 3U;
            break;

        default:
            return STATUS_ERROR_INVALID_PARAM;
    }

    s_adr_state.adr_ack_cnt = 0U;
    s_adr_state.adr_enabled = true;
    s_adr_state.adr_ack_req = false;
    s_adr_state.nb_trans    = 1U;

    s_duty_state.accumulated_toa_ms   = 0U;
    s_duty_state.last_tx_timestamp_ms = 0U;
    s_duty_state.min_off_time_ms      = 0U;

    s_is_initialized = true;
    return STATUS_OK;
}

status_t lorawan_regional_get_tx_channel(uint32_t *out_freq_hz, uint8_t *out_dr) {
    if ((out_freq_hz == NULL) || (out_dr == NULL)) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized || (s_channel_count == 0U)) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    /* Pseudo-random / round-robin hopping across enabled channels */
    uint8_t start_idx = s_current_channel_idx;
    for (uint8_t i = 0U; i < s_channel_count; i++) {
        uint8_t idx = (uint8_t)((start_idx + i) % s_channel_count);
        if (s_channel_table[idx].enabled) {
            *out_freq_hz = s_channel_table[idx].frequency_hz;
            *out_dr      = s_adr_state.current_dr;
            s_current_channel_idx = (uint8_t)((idx + 1U) % s_channel_count);
            return STATUS_OK;
        }
    }

    return STATUS_ERROR_HARDWARE;
}

status_t lorawan_regional_get_rx1_params(uint32_t tx_freq_hz, uint8_t tx_dr,
                                         uint32_t *out_rx1_freq, uint8_t *out_rx1_dr) {
    if ((out_rx1_freq == NULL) || (out_rx1_dr == NULL)) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    if (s_active_region == LORAWAN_REGION_US915) {
        /* US915: RX1 Frequency = 923.3 MHz + (Channel % 8) * 600 kHz */
        uint8_t ch = (tx_freq_hz >= 903900000U) ? (uint8_t)((tx_freq_hz - 903900000U) / 200000U) : 0U;
        *out_rx1_freq = 923300000U + ((uint32_t)(ch % 8U) * 600000U);
        *out_rx1_dr   = (tx_dr <= 10U) ? (uint8_t)(10U - tx_dr) : 0U;
    } else {
        /* IN865 and EU868: Same frequency as TX, same DR */
        *out_rx1_freq = tx_freq_hz;
        *out_rx1_dr   = tx_dr;
    }

    return STATUS_OK;
}

status_t lorawan_regional_get_rx2_params(uint32_t *out_rx2_freq, uint8_t *out_rx2_dr) {
    if ((out_rx2_freq == NULL) || (out_rx2_dr == NULL)) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    switch (s_active_region) {
        case LORAWAN_REGION_IN865:
            *out_rx2_freq = 866550000U;
            *out_rx2_dr   = 2U; /* SF10 / 125 kHz */
            break;
        case LORAWAN_REGION_EU868:
            *out_rx2_freq = 869525000U;
            *out_rx2_dr   = 0U; /* SF12 / 125 kHz (or DR3 869.525) */
            break;
        case LORAWAN_REGION_US915:
        default:
            *out_rx2_freq = 923300000U;
            *out_rx2_dr   = 8U; /* SF12 / 500 kHz */
            break;
    }

    return STATUS_OK;
}

uint32_t lorawan_calc_time_on_air_ms(uint8_t payload_bytes, uint8_t dr) {
    uint8_t sf = (dr <= 5U) ? (uint8_t)(12U - dr) : 7U; /* Maps DR0..DR5 to SF12..SF7 */
    if (sf < 7U) {
        sf = 7U;
    }
    if (sf > 12U) {
        sf = 12U;
    }

    float bw_hz = 125000.0f;
    float t_sym_ms = (float)(1U << sf) / (bw_hz / 1000.0f);
    float t_preamble_ms = (8.0f + 4.25f) * t_sym_ms;

    int32_t de = (sf >= 11U) ? 1 : 0;
    int32_t num = (8 * (int32_t)payload_bytes) - (4 * (int32_t)sf) + 28 + 16;
    int32_t den = 4 * ((int32_t)sf - (2 * de));

    float sym_payload = (float)num / (float)den;
    if (sym_payload < 0.0f) {
        sym_payload = 0.0f;
    }
    int32_t payload_sym_count = 8 + ((int32_t)ceilf(sym_payload) * 5); /* CR 4/5 */

    float t_payload_ms = (float)payload_sym_count * t_sym_ms;
    return (uint32_t)ceilf(t_preamble_ms + t_payload_ms);
}

status_t lorawan_regional_update_duty_cycle(uint32_t freq_hz, uint32_t toa_ms) {
    (void)freq_hz;
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    s_duty_state.accumulated_toa_ms += toa_ms;

    /* For 1% duty cycle: Toff = Toa * (1 - 0.01) / 0.01 = 99 * Toa */
    if (s_duty_state.duty_cycle_max_pct <= 1.0f) {
        s_duty_state.min_off_time_ms = toa_ms * 99U;
    } else {
        s_duty_state.min_off_time_ms = 0U;
    }

    return STATUS_OK;
}

uint32_t lorawan_regional_get_off_time_ms(uint32_t freq_hz) {
    (void)freq_hz;
    return s_duty_state.min_off_time_ms;
}

status_t lorawan_adr_on_uplink(bool *out_adr_ack_req) {
    if (out_adr_ack_req == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    if (!s_adr_state.adr_enabled) {
        *out_adr_ack_req = false;
        return STATUS_OK;
    }

    s_adr_state.adr_ack_cnt++;

    if (s_adr_state.adr_ack_cnt >= LORAWAN_ADR_ACK_LIMIT) {
        s_adr_state.adr_ack_req = true;
    }

    if (s_adr_state.adr_ack_cnt >= (LORAWAN_ADR_ACK_LIMIT + LORAWAN_ADR_ACK_DELAY)) {
        /* Fallback step down */
        if (s_adr_state.current_dr > 0U) {
            s_adr_state.current_dr--;
        }
        s_adr_state.current_tx_power = 22; /* Max power */
        s_adr_state.adr_ack_cnt = LORAWAN_ADR_ACK_LIMIT;
    }

    *out_adr_ack_req = s_adr_state.adr_ack_req;
    return STATUS_OK;
}

status_t lorawan_adr_on_downlink(int16_t rssi_dbm, int8_t snr_db) {
    (void)rssi_dbm;
    (void)snr_db;
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    s_adr_state.adr_ack_cnt = 0U;
    s_adr_state.adr_ack_req = false;
    return STATUS_OK;
}

status_t lorawan_adr_process_link_adr_req(const uint8_t *payload, uint8_t *out_ans) {
    if ((payload == NULL) || (out_ans == NULL)) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    uint8_t  target_dr  = (uint8_t)((payload[0] >> 4) & 0x0FU);
    uint8_t  target_pwr = (uint8_t)(payload[0] & 0x0FU);
    uint16_t ch_mask    = (uint16_t)((uint16_t)payload[1] | ((uint16_t)payload[2] << 8));
    uint8_t  nb_trans   = (uint8_t)(payload[3] & 0x0FU);

    bool dr_ack  = (target_dr <= 5U);
    bool pwr_ack = (target_pwr <= 7U);
    bool ch_ack  = (ch_mask != 0U);

    if (dr_ack && pwr_ack && ch_ack) {
        s_adr_state.current_dr = target_dr;
        if (nb_trans > 0U) {
            s_adr_state.nb_trans = nb_trans;
        }
        for (uint8_t i = 0U; (i < s_channel_count) && (i < 16U); i++) {
            s_channel_table[i].enabled = ((ch_mask & (1U << i)) != 0U);
        }
    }

    /* Bit 0: ChMask ACK, Bit 1: DR ACK, Bit 2: Power ACK */
    *out_ans = (uint8_t)((ch_ack  ? 0x01U : 0x00U) | 
                         (dr_ack  ? 0x02U : 0x00U) | 
                         (pwr_ack ? 0x04U : 0x00U));

    return STATUS_OK;
}

status_t lorawan_regional_set_dr(uint8_t dr) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (dr > 5U) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    s_adr_state.current_dr = dr;
    return STATUS_OK;
}

status_t lorawan_regional_set_tx_power(int8_t power_dbm) {
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if ((power_dbm < 0) || (power_dbm > 22)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    s_adr_state.current_tx_power = power_dbm;
    return STATUS_OK;
}

status_t lorawan_regional_get_adr_state(lorawan_adr_state_t *out_state) {
    if (out_state == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    *out_state = s_adr_state;
    return STATUS_OK;
}

status_t lorawan_regional_get_duty_cycle_state(lorawan_duty_cycle_state_t *out_state) {
    if (out_state == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    *out_state = s_duty_state;
    return STATUS_OK;
}

status_t lorawan_regional_get_channel(uint8_t index, lorawan_channel_t *out_channel) {
    if (out_channel == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (!s_is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }
    if (index >= s_channel_count) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    *out_channel = s_channel_table[index];
    return STATUS_OK;
}

uint8_t lorawan_regional_get_channel_count(void) {
    return s_channel_count;
}

lorawan_region_t lorawan_regional_get_active_region(void) {
    return s_active_region;
}
