/**
 * @file    telemetry_codec.c
 * @brief   LoRaWAN binary telemetry encoder and decoder implementation for STM32WLE5 SoC.
 */

#include "telemetry_codec.h"
#include <math.h>
#include <string.h>

/* ========================================================================== */
/* Static Helper Functions (Defensive Range Clamping)                         */
/* ========================================================================== */

static inline float clamp_float(float val, float min_val, float max_val) {
    if (val < min_val) {
        return min_val;
    }
    if (val > max_val) {
        return max_val;
    }
    return val;
}

static inline uint32_t clamp_u32(uint32_t val, uint32_t min_val, uint32_t max_val) {
    if (val < min_val) {
        return min_val;
    }
    if (val > max_val) {
        return max_val;
    }
    return val;
}

static inline int32_t clamp_i32(int32_t val, int32_t min_val, int32_t max_val) {
    if (val < min_val) {
        return min_val;
    }
    if (val > max_val) {
        return max_val;
    }
    return val;
}

/* ========================================================================== */
/* Public Codec Implementation                                                */
/* ========================================================================== */

status_t telemetry_encode_periodic(const telemetry_periodic_data_t *data,
                                  uint8_t *buffer,
                                  size_t buffer_size,
                                  size_t *encoded_len) {
    if (data == NULL || buffer == NULL || encoded_len == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (buffer_size < TELEMETRY_PERIODIC_PAYLOAD_SIZE) {
        return STATUS_ERROR_BUFFER_TOO_SMALL;
    }

    /* Zero destination buffer before serialization */
    memset(buffer, 0, TELEMETRY_PERIODIC_PAYLOAD_SIZE);

    /* ---------------------------------------------------------------------- */
    /* Bytes 0..1: Temperature (Signed 16-bit BE, 0.01 °C)                    */
    /* ---------------------------------------------------------------------- */
    float temp_clamped = clamp_float(data->temperature_c, TELEMETRY_TEMP_MIN_C, TELEMETRY_TEMP_MAX_C);
    int32_t raw_temp_32 = (int32_t)lroundf(temp_clamped * TELEMETRY_TEMP_SCALE);
    int16_t raw_temp = (int16_t)raw_temp_32;

    buffer[0] = (uint8_t)(((uint16_t)raw_temp >> 8) & 0xFFU);
    buffer[1] = (uint8_t)((uint16_t)raw_temp & 0xFFU);

    /* ---------------------------------------------------------------------- */
    /* Bytes 2..3: Relative Humidity (Unsigned 16-bit BE, 0.01 %RH)           */
    /* ---------------------------------------------------------------------- */
    float hum_clamped = clamp_float(data->humidity_pct, TELEMETRY_HUM_MIN_PCT, TELEMETRY_HUM_MAX_PCT);
    uint32_t raw_hum_32 = (uint32_t)lroundf(hum_clamped * TELEMETRY_HUM_SCALE);
    uint16_t raw_hum = (uint16_t)clamp_u32(raw_hum_32, 0U, 10000U);

    buffer[2] = (uint8_t)((raw_hum >> 8) & 0xFFU);
    buffer[3] = (uint8_t)(raw_hum & 0xFFU);

    /* ---------------------------------------------------------------------- */
    /* Bytes 4..5: Barometric Pressure (Unsigned 16-bit BE, Offset 300, 0.02) */
    /* ---------------------------------------------------------------------- */
    float press_clamped = clamp_float(data->pressure_hpa, TELEMETRY_PRESS_MIN_HPA, TELEMETRY_PRESS_MAX_HPA);
    uint32_t raw_press_32 = (uint32_t)lroundf((press_clamped - TELEMETRY_PRESS_OFFSET_HPA) / TELEMETRY_PRESS_STEP_HPA);
    uint16_t raw_press = (uint16_t)clamp_u32(raw_press_32, 0U, 40000U);

    buffer[4] = (uint8_t)((raw_press >> 8) & 0xFFU);
    buffer[5] = (uint8_t)(raw_press & 0xFFU);

    /* ---------------------------------------------------------------------- */
    /* Bytes 6..7: Ambient Lux (Unsigned 16-bit BE, Scale 2.0 Lux)            */
    /* ---------------------------------------------------------------------- */
    float lux_clamped = clamp_float(data->ambient_lux, TELEMETRY_LUX_MIN, TELEMETRY_LUX_MAX);
    uint32_t raw_lux_32 = (uint32_t)lroundf(lux_clamped / TELEMETRY_LUX_STEP);
    uint16_t raw_lux = (uint16_t)clamp_u32(raw_lux_32, 0U, 41500U);

    buffer[6] = (uint8_t)((raw_lux >> 8) & 0xFFU);
    buffer[7] = (uint8_t)(raw_lux & 0xFFU);

    /* ---------------------------------------------------------------------- */
    /* Byte 8: Interval Accumulated Rain (Unsigned 8-bit, Scale 0.2 mm)       */
    /* ---------------------------------------------------------------------- */
    float rain_clamped = clamp_float(data->rain_interval_mm, TELEMETRY_RAIN_MIN_MM, TELEMETRY_RAIN_MAX_MM);
    uint32_t raw_rain_32 = (uint32_t)lroundf(rain_clamped / TELEMETRY_RAIN_STEP_MM);
    uint8_t raw_rain = (uint8_t)clamp_u32(raw_rain_32, 0U, 255U);

    buffer[8] = raw_rain;

    /* ---------------------------------------------------------------------- */
    /* Byte 9: Forecast State [7:6] & Zambretti Index [5:0]                   */
    /* ---------------------------------------------------------------------- */
    uint8_t state_bits = (uint8_t)((uint8_t)data->forecast_state & 0x03U);
    uint8_t zambretti_clamped = (uint8_t)clamp_u32((uint32_t)data->zambretti_index,
                                                   (uint32_t)TELEMETRY_ZAMBRETTI_MIN,
                                                   (uint32_t)TELEMETRY_ZAMBRETTI_MAX);
    buffer[9] = (uint8_t)((state_bits << 6) | (zambretti_clamped & 0x3FU));

    /* ---------------------------------------------------------------------- */
    /* Byte 10: Solar Drop Flag [7] & CPI Probability [6:0]                   */
    /* ---------------------------------------------------------------------- */
    uint8_t solar_flag = data->solar_cloud_drop_alarm ? 0x80U : 0x00U;
    uint8_t cpi_clamped = (uint8_t)clamp_u32((uint32_t)data->cpi_prob_pct, 0U, (uint32_t)TELEMETRY_CPI_MAX_PCT);
    buffer[10] = (uint8_t)(solar_flag | (cpi_clamped & 0x7FU));

    /* ---------------------------------------------------------------------- */
    /* Byte 11: Reset Flag [7], Sensor Fault [6], Battery Vbat [5:0] (20mV)   */
    /* ---------------------------------------------------------------------- */
    uint8_t rst_flag = data->unexpected_reset ? 0x80U : 0x00U;
    uint8_t fault_flag = data->sensor_fault ? 0x40U : 0x00U;

    float vbat_clamped = clamp_float(data->battery_voltage_v, TELEMETRY_VBAT_MIN_V, TELEMETRY_VBAT_MAX_V);
    uint32_t raw_vbat_32 = (uint32_t)lroundf((vbat_clamped - TELEMETRY_VBAT_OFFSET_V) / TELEMETRY_VBAT_STEP_V);
    uint8_t raw_vbat = (uint8_t)clamp_u32(raw_vbat_32, 0U, 63U);

    buffer[11] = (uint8_t)(rst_flag | fault_flag | (raw_vbat & 0x3FU));

    *encoded_len = TELEMETRY_PERIODIC_PAYLOAD_SIZE;
    return STATUS_OK;
}

status_t telemetry_decode_periodic(const uint8_t *buffer,
                                  size_t buffer_size,
                                  telemetry_periodic_data_t *data) {
    if (buffer == NULL || data == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (buffer_size < TELEMETRY_PERIODIC_PAYLOAD_SIZE) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    memset(data, 0, sizeof(telemetry_periodic_data_t));

    /* ---------------------------------------------------------------------- */
    /* Bytes 0..1: Temperature (Signed 16-bit BE, 0.01 °C)                    */
    /* ---------------------------------------------------------------------- */
    uint16_t u16_temp = (uint16_t)(((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
    int16_t raw_temp = (int16_t)u16_temp;
    data->temperature_c = (float)raw_temp / TELEMETRY_TEMP_SCALE;

    /* ---------------------------------------------------------------------- */
    /* Bytes 2..3: Relative Humidity (Unsigned 16-bit BE, 0.01 %RH)           */
    /* ---------------------------------------------------------------------- */
    uint16_t raw_hum = (uint16_t)(((uint16_t)buffer[2] << 8) | (uint16_t)buffer[3]);
    data->humidity_pct = (float)raw_hum / TELEMETRY_HUM_SCALE;

    /* ---------------------------------------------------------------------- */
    /* Bytes 4..5: Barometric Pressure (Unsigned 16-bit BE, Offset 300, 0.02) */
    /* ---------------------------------------------------------------------- */
    uint16_t raw_press = (uint16_t)(((uint16_t)buffer[4] << 8) | (uint16_t)buffer[5]);
    data->pressure_hpa = TELEMETRY_PRESS_OFFSET_HPA + ((float)raw_press * TELEMETRY_PRESS_STEP_HPA);

    /* ---------------------------------------------------------------------- */
    /* Bytes 6..7: Ambient Lux (Unsigned 16-bit BE, Scale 2.0 Lux)            */
    /* ---------------------------------------------------------------------- */
    uint16_t raw_lux = (uint16_t)(((uint16_t)buffer[6] << 8) | (uint16_t)buffer[7]);
    data->ambient_lux = (float)raw_lux * TELEMETRY_LUX_STEP;

    /* ---------------------------------------------------------------------- */
    /* Byte 8: Interval Accumulated Rain (Unsigned 8-bit, Scale 0.2 mm)       */
    /* ---------------------------------------------------------------------- */
    uint8_t raw_rain = buffer[8];
    data->rain_interval_mm = (float)raw_rain * TELEMETRY_RAIN_STEP_MM;

    /* ---------------------------------------------------------------------- */
    /* Byte 9: Forecast State [7:6] & Zambretti Index [5:0]                   */
    /* ---------------------------------------------------------------------- */
    uint8_t state_code = (uint8_t)((buffer[9] >> 6) & 0x03U);
    data->forecast_state = (telemetry_rain_state_t)state_code;
    data->zambretti_index = (uint8_t)(buffer[9] & 0x3FU);

    /* ---------------------------------------------------------------------- */
    /* Byte 10: Solar Drop Flag [7] & CPI Probability [6:0]                   */
    /* ---------------------------------------------------------------------- */
    data->solar_cloud_drop_alarm = ((buffer[10] & 0x80U) != 0U);
    data->cpi_prob_pct = (uint8_t)(buffer[10] & 0x7FU);

    /* ---------------------------------------------------------------------- */
    /* Byte 11: Reset Flag [7], Sensor Fault [6], Battery Vbat [5:0] (20mV)   */
    /* ---------------------------------------------------------------------- */
    data->unexpected_reset = ((buffer[11] & 0x80U) != 0U);
    data->sensor_fault = ((buffer[11] & 0x40U) != 0U);
    uint8_t raw_vbat = (uint8_t)(buffer[11] & 0x3FU);
    data->battery_voltage_v = TELEMETRY_VBAT_OFFSET_V + ((float)raw_vbat * TELEMETRY_VBAT_STEP_V);

    return STATUS_OK;
}

/* ========================================================================== */
/* Urgent Storm Alert Codec (4 Bytes)                                         */
/* ========================================================================== */

status_t telemetry_encode_alert(const telemetry_alert_data_t *data,
                               uint8_t *buffer,
                               size_t buffer_size,
                               size_t *encoded_len) {
    if (data == NULL || buffer == NULL || encoded_len == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (buffer_size < TELEMETRY_ALERT_PAYLOAD_SIZE) {
        return STATUS_ERROR_BUFFER_TOO_SMALL;
    }

    memset(buffer, 0, TELEMETRY_ALERT_PAYLOAD_SIZE);

    /* ---------------------------------------------------------------------- */
    /* Byte 0: State [7:6] | Trigger Cause [5:3] | Sequence ID [2:0]          */
    /* ---------------------------------------------------------------------- */
    uint8_t state_bits = (uint8_t)(((uint8_t)data->alert_state & 0x03U) << 6);
    uint8_t trigger_bits = (uint8_t)(((uint8_t)data->trigger_cause & 0x07U) << 3);
    uint8_t seq_bits = (uint8_t)(data->alert_sequence_id & 0x07U);
    buffer[0] = (uint8_t)(state_bits | trigger_bits | seq_bits);

    /* ---------------------------------------------------------------------- */
    /* Byte 1: Solar Drop Flag [7] | CPI Probability [6:0]                    */
    /* ---------------------------------------------------------------------- */
    uint8_t solar_flag = data->solar_cloud_drop_alarm ? 0x80U : 0x00U;
    uint8_t cpi_clamped = (uint8_t)clamp_u32((uint32_t)data->cpi_prob_pct, 0U, (uint32_t)TELEMETRY_CPI_MAX_PCT);
    buffer[1] = (uint8_t)(solar_flag | (cpi_clamped & 0x7FU));

    /* ---------------------------------------------------------------------- */
    /* Byte 2: Barometric Pressure Rate dP/dt (Signed int8_t, 0.05 hPa/hr)    */
    /* ---------------------------------------------------------------------- */
    float rate_clamped = clamp_float(data->pressure_rate_hpa_per_h,
                                     TELEMETRY_PRESS_RATE_MIN_HPA_H,
                                     TELEMETRY_PRESS_RATE_MAX_HPA_H);
    int32_t raw_rate_32 = (int32_t)lroundf(rate_clamped / TELEMETRY_PRESS_RATE_STEP_HPA_H);
    int8_t raw_rate = (int8_t)clamp_i32(raw_rate_32, -128, 127);
    buffer[2] = (uint8_t)raw_rate;

    /* ---------------------------------------------------------------------- */
    /* Byte 3: Rain Intensity [7:6] | Sensor Fault [5] | Battery Vbat [4:0]   */
    /* ---------------------------------------------------------------------- */
    uint8_t rain_bits = (uint8_t)(((uint8_t)data->rain_intensity & 0x03U) << 6);
    uint8_t fault_bit = data->sensor_fault ? 0x20U : 0x00U;

    float vbat_clamped = clamp_float(data->battery_voltage_v,
                                     TELEMETRY_ALERT_VBAT_MIN_V,
                                     TELEMETRY_ALERT_VBAT_MAX_V);
    uint32_t raw_vbat_32 = (uint32_t)lroundf((vbat_clamped - TELEMETRY_ALERT_VBAT_OFFSET_V) / TELEMETRY_ALERT_VBAT_STEP_V);
    uint8_t raw_vbat = (uint8_t)clamp_u32(raw_vbat_32, 0U, 31U);

    buffer[3] = (uint8_t)(rain_bits | fault_bit | (raw_vbat & 0x1FU));

    *encoded_len = TELEMETRY_ALERT_PAYLOAD_SIZE;
    return STATUS_OK;
}

status_t telemetry_decode_alert(const uint8_t *buffer,
                               size_t buffer_size,
                               telemetry_alert_data_t *data) {
    if (buffer == NULL || data == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    if (buffer_size < TELEMETRY_ALERT_PAYLOAD_SIZE) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    memset(data, 0, sizeof(telemetry_alert_data_t));

    /* Byte 0: State [7:6], Trigger [5:3], Seq ID [2:0] */
    data->alert_state = (telemetry_rain_state_t)((buffer[0] >> 6) & 0x03U);
    data->trigger_cause = (telemetry_alert_trigger_t)((buffer[0] >> 3) & 0x07U);
    data->alert_sequence_id = (uint8_t)(buffer[0] & 0x07U);

    /* Byte 1: Solar Drop Flag [7], CPI Probability [6:0] */
    data->solar_cloud_drop_alarm = ((buffer[1] & 0x80U) != 0U);
    data->cpi_prob_pct = (uint8_t)(buffer[1] & 0x7FU);

    /* Byte 2: Barometric Pressure Rate dP/dt */
    int8_t raw_rate = (int8_t)buffer[2];
    data->pressure_rate_hpa_per_h = (float)raw_rate * TELEMETRY_PRESS_RATE_STEP_HPA_H;

    /* Byte 3: Rain Intensity [7:6], Sensor Fault [5], Battery Vbat [4:0] */
    data->rain_intensity = (telemetry_rain_intensity_t)((buffer[3] >> 6) & 0x03U);
    data->sensor_fault = ((buffer[3] & 0x20U) != 0U);
    uint8_t raw_vbat = (uint8_t)(buffer[3] & 0x1FU);
    data->battery_voltage_v = TELEMETRY_ALERT_VBAT_OFFSET_V + ((float)raw_vbat * TELEMETRY_ALERT_VBAT_STEP_V);

    return STATUS_OK;
}
