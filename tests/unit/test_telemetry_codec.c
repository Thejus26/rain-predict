/**
 * @file    test_telemetry_codec.c
 * @brief   Unit test suite for LoRaWAN binary telemetry serializer and deserializer.
 * @details Validates 12-byte periodic telemetry and 4-byte urgent alert binary encoding under Unity.
 */

#include "unity.h"
#include "telemetry_codec.h"
#include <string.h>
#include <math.h>

/* ========================================================================== */
/* Test Harness Setup and Teardown                                            */
/* ========================================================================== */

void setUp(void) {
    /* No dynamic memory allocation to reset */
}

void tearDown(void) {
    /* Verify clean execution */
}

/* ========================================================================== */
/* Periodic Telemetry Tests (12 Bytes)                                        */
/* ========================================================================== */

/**
 * @brief Verify NULL pointer guards on periodic encoder and decoder.
 */
static void test_periodic_codec_null_guards(void) {
    telemetry_periodic_data_t data;
    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    memset(&data, 0, sizeof(data));

    /* Encoder NULL checks */
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_encode_periodic(NULL, buffer, sizeof(buffer), &encoded_len));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_encode_periodic(&data, NULL, sizeof(buffer), &encoded_len));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_encode_periodic(&data, buffer, sizeof(buffer), NULL));

    /* Decoder NULL checks */
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_decode_periodic(NULL, sizeof(buffer), &data));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_decode_periodic(buffer, sizeof(buffer), NULL));
}

/**
 * @brief Verify buffer bounds rejection for truncated destination arrays.
 */
static void test_periodic_codec_buffer_underflow(void) {
    telemetry_periodic_data_t data;
    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    memset(&data, 0, sizeof(data));

    /* Buffer sizes less than 12 bytes must be rejected */
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUFFER_TOO_SMALL, telemetry_encode_periodic(&data, buffer, 11, &encoded_len));
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUFFER_TOO_SMALL, telemetry_encode_periodic(&data, buffer, 0, &encoded_len));

    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, telemetry_decode_periodic(buffer, 11, &data));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, telemetry_decode_periodic(buffer, 0, &data));
}

/**
 * @brief Test exact hexadecimal bit-match for nominal tropical daytime profile.
 */
static void test_periodic_encode_nominal_daytime_hex_match(void) {
    telemetry_periodic_data_t data = {
        .temperature_c          = 24.50f,
        .humidity_pct           = 85.25f,
        .pressure_hpa           = 945.50f,
        .ambient_lux            = 45000.0f,
        .rain_interval_mm       = 0.0f,
        .forecast_state         = RAIN_ALERT_POSSIBLE,
        .zambretti_index        = 14U,
        .cpi_prob_pct           = 45U,
        .solar_cloud_drop_alarm = false,
        .battery_voltage_v      = 3.30f,
        .sensor_fault           = false,
        .unexpected_reset       = false
    };

    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    const uint8_t expected_hex[TELEMETRY_PERIODIC_PAYLOAD_SIZE] = {
        0x09, 0x92, 0x21, 0x4D, 0x7E, 0x13, 0x57, 0xE4, 0x00, 0x4E, 0x2D, 0x28
    };

    status_t status = telemetry_encode_periodic(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_EQUAL(TELEMETRY_PERIODIC_PAYLOAD_SIZE, encoded_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_hex, buffer, TELEMETRY_PERIODIC_PAYLOAD_SIZE);
}

/**
 * @brief Test sub-zero temperature two's-complement sign preservation.
 */
static void test_periodic_encode_subzero_temperature(void) {
    telemetry_periodic_data_t data = {
        .temperature_c          = -12.75f,
        .humidity_pct           = 99.90f,
        .pressure_hpa           = 810.20f,
        .ambient_lux            = 0.0f,
        .rain_interval_mm       = 2.4f,
        .forecast_state         = RAIN_ALERT_IMMINENT,
        .zambretti_index        = 22U,
        .cpi_prob_pct           = 78U,
        .solar_cloud_drop_alarm = false,
        .battery_voltage_v      = 3.10f,
        .sensor_fault           = false,
        .unexpected_reset       = false
    };

    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    const uint8_t expected_hex[TELEMETRY_PERIODIC_PAYLOAD_SIZE] = {
        0xFB, 0x05, 0x27, 0x06, 0x63, 0xA6, 0x00, 0x00, 0x0C, 0x96, 0x4E, 0x1E
    };

    status_t status = telemetry_encode_periodic(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_hex, buffer, TELEMETRY_PERIODIC_PAYLOAD_SIZE);

    /* Verify decode accurately reconstructs negative temperature */
    telemetry_periodic_data_t decoded;
    status = telemetry_decode_periodic(buffer, sizeof(buffer), &decoded);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, -12.75f, decoded.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 99.90f, decoded.humidity_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 810.20f, decoded.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.4f, decoded.rain_interval_mm);
}

/**
 * @brief Test bitfield packing, flag masking, and isolation in Bytes 9, 10, and 11.
 */
static void test_periodic_encode_bitfield_masks(void) {
    telemetry_periodic_data_t data = {
        .temperature_c          = 20.00f,
        .humidity_pct           = 50.00f,
        .pressure_hpa           = 1000.00f,
        .ambient_lux            = 1000.0f,
        .rain_interval_mm       = 1.0f,
        .forecast_state         = RAIN_ALERT_ACTIVE_RAIN, /* 3 (bits 7:6 = 11) */
        .zambretti_index        = 26U,                    /* 26 (bits 5:0 = 0x1A) -> 0xDA */
        .cpi_prob_pct           = 100U,                   /* 100 (bits 6:0 = 0x64) */
        .solar_cloud_drop_alarm = true,                   /* true (bit 7 = 1) -> 0xE4 */
        .battery_voltage_v      = 3.76f,                  /* raw 63 (bits 5:0 = 0x3F) */
        .sensor_fault           = true,                   /* true (bit 6 = 1) */
        .unexpected_reset       = true                    /* true (bit 7 = 1) -> 0xFF */
    };

    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    status_t status = telemetry_encode_periodic(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);

    TEST_ASSERT_EQUAL_HEX8(0xDA, buffer[9]);
    TEST_ASSERT_EQUAL_HEX8(0xE4, buffer[10]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[11]);

    telemetry_periodic_data_t decoded;
    status = telemetry_decode_periodic(buffer, sizeof(buffer), &decoded);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_EQUAL(RAIN_ALERT_ACTIVE_RAIN, decoded.forecast_state);
    TEST_ASSERT_EQUAL_UINT8(26U, decoded.zambretti_index);
    TEST_ASSERT_EQUAL_UINT8(100U, decoded.cpi_prob_pct);
    TEST_ASSERT_TRUE(decoded.solar_cloud_drop_alarm);
    TEST_ASSERT_TRUE(decoded.unexpected_reset);
    TEST_ASSERT_TRUE(decoded.sensor_fault);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.76f, decoded.battery_voltage_v);
}

/**
 * @brief Test maximum physical boundary clamping (no integer wrap-around).
 */
static void test_periodic_encode_upper_boundary_clamping(void) {
    telemetry_periodic_data_t data = {
        .temperature_c          = 150.0f,    /* Exceeds 85.0 °C */
        .humidity_pct           = 120.0f,    /* Exceeds 100.0 % */
        .pressure_hpa           = 1300.0f,   /* Exceeds 1100.0 hPa */
        .ambient_lux            = 200000.0f, /* Exceeds 83,000 Lux */
        .rain_interval_mm       = 100.0f,    /* Exceeds 51.0 mm */
        .forecast_state         = RAIN_ALERT_ACTIVE_RAIN,
        .zambretti_index        = 30U,       /* Exceeds 26 */
        .cpi_prob_pct           = 150U,      /* Exceeds 100% */
        .solar_cloud_drop_alarm = true,
        .battery_voltage_v      = 5.0f,      /* Exceeds 3.76 V */
        .sensor_fault           = false,
        .unexpected_reset       = false
    };

    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    status_t status = telemetry_encode_periodic(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);

    telemetry_periodic_data_t decoded;
    status = telemetry_decode_periodic(buffer, sizeof(buffer), &decoded);
    TEST_ASSERT_EQUAL(STATUS_OK, status);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 85.00f, decoded.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.00f, decoded.humidity_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1100.00f, decoded.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 83000.0f, decoded.ambient_lux);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 51.0f, decoded.rain_interval_mm);
    TEST_ASSERT_EQUAL_UINT8(26U, decoded.zambretti_index);
    TEST_ASSERT_EQUAL_UINT8(100U, decoded.cpi_prob_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 3.76f, decoded.battery_voltage_v);
}

/**
 * @brief Test minimum physical boundary clamping (no integer underflow).
 */
static void test_periodic_encode_lower_boundary_clamping(void) {
    telemetry_periodic_data_t data = {
        .temperature_c          = -80.0f,   /* Below -40.0 °C */
        .humidity_pct           = -20.0f,   /* Below 0.0 % */
        .pressure_hpa           = 100.0f,   /* Below 300.0 hPa */
        .ambient_lux            = -50.0f,   /* Below 0.0 Lux */
        .rain_interval_mm       = -5.0f,    /* Below 0.0 mm */
        .forecast_state         = RAIN_ALERT_UNLIKELY,
        .zambretti_index        = 0U,       /* Below 1 */
        .cpi_prob_pct           = 0U,
        .solar_cloud_drop_alarm = false,
        .battery_voltage_v      = 1.50f,    /* Below 2.50 V */
        .sensor_fault           = false,
        .unexpected_reset       = false
    };

    uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    status_t status = telemetry_encode_periodic(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);

    telemetry_periodic_data_t decoded;
    status = telemetry_decode_periodic(buffer, sizeof(buffer), &decoded);
    TEST_ASSERT_EQUAL(STATUS_OK, status);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.00f, decoded.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.00f, decoded.humidity_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 300.00f, decoded.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, decoded.ambient_lux);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 0.0f, decoded.rain_interval_mm);
    TEST_ASSERT_EQUAL_UINT8(1U, decoded.zambretti_index);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 2.50f, decoded.battery_voltage_v);
}

/**
 * @brief Test comprehensive lossless round-trip encoding and decoding.
 */
static void test_periodic_roundtrip_lossless_fidelity(void) {
    const float test_temps[] = { -39.99f, -10.50f, 0.00f, 15.33f, 28.75f, 42.10f, 84.99f };
    const float test_pressures[] = { 300.00f, 650.40f, 950.22f, 1013.25f, 1099.98f };
    const float test_humidities[] = { 0.00f, 25.40f, 65.50f, 92.80f, 100.00f };

    for (size_t t = 0; t < sizeof(test_temps)/sizeof(test_temps[0]); t++) {
        for (size_t p = 0; p < sizeof(test_pressures)/sizeof(test_pressures[0]); p++) {
            for (size_t h = 0; h < sizeof(test_humidities)/sizeof(test_humidities[0]); h++) {
                telemetry_periodic_data_t src = {
                    .temperature_c          = test_temps[t],
                    .humidity_pct           = test_humidities[h],
                    .pressure_hpa           = test_pressures[p],
                    .ambient_lux            = 32000.0f,
                    .rain_interval_mm       = 4.6f,
                    .forecast_state         = RAIN_ALERT_POSSIBLE,
                    .zambretti_index        = 12U,
                    .cpi_prob_pct           = 55U,
                    .solar_cloud_drop_alarm = false,
                    .battery_voltage_v      = 3.28f,
                    .sensor_fault           = false,
                    .unexpected_reset       = false
                };

                uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
                size_t encoded_len = 0;
                status_t status = telemetry_encode_periodic(&src, buffer, sizeof(buffer), &encoded_len);
                TEST_ASSERT_EQUAL(STATUS_OK, status);

                telemetry_periodic_data_t dst;
                status = telemetry_decode_periodic(buffer, sizeof(buffer), &dst);
                TEST_ASSERT_EQUAL(STATUS_OK, status);

                TEST_ASSERT_FLOAT_WITHIN(0.006f, src.temperature_c, dst.temperature_c);
                TEST_ASSERT_FLOAT_WITHIN(0.006f, src.humidity_pct, dst.humidity_pct);
                TEST_ASSERT_FLOAT_WITHIN(0.015f, src.pressure_hpa, dst.pressure_hpa);
                TEST_ASSERT_FLOAT_WITHIN(1.0f, src.ambient_lux, dst.ambient_lux);
                TEST_ASSERT_FLOAT_WITHIN(0.15f, src.rain_interval_mm, dst.rain_interval_mm);
                TEST_ASSERT_FLOAT_WITHIN(0.015f, src.battery_voltage_v, dst.battery_voltage_v);
            }
        }
    }
}

/* ========================================================================== */
/* Urgent Storm Alert Tests (4 Bytes)                                         */
/* ========================================================================== */

/**
 * @brief Verify NULL pointer guards on alert encoder and decoder.
 */
static void test_alert_codec_null_guards(void) {
    telemetry_alert_data_t data;
    uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    memset(&data, 0, sizeof(data));

    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_encode_alert(NULL, buffer, sizeof(buffer), &encoded_len));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_encode_alert(&data, NULL, sizeof(buffer), &encoded_len));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_encode_alert(&data, buffer, sizeof(buffer), NULL));

    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_decode_alert(NULL, sizeof(buffer), &data));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, telemetry_decode_alert(buffer, sizeof(buffer), NULL));
}

/**
 * @brief Verify buffer bounds rejection for truncated alert destination arrays.
 */
static void test_alert_codec_buffer_underflow(void) {
    telemetry_alert_data_t data;
    uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    memset(&data, 0, sizeof(data));

    TEST_ASSERT_EQUAL(STATUS_ERROR_BUFFER_TOO_SMALL, telemetry_encode_alert(&data, buffer, 3, &encoded_len));
    TEST_ASSERT_EQUAL(STATUS_ERROR_BUFFER_TOO_SMALL, telemetry_encode_alert(&data, buffer, 0, &encoded_len));

    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, telemetry_decode_alert(buffer, 3, &data));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, telemetry_decode_alert(buffer, 0, &data));
}

/**
 * @brief Test exact hexadecimal bit-match for severe convective storm alert.
 */
static void test_alert_encode_severe_storm_hex_match(void) {
    telemetry_alert_data_t data = {
        .alert_state            = RAIN_ALERT_IMMINENT,
        .trigger_cause          = ALERT_TRIGGER_CPI_THRESHOLD,
        .alert_sequence_id      = 3U,
        .cpi_prob_pct           = 88U,
        .solar_cloud_drop_alarm = true,
        .pressure_rate_hpa_per_h= -3.50f,
        .rain_intensity         = RAIN_INTENSITY_LIGHT,
        .sensor_fault           = false,
        .battery_voltage_v      = 3.30f
    };

    uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    const uint8_t expected_hex[TELEMETRY_ALERT_PAYLOAD_SIZE] = {
        0x8B, 0xD8, 0xBA, 0x54
    };

    status_t status = telemetry_encode_alert(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_EQUAL(TELEMETRY_ALERT_PAYLOAD_SIZE, encoded_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_hex, buffer, TELEMETRY_ALERT_PAYLOAD_SIZE);
}

/**
 * @brief Test signed pressure rate quantization, negative drop rates, and clamping.
 */
static void test_alert_encode_signed_pressure_rate(void) {
    telemetry_alert_data_t data = {
        .alert_state            = RAIN_ALERT_IMMINENT,
        .trigger_cause          = ALERT_TRIGGER_PRESSURE_PLUNGE,
        .alert_sequence_id      = 5U,
        .cpi_prob_pct           = 92U,
        .solar_cloud_drop_alarm = false,
        .pressure_rate_hpa_per_h= -5.20f, /* raw -104 = 0x98 */
        .rain_intensity         = RAIN_INTENSITY_NONE,
        .sensor_fault           = false,
        .battery_voltage_v      = 3.26f   /* raw 19 = 0x13 */
    };

    uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
    size_t encoded_len = 0;

    const uint8_t expected_hex[TELEMETRY_ALERT_PAYLOAD_SIZE] = {
        0x95, 0x5C, 0x98, 0x13
    };

    status_t status = telemetry_encode_alert(&data, buffer, sizeof(buffer), &encoded_len);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_hex, buffer, TELEMETRY_ALERT_PAYLOAD_SIZE);

    telemetry_alert_data_t decoded;
    status = telemetry_decode_alert(buffer, sizeof(buffer), &decoded);
    TEST_ASSERT_EQUAL(STATUS_OK, status);
    TEST_ASSERT_EQUAL(ALERT_TRIGGER_PRESSURE_PLUNGE, decoded.trigger_cause);
    TEST_ASSERT_EQUAL_UINT8(5U, decoded.alert_sequence_id);
    TEST_ASSERT_FLOAT_WITHIN(0.025f, -5.20f, decoded.pressure_rate_hpa_per_h);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 3.26f, decoded.battery_voltage_v);
}

/**
 * @brief Test full alert round-trip encoding and decoding across all enum states.
 */
static void test_alert_roundtrip_lossless_fidelity(void) {
    for (uint8_t state = 0; state <= 3; state++) {
        for (uint8_t trig = 0; trig <= 7; trig++) {
            telemetry_alert_data_t src = {
                .alert_state            = (telemetry_rain_state_t)state,
                .trigger_cause          = (telemetry_alert_trigger_t)trig,
                .alert_sequence_id      = (uint8_t)(trig & 0x07U),
                .cpi_prob_pct           = (uint8_t)(trig * 12U),
                .solar_cloud_drop_alarm = (trig % 2 != 0),
                .pressure_rate_hpa_per_h= (float)trig * -0.80f,
                .rain_intensity         = (telemetry_rain_intensity_t)(state),
                .sensor_fault           = (trig == 7),
                .battery_voltage_v      = 3.10f + (float)trig * 0.05f
            };

            uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
            size_t encoded_len = 0;
            status_t status = telemetry_encode_alert(&src, buffer, sizeof(buffer), &encoded_len);
            TEST_ASSERT_EQUAL(STATUS_OK, status);

            telemetry_alert_data_t dst;
            status = telemetry_decode_alert(buffer, sizeof(buffer), &dst);
            TEST_ASSERT_EQUAL(STATUS_OK, status);

            TEST_ASSERT_EQUAL(src.alert_state, dst.alert_state);
            TEST_ASSERT_EQUAL(src.trigger_cause, dst.trigger_cause);
            TEST_ASSERT_EQUAL_UINT8(src.alert_sequence_id, dst.alert_sequence_id);
            TEST_ASSERT_EQUAL_UINT8(src.cpi_prob_pct, dst.cpi_prob_pct);
            TEST_ASSERT_EQUAL(src.solar_cloud_drop_alarm, dst.solar_cloud_drop_alarm);
            TEST_ASSERT_FLOAT_WITHIN(0.03f, src.pressure_rate_hpa_per_h, dst.pressure_rate_hpa_per_h);
            TEST_ASSERT_EQUAL(src.rain_intensity, dst.rain_intensity);
            TEST_ASSERT_EQUAL(src.sensor_fault, dst.sensor_fault);
            TEST_ASSERT_FLOAT_WITHIN(0.025f, src.battery_voltage_v, dst.battery_voltage_v);
        }
    }
}

/* ========================================================================== */
/* Main Unity Test Runner Execution                                           */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Periodic Telemetry Suite */
    RUN_TEST(test_periodic_codec_null_guards);
    RUN_TEST(test_periodic_codec_buffer_underflow);
    RUN_TEST(test_periodic_encode_nominal_daytime_hex_match);
    RUN_TEST(test_periodic_encode_subzero_temperature);
    RUN_TEST(test_periodic_encode_bitfield_masks);
    RUN_TEST(test_periodic_encode_upper_boundary_clamping);
    RUN_TEST(test_periodic_encode_lower_boundary_clamping);
    RUN_TEST(test_periodic_roundtrip_lossless_fidelity);

    /* Urgent Storm Alert Suite */
    RUN_TEST(test_alert_codec_null_guards);
    RUN_TEST(test_alert_codec_buffer_underflow);
    RUN_TEST(test_alert_encode_severe_storm_hex_match);
    RUN_TEST(test_alert_encode_signed_pressure_rate);
    RUN_TEST(test_alert_roundtrip_lossless_fidelity);

    return UNITY_END();
}
