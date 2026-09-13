/**
 * @file    test_i2c_bus.c
 * @brief   Unit test suite for Bounded Non-Blocking I2C Driver with 9-Clock Lockup Recovery.
 * @details Validates initialization, standard/fast speed modes, multi-byte burst transfers,
 *          16-bit big-endian conversions, bounded timeouts, 9-clock recovery, and NULL pointer safety.
 */

#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "i2c_bus.h"

#define TEST_BME280_ADDR        0x76U
#define TEST_OPT3001_ADDR       0x44U
#define TEST_REG_CALIB_START    0x88U
#define TEST_REG_CONFIG         0x01U

void setUp(void) {
    i2c_bus_test_reset();
    (void)i2c_bus_init(I2C_BUS_SPEED_FAST_HZ);
}

void tearDown(void) {
    (void)i2c_bus_deinit();
    i2c_bus_test_reset();
}

/**
 * @brief TC-S3-T2.1-01: Verify bus initialization with Standard (100kHz) and Fast (400kHz) modes.
 */
static void test_i2c_bus_init_speeds(void) {
    /* 1. Fast Mode 400 kHz */
    status_t status = i2c_bus_init(I2C_BUS_SPEED_FAST_HZ);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(i2c_bus_test_is_initialized());
    TEST_ASSERT_EQUAL_UINT32(400000UL, i2c_bus_test_get_speed_hz());

    /* 2. Standard Mode 100 kHz */
    status = i2c_bus_init(I2C_BUS_SPEED_STANDARD_HZ);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(i2c_bus_test_is_initialized());
    TEST_ASSERT_EQUAL_UINT32(100000UL, i2c_bus_test_get_speed_hz());

    /* 3. Invalid speed parameter */
    status = i2c_bus_init(250000UL);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);
}

/**
 * @brief TC-S3-T2.1-02: Verify multi-byte contiguous burst readback (e.g. BME280 24-byte calib table).
 */
static void test_i2c_bus_burst_readback(void) {
    const uint8_t mock_calib[24] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
    };

    for (uint8_t i = 0; i < 24; i++) {
        i2c_bus_test_set_slave_reg(TEST_BME280_ADDR, (uint8_t)(TEST_REG_CALIB_START + i), mock_calib[i]);
    }

    uint8_t rx_buffer[24] = {0};
    status_t status = i2c_bus_read(TEST_BME280_ADDR,
                                   TEST_REG_CALIB_START,
                                   rx_buffer,
                                   sizeof(rx_buffer),
                                   I2C_BUS_DEFAULT_TIMEOUT_MS);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(mock_calib, rx_buffer, sizeof(mock_calib));
}

/**
 * @brief TC-S3-T2.1-03: Verify multi-byte write sequence to sensor registers.
 */
static void test_i2c_bus_write_sequence(void) {
    const uint8_t write_payload[4] = {0x54, 0xA2, 0x33, 0x91};

    status_t status = i2c_bus_write(TEST_BME280_ADDR,
                                    0xF2U,
                                    write_payload,
                                    sizeof(write_payload),
                                    I2C_BUS_DEFAULT_TIMEOUT_MS);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    for (uint8_t i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(write_payload[i], i2c_bus_test_get_slave_reg(TEST_BME280_ADDR, (uint8_t)(0xF2U + i)));
    }
}

/**
 * @brief TC-S3-T2.1-04: Verify 16-bit Big-Endian word serialization (OPT3001 sensor format).
 */
static void test_i2c_bus_16bit_word_endianness(void) {
    /* Write 0x1234 -> should store MSB (0x12) at reg 0x01, LSB (0x34) at reg 0x02 */
    const uint16_t test_word = 0x1234U;
    status_t status = i2c_bus_write16(TEST_OPT3001_ADDR,
                                      TEST_REG_CONFIG,
                                      test_word,
                                      I2C_BUS_DEFAULT_TIMEOUT_MS);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX8(0x12U, i2c_bus_test_get_slave_reg(TEST_OPT3001_ADDR, TEST_REG_CONFIG));
    TEST_ASSERT_EQUAL_HEX8(0x34U, i2c_bus_test_get_slave_reg(TEST_OPT3001_ADDR, (uint8_t)(TEST_REG_CONFIG + 1U)));

    /* Read back 16-bit word */
    uint16_t read_word = 0x0000U;
    status = i2c_bus_read16(TEST_OPT3001_ADDR,
                            TEST_REG_CONFIG,
                            &read_word,
                            I2C_BUS_DEFAULT_TIMEOUT_MS);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX16(test_word, read_word);
}

/**
 * @brief TC-S3-T2.1-05: Verify device ready probing and NACK handling.
 */
static void test_i2c_bus_probe_and_nack(void) {
    /* 1. Probing unconfigured device should return STATUS_ERR_SENSOR_NO_RESPONSE */
    status_t status = i2c_bus_is_device_ready(0x22U, 3U, I2C_BUS_PROBE_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_SENSOR_NO_RESPONSE, status);

    /* 2. Configure device -> should return STATUS_OK */
    i2c_bus_test_set_slave_reg(0x22U, 0x00U, 0x55U);
    status = i2c_bus_is_device_ready(0x22U, 3U, I2C_BUS_PROBE_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
}

/**
 * @brief TC-S3-T2.1-06: Verify 9-clock SCL lockup recovery releases stuck SDA line.
 */
static void test_i2c_bus_9clock_lockup_recovery(void) {
    /* Simulate slave holding SDA stuck LOW */
    i2c_bus_test_set_sda_stuck(true);
    TEST_ASSERT_TRUE(i2c_bus_test_get_sda_stuck());

    /* Execute bus recovery routine */
    status_t status = i2c_bus_recover();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify SCL was clocked and STOP condition was emitted */
    TEST_ASSERT_GREATER_THAN_UINT32(0U, i2c_bus_test_get_recovery_pulse_count());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(9U, i2c_bus_test_get_recovery_pulse_count());
    TEST_ASSERT_TRUE(i2c_bus_test_get_stop_condition_emitted());
    TEST_ASSERT_FALSE(i2c_bus_test_get_sda_stuck());
    TEST_ASSERT_TRUE(i2c_bus_test_is_initialized());
}

/**
 * @brief TC-S3-T2.1-07: Verify defensive NULL pointer and parameter validation guards.
 */
static void test_i2c_bus_defensive_guards(void) {
    uint8_t buffer[4] = {0};

    /* NULL pointer destination buffer */
    status_t status = i2c_bus_read(TEST_BME280_ADDR, 0x00U, NULL, 4U, I2C_BUS_DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);

    /* Zero length read */
    status = i2c_bus_read(TEST_BME280_ADDR, 0x00U, buffer, 0U, I2C_BUS_DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);

    /* NULL pointer source buffer */
    status = i2c_bus_write(TEST_BME280_ADDR, 0x00U, NULL, 4U, I2C_BUS_DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);

    /* Zero length write */
    status = i2c_bus_write(TEST_BME280_ADDR, 0x00U, buffer, 0U, I2C_BUS_DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);

    /* NULL pointer 16-bit read */
    status = i2c_bus_read16(TEST_OPT3001_ADDR, 0x00U, NULL, I2C_BUS_DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);
}

/**
 * @brief TC-S3-T2.1-08: Verify pre-sleep deinitialization and bus busy check.
 */
static void test_i2c_bus_deinit_and_busy(void) {
    TEST_ASSERT_TRUE(i2c_bus_test_is_initialized());
    TEST_ASSERT_FALSE(i2c_bus_is_busy());

    status_t status = i2c_bus_deinit();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(i2c_bus_test_is_initialized());

    /* Operating on de-initialized bus should return error */
    uint8_t val = 0;
    status = i2c_bus_read(TEST_BME280_ADDR, 0x00U, &val, 1U, I2C_BUS_DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_i2c_bus_init_speeds);
    RUN_TEST(test_i2c_bus_burst_readback);
    RUN_TEST(test_i2c_bus_write_sequence);
    RUN_TEST(test_i2c_bus_16bit_word_endianness);
    RUN_TEST(test_i2c_bus_probe_and_nack);
    RUN_TEST(test_i2c_bus_9clock_lockup_recovery);
    RUN_TEST(test_i2c_bus_defensive_guards);
    RUN_TEST(test_i2c_bus_deinit_and_busy);
    return UNITY_END();
}
