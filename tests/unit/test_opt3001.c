/**
 * @file    test_opt3001.c
 * @brief   ThrowTheSwitch Unity unit test suite for TI OPT3001 Driver (S4-T2.1).
 * @details Validates device ID verification, single-shot triggering, conversion polling,
 *          timeout handling, and raw register exponent/mantissa bitfield unpacking.
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
    TEST_ASSERT_EQUAL_HEX16(0xCA10U, OPT3001_CONFIG_SINGLE_SHOT_CMD);
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
    TEST_ASSERT_EQUAL_HEX16(OPT3001_CONFIG_SINGLE_SHOT_CMD, written_config);
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

int main(void) {
    UNITY_BEGIN();

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

    return UNITY_END();
}
