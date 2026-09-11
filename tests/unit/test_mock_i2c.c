/**
 * @file    test_mock_i2c.c
 * @brief   Unit test verification suite for Mock I2C Bus and Fault Injection Engine.
 */

#include "unity.h"
#include "mock_i2c_bus.h"
#include <string.h>

#define BME280_I2C_ADDR         0x76U
#define OPT3001_I2C_ADDR        0x44U
#define BME280_REG_CHIP_ID      0xD0U
#define BME280_CHIP_ID_VALUE    0x60U

void setUp(void) {
    mock_i2c_reset();
}

void tearDown(void) {
    mock_i2c_clear_faults();
}

void test_mock_i2c_single_register_readback(void) {
    status_t status = mock_i2c_set_register(BME280_I2C_ADDR, BME280_REG_CHIP_ID, BME280_CHIP_ID_VALUE);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    uint8_t read_val = 0x00U;
    status = i2c_bus_read(BME280_I2C_ADDR, BME280_REG_CHIP_ID, &read_val, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX8(BME280_CHIP_ID_VALUE, read_val);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_i2c_get_read_count(BME280_I2C_ADDR));
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CHIP_ID, mock_i2c_get_last_reg(BME280_I2C_ADDR));
}

void test_mock_i2c_burst_readback(void) {
    const uint8_t calib_data[24] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18
    };

    status_t status = mock_i2c_set_registers(BME280_I2C_ADDR, 0x88U, calib_data, sizeof(calib_data));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    uint8_t rx_buf[24] = {0};
    status = i2c_bus_read(BME280_I2C_ADDR, 0x88U, rx_buf, sizeof(rx_buf), 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(calib_data, rx_buf, sizeof(calib_data));
}

void test_mock_i2c_driver_write_interception(void) {
    const uint8_t config_bytes[2] = {0xCE, 0x10};

    status_t status = i2c_bus_write(OPT3001_I2C_ADDR, 0x01U, config_bytes, sizeof(config_bytes), 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_i2c_get_write_count(OPT3001_I2C_ADDR));
    TEST_ASSERT_EQUAL_HEX8(0x01U, mock_i2c_get_last_reg(OPT3001_I2C_ADDR));

    TEST_ASSERT_EQUAL_HEX8(0xCEU, mock_i2c_get_register(OPT3001_I2C_ADDR, 0x01U));
    TEST_ASSERT_EQUAL_HEX8(0x10U, mock_i2c_get_register(OPT3001_I2C_ADDR, 0x02U));
}

void test_mock_i2c_address_nack_fault(void) {
    mock_i2c_inject_fault(MOCK_I2C_FAULT_NACK_ADDR, 0);

    uint8_t dummy = 0;
    status_t status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, &dummy, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_SENSOR_NO_RESPONSE, status);

    status = i2c_bus_write(BME280_I2C_ADDR, 0xF4U, &dummy, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_SENSOR_NO_RESPONSE, status);

    status = i2c_bus_is_device_ready(BME280_I2C_ADDR, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_SENSOR_NO_RESPONSE, status);
}

void test_mock_i2c_timeout_fault(void) {
    mock_i2c_inject_fault(MOCK_I2C_FAULT_TIMEOUT, 0);

    uint8_t dummy = 0;
    status_t status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, &dummy, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
}

void test_mock_i2c_data_corruption_fault(void) {
    (void)mock_i2c_set_register(BME280_I2C_ADDR, 0xD0U, 0xAAU);
    mock_i2c_inject_fault(MOCK_I2C_FAULT_CORRUPT_DATA, 0);

    uint8_t rx_byte = 0;
    status_t status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, &rx_byte, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX8(0x55U, rx_byte); /* 0xAA ^ 0xFF = 0x55 */
}

void test_mock_i2c_delayed_fault_trigger(void) {
    (void)mock_i2c_set_register(BME280_I2C_ADDR, 0xD0U, 0x60U);
    /* Fail on 3rd transaction */
    mock_i2c_inject_fault(MOCK_I2C_FAULT_TIMEOUT, 3);

    uint8_t rx = 0;
    /* Transaction 1: Success */
    status_t status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, &rx, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Transaction 2: Success */
    status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, &rx, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Transaction 3: Fails with TIMEOUT */
    status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, &rx, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
}

void test_mock_i2c_device_ready_check(void) {
    /* Initially unconfigured */
    status_t status = i2c_bus_is_device_ready(0x22U, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_SENSOR_NO_RESPONSE, status);

    /* Configure device */
    (void)mock_i2c_set_register(0x22U, 0x00U, 0x12U);
    status = i2c_bus_is_device_ready(0x22U, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
}

void test_mock_i2c_null_ptr_guard(void) {
    status_t status = i2c_bus_read(BME280_I2C_ADDR, 0xD0U, NULL, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);

    status = i2c_bus_write(BME280_I2C_ADDR, 0xD0U, NULL, 1, 100);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, status);
}

void test_mock_i2c_reset_isolation(void) {
    (void)mock_i2c_set_register(BME280_I2C_ADDR, 0xD0U, 0x60U);
    mock_i2c_reset();

    uint8_t val = mock_i2c_get_register(BME280_I2C_ADDR, 0xD0U);
    TEST_ASSERT_EQUAL_HEX8(0x00U, val);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_i2c_get_read_count(BME280_I2C_ADDR));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_i2c_get_write_count(BME280_I2C_ADDR));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mock_i2c_single_register_readback);
    RUN_TEST(test_mock_i2c_burst_readback);
    RUN_TEST(test_mock_i2c_driver_write_interception);
    RUN_TEST(test_mock_i2c_address_nack_fault);
    RUN_TEST(test_mock_i2c_timeout_fault);
    RUN_TEST(test_mock_i2c_data_corruption_fault);
    RUN_TEST(test_mock_i2c_delayed_fault_trigger);
    RUN_TEST(test_mock_i2c_device_ready_check);
    RUN_TEST(test_mock_i2c_null_ptr_guard);
    RUN_TEST(test_mock_i2c_reset_isolation);
    return UNITY_END();
}
