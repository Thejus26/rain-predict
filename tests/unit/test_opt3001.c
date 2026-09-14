/**
 * @file    test_opt3001.c
 * @brief   ThrowTheSwitch Unity unit test suite for TI OPT3001 Driver (S4-T2.1 - S4-T2.4).
 * @details Validates device ID verification, single-shot acquisition, conversion polling,
 *          timeout handling, raw register unpacking, exponential lux math, broadband
 *          solar irradiance, telemetry serialization, day/night hysteresis, and storm attenuation.
 */

#include "unity.h"
#include "mock_i2c_bus.h"
#include "opt3001_driver.h"
#include <string.h>

#define TEST_OPT3001_ADDR   0x44U

void setUp(void) {
    mock_i2c_reset();
}

void tearDown(void) {
    mock_i2c_clear_faults();
}

/**
 * @brief Helper to set up standard valid OPT3001 mock IDs.
 */
static void setup_valid_mock_opt3001(uint8_t addr) {
    mock_i2c_set_auto_crf(true);
    (void)mock_i2c_set_word_register(addr, OPT3001_REG_MANUFACTURER_ID, OPT3001_EXPECTED_MFG_ID);
    (void)mock_i2c_set_word_register(addr, OPT3001_REG_DEVICE_ID, OPT3001_EXPECTED_DEV_ID);
}

/**
 * @brief TC-S4-T2.1-01: Header Inclusion & C99 Compilation.
 */
static void test_opt3001_definitions_and_constants(void) {
    TEST_ASSERT_EQUAL_HEX8(0x44U, OPT3001_I2C_ADDR_DEFAULT);
    TEST_ASSERT_EQUAL_HEX8(0x00U, OPT3001_REG_RESULT);
    TEST_ASSERT_EQUAL_HEX8(0x01U, OPT3001_REG_CONFIG);
    TEST_ASSERT_EQUAL_HEX8(0x02U, OPT3001_REG_LOW_LIMIT);
    TEST_ASSERT_EQUAL_HEX8(0x03U, OPT3001_REG_HIGH_LIMIT);
    TEST_ASSERT_EQUAL_HEX8(0x7EU, OPT3001_REG_MANUFACTURER_ID);
    TEST_ASSERT_EQUAL_HEX8(0x7FU, OPT3001_REG_DEVICE_ID);

    TEST_ASSERT_EQUAL_HEX16(0x5449U, OPT3001_EXPECTED_MFG_ID);
    TEST_ASSERT_EQUAL_HEX16(0x3001U, OPT3001_EXPECTED_DEV_ID);
    TEST_ASSERT_EQUAL_HEX16(0xC210U, OPT3001_CONFIG_SINGLE_SHOT_CMD);
}

/**
 * @brief TC-S4-T2.1-02: NULL Pointer Defensive Guards.
 */
static void test_opt3001_null_pointer_defensive_guards(void) {
    opt3001_dev_t dev;
    opt3001_raw_data_t raw;
    uint16_t mfg_id = 0;
    uint16_t dev_id = 0;
    bool is_ready = false;

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_init(NULL, TEST_OPT3001_ADDR));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_device_id(NULL, &mfg_id, &dev_id));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_device_id(&dev, NULL, &dev_id));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_device_id(&dev, &mfg_id, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_trigger_single_shot(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_is_conversion_ready(NULL, &is_ready));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_is_conversion_ready(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_wait_for_completion(NULL, 100U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_raw_result(NULL, &raw));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_raw_result(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_sample_forced_raw(NULL, &raw));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_sample_forced_raw(&dev, NULL));
}

/**
 * @brief TC-S4-T2.1-03: Valid I2C Initialization.
 */
static void test_opt3001_valid_initialization(void) {
    opt3001_dev_t dev;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);

    status_t status = opt3001_init(&dev, TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(dev.is_initialized);
    TEST_ASSERT_EQUAL_HEX8(TEST_OPT3001_ADDR, dev.i2c_address);
    TEST_ASSERT_EQUAL_HEX16(0x5449U, dev.manufacturer_id);
    TEST_ASSERT_EQUAL_HEX16(0x3001U, dev.device_id);

    /* Invalid I2C Address range checks */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, opt3001_init(&dev, 0x43U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, opt3001_init(&dev, 0x48U));
}

/**
 * @brief TC-S4-T2.1-04: Invalid Manufacturer ID Rejection.
 */
static void test_opt3001_invalid_manufacturer_id_rejection(void) {
    opt3001_dev_t dev;

    /* Bad Mfg ID: 0x0000 */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_MANUFACTURER_ID, 0x0000U);
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_DEVICE_ID, OPT3001_EXPECTED_DEV_ID);
    status_t status = opt3001_init(&dev, TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_HARDWARE, status);
    TEST_ASSERT_FALSE(dev.is_initialized);

    /* Bad Mfg ID: 0xFFFF */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_MANUFACTURER_ID, 0xFFFFU);
    status = opt3001_init(&dev, TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_HARDWARE, status);
    TEST_ASSERT_FALSE(dev.is_initialized);
}

/**
 * @brief TC-S4-T2.1-05: Invalid Device ID Rejection.
 */
static void test_opt3001_invalid_device_id_rejection(void) {
    opt3001_dev_t dev;

    /* Valid Mfg ID but wrong Device ID (e.g. 0x3000) */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_MANUFACTURER_ID, OPT3001_EXPECTED_MFG_ID);
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_DEVICE_ID, 0x3000U);
    status_t status = opt3001_init(&dev, TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_HARDWARE, status);
    TEST_ASSERT_FALSE(dev.is_initialized);
}

/**
 * @brief TC-S4-T2.1-06: Single-Shot Trigger Register Write.
 */
static void test_opt3001_trigger_single_shot(void) {
    opt3001_dev_t dev;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));

    status_t status = opt3001_trigger_single_shot(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    uint16_t written_config = mock_i2c_get_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG);
    TEST_ASSERT_EQUAL_HEX16(OPT3001_CONFIG_SINGLE_SHOT_CMD, written_config & ~OPT3001_CONFIG_CRF_BIT);
    TEST_ASSERT_TRUE((written_config & OPT3001_CONFIG_CRF_BIT) != 0U);
}

/**
 * @brief TC-S4-T2.1-07: Conversion Ready Polling (Active / Not Ready).
 */
static void test_opt3001_is_conversion_ready_active(void) {
    opt3001_dev_t dev;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));

    /* Config register with CRF (bit 7) = 0 */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC210U);

    bool is_ready = true;
    status_t status = opt3001_is_conversion_ready(&dev, &is_ready);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(is_ready);
}

/**
 * @brief TC-S4-T2.1-08: Conversion Ready Polling (Complete / Ready).
 */
static void test_opt3001_is_conversion_ready_complete(void) {
    opt3001_dev_t dev;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));

    /* Config register with CRF (bit 7) = 1 */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC290U);

    bool is_ready = false;
    status_t status = opt3001_is_conversion_ready(&dev, &is_ready);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(is_ready);
}

/**
 * @brief TC-S4-T2.1-09: Conversion Polling Timeout.
 */
static void test_opt3001_wait_for_completion_timeout(void) {
    opt3001_dev_t dev;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));

    mock_i2c_set_auto_crf(false);
    /* CRF continuously 0 */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC210U);

    status_t status = opt3001_wait_for_completion(&dev, 100U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
}

/**
 * @brief TC-S4-T2.1-10: Raw Register Unpacking (Exponent & Mantissa).
 */
static void test_opt3001_read_raw_result_and_sample_forced_raw(void) {
    opt3001_dev_t dev;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));

    /* Test Case vector: 0x52AC -> Exponent = 5 (0x5), Mantissa = 684 (0x2AC) */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_RESULT, 0x52ACU);

    opt3001_raw_data_t raw;
    status_t status = opt3001_read_raw_result(&dev, &raw);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX16(0x52ACU, raw.raw_result);
    TEST_ASSERT_EQUAL_UINT8(5U, raw.exponent);
    TEST_ASSERT_EQUAL_HEX16(0x02ACU, raw.mantissa);

    /* End-to-end forced sampling workflow test */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC280U | OPT3001_CONFIG_CRF_BIT);
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_RESULT, 0x7B12U);

    opt3001_raw_data_t sample;
    status = opt3001_sample_forced_raw(&dev, &sample);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX16(0x7B12U, sample.raw_result);
    TEST_ASSERT_EQUAL_UINT8(7U, sample.exponent);
    TEST_ASSERT_EQUAL_HEX16(0x0B12U, sample.mantissa);
}

/**
 * @brief TC-S4-T2.2-01: Mathematical Constants & Telemetry Definitions.
 */
static void test_opt3001_math_constants(void) {
    TEST_ASSERT_EQUAL_UINT8(11U, OPT3001_MAX_EXPONENT);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 83865.60f, OPT3001_MAX_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, OPT3001_MIN_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, OPT3001_SOLAR_LUMINOUS_EFFICACY);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, OPT3001_TELEMETRY_LUX_SCALE);
    TEST_ASSERT_EQUAL_UINT16(41500U, OPT3001_TELEMETRY_MAX_RAW);
}

/**
 * @brief TC-S4-T2.2-02 & TC-S4-T2.2-03 & TC-S4-T2.2-04 & TC-S4-T2.2-05 & TC-S4-T2.2-06:
 *        Exponential Lux & Centi-Lux Calculations across Dynamic Operating Span.
 */
static void test_opt3001_exponential_lux_and_centi_lux_math(void) {
    /* 1. Min reading: E=0, R=1 (0x0001) -> 0.01 Lux, 1 centi-lux */
    float lux = opt3001_raw_to_lux(0x0001U);
    uint32_t centi = opt3001_raw_to_centi_lux(0x0001U);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, lux);
    TEST_ASSERT_EQUAL_UINT32(1U, centi);

    /* 2. Low-light / overcast: E=3, R=410 (0x319A) -> 32.80 Lux, 3280 centi-lux */
    lux = opt3001_raw_to_lux(0x319AU);
    centi = opt3001_raw_to_centi_lux(0x319AU);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.80f, lux);
    TEST_ASSERT_EQUAL_UINT32(3280U, centi);

    /* 3. Mid-range sun: E=7, R=2000 (0x77D0) -> 2560.00 Lux, 256000 centi-lux */
    lux = opt3001_raw_to_lux(0x77D0U);
    centi = opt3001_raw_to_centi_lux(0x77D0U);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2560.00f, lux);
    TEST_ASSERT_EQUAL_UINT32(256000U, centi);

    /* 4. Direct tropical sun: E=11, R=2612 (0xBA34) -> 53493.76 Lux, 5349376 centi-lux */
    lux = opt3001_raw_to_lux(0xBA34U);
    centi = opt3001_raw_to_centi_lux(0xBA34U);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 53493.76f, lux);
    TEST_ASSERT_EQUAL_UINT32(5349376U, centi);

    /* 5. Max full scale: E=11, R=4095 (0xBFFF) -> 83865.60 Lux, 8386560 centi-lux */
    lux = opt3001_raw_to_lux(0xBFFFU);
    centi = opt3001_raw_to_centi_lux(0xBFFFU);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 83865.60f, lux);
    TEST_ASSERT_EQUAL_UINT32(8386560U, centi);
}

/**
 * @brief TC-S4-T2.2-07: Invalid Exponent Clamping (E >= 12 clamped to 11).
 */
static void test_opt3001_invalid_exponent_clamping(void) {
    /* Exponent 14 (0xE), Mantissa 256 (0x100) -> clamped to E=11, R=256 -> 5242.88 Lux */
    float lux = opt3001_raw_to_lux(0xE100U);
    uint32_t centi = opt3001_raw_to_centi_lux(0xE100U);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5242.88f, lux);
    TEST_ASSERT_EQUAL_UINT32(256U << 11, centi);
}

/**
 * @brief TC-S4-T2.2-08: Solar Irradiance Estimation (W/m² @ 120 lm/W).
 */
static void test_opt3001_solar_irradiance_conversion(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, opt3001_lux_to_irradiance(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, opt3001_lux_to_irradiance(-10.0f));

    /* 60,000 Lux / 120.0 lm/W = 500.00 W/m² */
    float irr = opt3001_lux_to_irradiance(60000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.00f, irr);

    /* 32.80 Lux / 120.0 lm/W = 0.2733 W/m² */
    irr = opt3001_lux_to_irradiance(32.80f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2733f, irr);
}

/**
 * @brief TC-S4-T2.2-09 & TC-S4-T2.2-10: LoRaWAN Telemetry Byte 6-7 Scaling & Clamping.
 */
static void test_opt3001_telemetry_scaling_and_clamping(void) {
    /* Zero / negative */
    TEST_ASSERT_EQUAL_UINT16(0U, opt3001_lux_to_telemetry_u16(0.0f));
    TEST_ASSERT_EQUAL_UINT16(0U, opt3001_lux_to_telemetry_u16(-5.0f));

    /* 32.80 Lux -> (32.80 + 1.0) / 2.0 = 16 */
    TEST_ASSERT_EQUAL_UINT16(16U, opt3001_lux_to_telemetry_u16(32.80f));

    /* 53493.76 Lux -> (53493.76 + 1.0) / 2.0 = 26747 */
    TEST_ASSERT_EQUAL_UINT16(26747U, opt3001_lux_to_telemetry_u16(53493.76f));

    /* Max clamping at 41500 (e.g. 90,000.0 Lux) */
    TEST_ASSERT_EQUAL_UINT16(41500U, opt3001_lux_to_telemetry_u16(90000.0f));
}

/**
 * @brief Master Structure Conversion & NULL Pointer Safety (`opt3001_convert_raw`).
 */
static void test_opt3001_convert_raw(void) {
    opt3001_reading_t reading;
    opt3001_raw_data_t raw;

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_convert_raw(NULL, &reading));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_convert_raw(&raw, NULL));

    /* Valid raw data: 0x319A (E=3, R=410) */
    raw.raw_result = 0x319AU;
    raw.exponent = 3U;
    raw.mantissa = 410U;

    status_t status = opt3001_convert_raw(&raw, &reading);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(reading.is_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.80f, reading.lux);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2733f, reading.irradiance_w_m2);
    TEST_ASSERT_EQUAL_UINT32(3280U, reading.centi_lux);
    TEST_ASSERT_EQUAL_UINT16(16U, reading.telemetry_raw);

    /* Invalid exponent in raw: E=13 */
    raw.raw_result = 0xD19AU;
    raw.exponent = 13U;
    status = opt3001_convert_raw(&raw, &reading);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(reading.is_valid);
}

/**
 * @brief Master High-Level Reading API (`opt3001_read_lux`).
 */
static void test_opt3001_read_lux_high_level_api(void) {
    opt3001_dev_t dev;
    opt3001_reading_t reading;
    setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_lux(NULL, &reading));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_read_lux(&dev, NULL));

    /* Simulate reading 0xBA34 (53493.76 Lux) */
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC280U | OPT3001_CONFIG_CRF_BIT);
    (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_RESULT, 0xBA34U);

    status_t status = opt3001_read_lux(&dev, &reading);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(reading.is_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 53493.76f, reading.lux);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 445.78f, reading.irradiance_w_m2);
    TEST_ASSERT_EQUAL_UINT32(5349376U, reading.centi_lux);
    TEST_ASSERT_EQUAL_UINT16(26747U, reading.telemetry_raw);
}

/**
 * @brief TC-S4-T2.3-01: Day/Night & Cloud Attenuation Threshold Constants.
 */
static void test_opt3001_day_night_threshold_constants(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, OPT3001_NIGHT_THRESHOLD_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.0f, OPT3001_DAWN_THRESHOLD_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, OPT3001_DAYLIGHT_CONFIRM_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, OPT3001_DUSK_THRESHOLD_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5000.0f, OPT3001_ATTENUATION_MIN_HIST_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, OPT3001_ATTENUATION_SEVERE_MAX_LUX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.70f, OPT3001_DROP_RATIO_SEVERE);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.50f, OPT3001_DROP_RATIO_MODERATE);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.30f, OPT3001_DROP_RATIO_MINOR);
}

/**
 * @brief TC-S4-T2.3-02 & TC-S4-T2.3-03 & TC-S4-T2.3-04 & TC-S4-T2.3-05:
 *        Day/Night State Classification with Hysteresis & Daylight Confirmation.
 */
static void test_opt3001_classify_day_state_and_hysteresis(void) {
    /* 1. From DAYLIGHT, drop < 10 Lux (e.g. 8.0 Lux) -> enters NIGHT */
    opt3001_day_state_t state = opt3001_classify_day_state(8.0f, OPT3001_STATE_DAYLIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_NIGHT, state);

    /* 2. From NIGHT, rise to 12.0 Lux (below 15.0 Lux dawn threshold) -> stays in NIGHT */
    state = opt3001_classify_day_state(12.0f, OPT3001_STATE_NIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_NIGHT, state);

    /* 3. From NIGHT, rise to 18.0 Lux (>= 15.0 Lux dawn threshold) -> enters TWILIGHT */
    state = opt3001_classify_day_state(18.0f, OPT3001_STATE_NIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_TWILIGHT, state);

    /* 4. From TWILIGHT, rise to 55.0 Lux (>= 50.0 Lux daylight confirmation) -> enters DAYLIGHT */
    state = opt3001_classify_day_state(55.0f, OPT3001_STATE_TWILIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_DAYLIGHT, state);

    /* 5. From DAYLIGHT, drop to 45.0 Lux (>= 40.0 Lux dusk threshold) -> stays in DAYLIGHT */
    state = opt3001_classify_day_state(45.0f, OPT3001_STATE_DAYLIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_DAYLIGHT, state);

    /* 6. From DAYLIGHT, drop to 35.0 Lux (< 40.0 Lux dusk threshold) -> enters TWILIGHT */
    state = opt3001_classify_day_state(35.0f, OPT3001_STATE_DAYLIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_TWILIGHT, state);

    /* 7. From TWILIGHT, drop to 8.0 Lux (< 10.0 Lux night threshold) -> enters NIGHT */
    state = opt3001_classify_day_state(8.0f, OPT3001_STATE_TWILIGHT);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_NIGHT, state);
}

/**
 * @brief Boolean Daylight Query Function (`opt3001_is_daylight`) with Deadband.
 */
static void test_opt3001_is_daylight_query(void) {
    /* Initially not daylight (night/dawn): needs >= 50 Lux */
    TEST_ASSERT_FALSE(opt3001_is_daylight(45.0f, false));
    TEST_ASSERT_TRUE(opt3001_is_daylight(50.0f, false));
    TEST_ASSERT_TRUE(opt3001_is_daylight(100.0f, false));

    /* Already in daylight: needs < 40 Lux to exit */
    TEST_ASSERT_TRUE(opt3001_is_daylight(45.0f, true));
    TEST_ASSERT_TRUE(opt3001_is_daylight(40.0f, true));
    TEST_ASSERT_FALSE(opt3001_is_daylight(39.9f, true));
    TEST_ASSERT_FALSE(opt3001_is_daylight(5.0f, true));
}

/**
 * @brief TC-S4-T2.3-06: Nighttime / Low Historical Lux Attenuation Suppression.
 */
static void test_opt3001_nighttime_attenuation_suppression(void) {
    float drop_ratio = -1.0f;
    bool solar_alarm = true;

    /* 1. is_daylight == false: scoring suppressed */
    uint8_t score = opt3001_evaluate_solar_attenuation(100.0f, 4000.0f, false, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(0U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, drop_ratio);
    TEST_ASSERT_FALSE(solar_alarm);

    /* 2. is_daylight == true, but history < 5000 Lux: scoring suppressed (early dawn / late dusk) */
    score = opt3001_evaluate_solar_attenuation(1000.0f, 4500.0f, true, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(0U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, drop_ratio);
    TEST_ASSERT_FALSE(solar_alarm);
}

/**
 * @brief TC-S4-T2.3-07: Severe Storm Attenuation (Score = 100, Alarm = 1).
 */
static void test_opt3001_severe_storm_cloud_attenuation(void) {
    float drop_ratio = 0.0f;
    bool solar_alarm = false;

    /* History = 30,000 Lux, Current = 2,500 Lux -> Drop = (30000 - 2500)/30000 = 0.9167 (91.7%), Current < 3000 Lux */
    uint8_t score = opt3001_evaluate_solar_attenuation(2500.0f, 30000.0f, true, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(100U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9167f, drop_ratio);
    TEST_ASSERT_TRUE(solar_alarm);
}

/**
 * @brief TC-S4-T2.3-08: Moderate Storm Attenuation (Score = 65, Alarm = 1).
 */
static void test_opt3001_moderate_storm_cloud_attenuation(void) {
    float drop_ratio = 0.0f;
    bool solar_alarm = false;

    /* History = 40,000 Lux, Current = 18,000 Lux -> Drop = (40000 - 18000)/40000 = 0.55 (55%) */
    uint8_t score = opt3001_evaluate_solar_attenuation(18000.0f, 40000.0f, true, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(65U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.55f, drop_ratio);
    TEST_ASSERT_TRUE(solar_alarm);
}

/**
 * @brief TC-S4-T2.3-09: Minor Storm Attenuation (Score = 30, Alarm = 0).
 */
static void test_opt3001_minor_storm_cloud_attenuation(void) {
    float drop_ratio = 0.0f;
    bool solar_alarm = true;

    /* History = 30,000 Lux, Current = 19,500 Lux -> Drop = (30000 - 19500)/30000 = 0.35 (35%) */
    uint8_t score = opt3001_evaluate_solar_attenuation(19500.0f, 30000.0f, true, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(30U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f, drop_ratio);
    TEST_ASSERT_FALSE(solar_alarm);
}

/**
 * @brief TC-S4-T2.3-10: Steady / Increasing Sunlight (Score = 0, Alarm = 0).
 */
static void test_opt3001_steady_or_increasing_sunlight(void) {
    float drop_ratio = 1.0f;
    bool solar_alarm = true;

    /* History = 30,000 Lux, Current = 35,000 Lux -> Increasing sunlight -> Drop = 0.0 */
    uint8_t score = opt3001_evaluate_solar_attenuation(35000.0f, 30000.0f, true, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(0U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, drop_ratio);
    TEST_ASSERT_FALSE(solar_alarm);

    /* History = 30,000 Lux, Current = 25,000 Lux -> Drop = 16.7% (< 30%) -> Score = 0 */
    score = opt3001_evaluate_solar_attenuation(25000.0f, 30000.0f, true, &drop_ratio, &solar_alarm);
    TEST_ASSERT_EQUAL_UINT8(0U, score);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1667f, drop_ratio);
    TEST_ASSERT_FALSE(solar_alarm);
}

/**
 * @brief Master Solar Context Evaluator & NULL Pointer Safety (`opt3001_update_solar_context`).
 */
static void test_opt3001_update_solar_context(void) {
    opt3001_solar_context_t ctx = {0};

    /* NULL pointer guard */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, opt3001_update_solar_context(1000.0f, 5000.0f, NULL));

    /* Initialize in DAYLIGHT */
    ctx.day_state = OPT3001_STATE_DAYLIGHT;

    /* Simulate severe squall darkening during bright daylight */
    status_t status = opt3001_update_solar_context(2500.0f, 35000.0f, &ctx);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_DAYLIGHT, ctx.day_state);
    TEST_ASSERT_TRUE(ctx.is_daylight);
    TEST_ASSERT_EQUAL_UINT8(100U, ctx.attenuation_score);
    TEST_ASSERT_EQUAL_INT(OPT3001_ATTENUATION_SEVERE, ctx.attenuation_level);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9286f, ctx.drop_ratio);
    TEST_ASSERT_TRUE(ctx.solar_drop_alarm);

    /* Simulate sunset */
    status = opt3001_update_solar_context(5.0f, 1000.0f, &ctx);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(OPT3001_STATE_NIGHT, ctx.day_state);
    TEST_ASSERT_FALSE(ctx.is_daylight);
    TEST_ASSERT_EQUAL_UINT8(0U, ctx.attenuation_score);
    TEST_ASSERT_EQUAL_INT(OPT3001_ATTENUATION_NONE, ctx.attenuation_level);
    TEST_ASSERT_FALSE(ctx.solar_drop_alarm);
}

int main(void) {
    UNITY_BEGIN();

    /* S4-T2.1 Tests */
    RUN_TEST(test_opt3001_definitions_and_constants);
    RUN_TEST(test_opt3001_null_pointer_defensive_guards);
    RUN_TEST(test_opt3001_valid_initialization);
    RUN_TEST(test_opt3001_invalid_manufacturer_id_rejection);
    RUN_TEST(test_opt3001_invalid_device_id_rejection);
    RUN_TEST(test_opt3001_trigger_single_shot);
    RUN_TEST(test_opt3001_is_conversion_ready_active);
    RUN_TEST(test_opt3001_is_conversion_ready_complete);
    RUN_TEST(test_opt3001_wait_for_completion_timeout);
    RUN_TEST(test_opt3001_read_raw_result_and_sample_forced_raw);

    /* S4-T2.2 Tests */
    RUN_TEST(test_opt3001_math_constants);
    RUN_TEST(test_opt3001_exponential_lux_and_centi_lux_math);
    RUN_TEST(test_opt3001_invalid_exponent_clamping);
    RUN_TEST(test_opt3001_solar_irradiance_conversion);
    RUN_TEST(test_opt3001_telemetry_scaling_and_clamping);
    RUN_TEST(test_opt3001_convert_raw);
    RUN_TEST(test_opt3001_read_lux_high_level_api);

    /* S4-T2.3 Tests */
    RUN_TEST(test_opt3001_day_night_threshold_constants);
    RUN_TEST(test_opt3001_classify_day_state_and_hysteresis);
    RUN_TEST(test_opt3001_is_daylight_query);
    RUN_TEST(test_opt3001_nighttime_attenuation_suppression);
    RUN_TEST(test_opt3001_severe_storm_cloud_attenuation);
    RUN_TEST(test_opt3001_moderate_storm_cloud_attenuation);
    RUN_TEST(test_opt3001_minor_storm_cloud_attenuation);
    RUN_TEST(test_opt3001_steady_or_increasing_sunlight);
    RUN_TEST(test_opt3001_update_solar_context);

    return UNITY_END();
}
