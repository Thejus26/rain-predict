/**
 * @file    test_bme280.c
 * @brief   Unit test suite for Bosch BME280 Factory Trimming Parameter Readout & Calibration Unpacking.
 * @details Validates Chip ID verification, 2-phase burst NVM readout, little-endian and bit-split unpacking,
 *          signed sign-extension, corrupt NVM validation, I2C bus error handling, and parameter guards.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "i2c_bus.h"
#include "bme280_driver.h"

/* Standard Bosch Datasheet Test Vectors (BST-BME280-DS002-15 Appendix 8.1) */
#define VECTOR_DIG_T1       27504U
#define VECTOR_DIG_T2       26435
#define VECTOR_DIG_T3       (-1000)

#define VECTOR_DIG_P1       36477U
#define VECTOR_DIG_P2       (-10685)
#define VECTOR_DIG_P3       3024
#define VECTOR_DIG_P4       2855
#define VECTOR_DIG_P5       140
#define VECTOR_DIG_P6       (-7)
#define VECTOR_DIG_P7       15500
#define VECTOR_DIG_P8       (-14600)
#define VECTOR_DIG_P9       6000

#define VECTOR_DIG_H1       75U
#define VECTOR_DIG_H2       363
#define VECTOR_DIG_H3       0U
#define VECTOR_DIG_H4       315
#define VECTOR_DIG_H5       50
#define VECTOR_DIG_H6       30

static void setup_mock_bme280_registers(uint8_t addr) {
    /* Set Chip ID register (0xD0) */
    (void)i2c_bus_test_set_slave_reg(addr, BME280_REG_CHIP_ID, BME280_CHIP_ID);

    /* Block 1 (0x88..0xA1) */
    uint8_t b1[26];
    b1[0]  = (uint8_t)(VECTOR_DIG_T1 & 0xFFU);
    b1[1]  = (uint8_t)((VECTOR_DIG_T1 >> 8) & 0xFFU);
    b1[2]  = (uint8_t)((uint16_t)VECTOR_DIG_T2 & 0xFFU);
    b1[3]  = (uint8_t)(((uint16_t)VECTOR_DIG_T2 >> 8) & 0xFFU);
    b1[4]  = (uint8_t)((uint16_t)VECTOR_DIG_T3 & 0xFFU);
    b1[5]  = (uint8_t)(((uint16_t)VECTOR_DIG_T3 >> 8) & 0xFFU);

    b1[6]  = (uint8_t)(VECTOR_DIG_P1 & 0xFFU);
    b1[7]  = (uint8_t)((VECTOR_DIG_P1 >> 8) & 0xFFU);
    b1[8]  = (uint8_t)((uint16_t)VECTOR_DIG_P2 & 0xFFU);
    b1[9]  = (uint8_t)(((uint16_t)VECTOR_DIG_P2 >> 8) & 0xFFU);
    b1[10] = (uint8_t)((uint16_t)VECTOR_DIG_P3 & 0xFFU);
    b1[11] = (uint8_t)(((uint16_t)VECTOR_DIG_P3 >> 8) & 0xFFU);
    b1[12] = (uint8_t)((uint16_t)VECTOR_DIG_P4 & 0xFFU);
    b1[13] = (uint8_t)(((uint16_t)VECTOR_DIG_P4 >> 8) & 0xFFU);
    b1[14] = (uint8_t)((uint16_t)VECTOR_DIG_P5 & 0xFFU);
    b1[15] = (uint8_t)(((uint16_t)VECTOR_DIG_P5 >> 8) & 0xFFU);
    b1[16] = (uint8_t)((uint16_t)VECTOR_DIG_P6 & 0xFFU);
    b1[17] = (uint8_t)(((uint16_t)VECTOR_DIG_P6 >> 8) & 0xFFU);
    b1[18] = (uint8_t)((uint16_t)VECTOR_DIG_P7 & 0xFFU);
    b1[19] = (uint8_t)(((uint16_t)VECTOR_DIG_P7 >> 8) & 0xFFU);
    b1[20] = (uint8_t)((uint16_t)VECTOR_DIG_P8 & 0xFFU);
    b1[21] = (uint8_t)(((uint16_t)VECTOR_DIG_P8 >> 8) & 0xFFU);
    b1[22] = (uint8_t)((uint16_t)VECTOR_DIG_P9 & 0xFFU);
    b1[23] = (uint8_t)(((uint16_t)VECTOR_DIG_P9 >> 8) & 0xFFU);
    b1[24] = 0x00U; /* Reserved 0xA0 */
    b1[25] = VECTOR_DIG_H1; /* 0xA1 */

    (void)i2c_bus_test_set_slave_regs(addr, BME280_REG_CALIB_00_25, b1, sizeof(b1));

    /* Block 2 (0xE1..0xE7) */
    uint8_t b2[7];
    b2[0] = (uint8_t)((uint16_t)VECTOR_DIG_H2 & 0xFFU);
    b2[1] = (uint8_t)(((uint16_t)VECTOR_DIG_H2 >> 8) & 0xFFU);
    b2[2] = VECTOR_DIG_H3;
    /* dig_H4 = 315 (0x13B) -> 0xE4=0x13, 0xE5[3:0]=0x0B */
    b2[3] = (uint8_t)((VECTOR_DIG_H4 >> 4) & 0xFFU); /* 0x13 */
    /* dig_H5 = 50 (0x032) -> 0xE6=0x03, 0xE5[7:4]=0x20 */
    b2[4] = (uint8_t)((VECTOR_DIG_H4 & 0x0FU) | (uint8_t)((VECTOR_DIG_H5 << 4) & 0xF0U)); /* 0x2B */
    b2[5] = (uint8_t)((VECTOR_DIG_H5 >> 4) & 0xFFU); /* 0x03 */
    b2[6] = (uint8_t)VECTOR_DIG_H6; /* 0x1E */

    (void)i2c_bus_test_set_slave_regs(addr, BME280_REG_CALIB_26_41, b2, sizeof(b2));
}

void setUp(void) {
    i2c_bus_test_reset();
    (void)i2c_bus_init(I2C_BUS_SPEED_FAST_HZ);
    setup_mock_bme280_registers(BME280_I2C_ADDR_PRIMARY);
    setup_mock_bme280_registers(BME280_I2C_ADDR_SECONDARY);
}

void tearDown(void) {
    (void)i2c_bus_deinit();
    i2c_bus_test_reset();
}

/**
 * @brief TC-S4-T1.1-01 & 02: Chip ID Verification (0x60).
 */
static void test_bme280_read_chip_id_success(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    uint8_t chip_id = 0;
    status_t status = bme280_read_chip_id(&dev, &chip_id);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(BME280_CHIP_ID, chip_id);
    TEST_ASSERT_EQUAL_UINT8(BME280_CHIP_ID, dev.chip_id);
}

/**
 * @brief TC-S4-T1.1-03: Chip ID Mismatch (BMP280 0x58 and 0xFF).
 */
static void test_bme280_read_chip_id_mismatch(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    /* Simulate BMP280 */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_CHIP_ID, 0x58U);

    uint8_t chip_id = 0;
    status_t status = bme280_read_chip_id(&dev, &chip_id);

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_HARDWARE, status);
    TEST_ASSERT_EQUAL_UINT8(0x58U, chip_id);

    /* Simulate floating bus (0xFF) */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_CHIP_ID, 0xFFU);
    status = bme280_read_chip_id(&dev, &chip_id);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_HARDWARE, status);
}

/**
 * @brief TC-S4-T1.1-04: Burst read transaction structure (2 transactions).
 */
static void test_bme280_read_calibration_burst_structure(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    uint32_t reads_before = i2c_bus_test_get_read_count(BME280_I2C_ADDR_PRIMARY);
    status_t status = bme280_read_calibration(&dev);
    uint32_t reads_after = i2c_bus_test_get_read_count(BME280_I2C_ADDR_PRIMARY);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(2U, reads_after - reads_before);
}

/**
 * @brief TC-S4-T1.1-05: Temperature Trimming Unpacking Accuracy.
 */
static void test_bme280_temperature_calibration_unpacking(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    status_t status = bme280_read_calibration(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    TEST_ASSERT_EQUAL_UINT16(VECTOR_DIG_T1, dev.calib.dig_T1);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_T2, dev.calib.dig_T2);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_T3, dev.calib.dig_T3);
}

/**
 * @brief TC-S4-T1.1-06: Pressure Trimming Unpacking Accuracy.
 */
static void test_bme280_pressure_calibration_unpacking(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    status_t status = bme280_read_calibration(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    TEST_ASSERT_EQUAL_UINT16(VECTOR_DIG_P1, dev.calib.dig_P1);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P2, dev.calib.dig_P2);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P3, dev.calib.dig_P3);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P4, dev.calib.dig_P4);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P5, dev.calib.dig_P5);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P6, dev.calib.dig_P6);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P7, dev.calib.dig_P7);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P8, dev.calib.dig_P8);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_P9, dev.calib.dig_P9);
}

/**
 * @brief TC-S4-T1.1-07 & 08: Humidity Trimming & Bit-Split Unpacking.
 */
static void test_bme280_humidity_calibration_unpacking(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    status_t status = bme280_read_calibration(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    TEST_ASSERT_EQUAL_UINT8(VECTOR_DIG_H1, dev.calib.dig_H1);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_H2, dev.calib.dig_H2);
    TEST_ASSERT_EQUAL_UINT8(VECTOR_DIG_H3, dev.calib.dig_H3);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_H4, dev.calib.dig_H4);
    TEST_ASSERT_EQUAL_INT16(VECTOR_DIG_H5, dev.calib.dig_H5);
    TEST_ASSERT_EQUAL_INT8(VECTOR_DIG_H6, dev.calib.dig_H6);
}

/**
 * @brief Test signed negative sign extension in bit-split dig_H4 and dig_H5.
 */
static void test_bme280_humidity_bit_split_signed_negative(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    /* Set negative MSB for dig_H4: 0xE4 = 0xF0 (-16 as int8), 0xE5 = 0x05 -> (-16<<4)|5 = -251 */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, 0xE4U, 0xF0U);
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, 0xE5U, 0x05U);
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, 0xE6U, 0x00U);

    status_t status = bme280_read_calibration(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT16(-251, dev.calib.dig_H4);

    /* Set negative MSB for dig_H5: 0xE6 = 0x85 (-123 as int8), 0xE5 = 0xC0 -> (-123<<4)|12 = -1956 */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, 0xE4U, 0x10U);
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, 0xE5U, 0xC0U);
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, 0xE6U, 0x85U);

    status = bme280_read_calibration(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT16(-1956, dev.calib.dig_H5);
}

/**
 * @brief TC-S4-T1.1-09: Corrupt NVM Detection (All-Zero / All-0xFF).
 */
static void test_bme280_corrupt_nvm_detection(void) {
    bme280_calib_data_t calib;

    /* 1. All Zeros */
    memset(&calib, 0, sizeof(calib));
    status_t status = bme280_validate_calibration(&calib);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_DATA_CORRUPT, status);

    /* 2. All 0xFF */
    memset(&calib, 0xFF, sizeof(calib));
    status = bme280_validate_calibration(&calib);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_DATA_CORRUPT, status);

    /* 3. Valid Struct */
    memset(&calib, 0, sizeof(calib));
    calib.dig_T1 = VECTOR_DIG_T1;
    calib.dig_P1 = VECTOR_DIG_P1;
    calib.dig_H1 = VECTOR_DIG_H1;
    calib.dig_H3 = VECTOR_DIG_H3;
    status = bme280_validate_calibration(&calib);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
}

/**
 * @brief TC-S4-T1.1-10: I2C Bus Timeout & Fault Propagation.
 */
static void test_bme280_i2c_bus_faults(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    /* Timeout fault */
    i2c_bus_test_inject_fault(STATUS_ERR_TIMEOUT);
    status_t status = bme280_init(&dev, BME280_I2C_ADDR_PRIMARY);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
    TEST_ASSERT_FALSE(dev.is_initialized);

    /* Sensor NACK / No response */
    i2c_bus_test_inject_fault(STATUS_ERR_SENSOR_NO_RESPONSE);
    status = bme280_init(&dev, BME280_I2C_ADDR_PRIMARY);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_SENSOR_NO_RESPONSE, status);
    TEST_ASSERT_FALSE(dev.is_initialized);

    i2c_bus_test_clear_faults();
}

/**
 * @brief Test bme280_init lifecycle, secondary address, and accessors.
 */
static void test_bme280_init_and_getters(void) {
    bme280_dev_t dev;

    /* Primary address init */
    status_t status = bme280_init(&dev, BME280_I2C_ADDR_PRIMARY);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(dev.is_initialized);
    TEST_ASSERT_EQUAL_UINT8(BME280_I2C_ADDR_PRIMARY, dev.i2c_address);
    TEST_ASSERT_EQUAL_UINT8(BME280_CHIP_ID, dev.chip_id);

    const bme280_calib_data_t *p_calib = bme280_get_calibration(&dev);
    TEST_ASSERT_NOT_NULL(p_calib);
    TEST_ASSERT_EQUAL_UINT16(VECTOR_DIG_T1, p_calib->dig_T1);

    /* Secondary address init */
    memset(&dev, 0, sizeof(dev));
    status = bme280_init(&dev, BME280_I2C_ADDR_SECONDARY);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(dev.is_initialized);
    TEST_ASSERT_EQUAL_UINT8(BME280_I2C_ADDR_SECONDARY, dev.i2c_address);

    /* Uninitialized handle getter */
    bme280_dev_t uninit_dev;
    memset(&uninit_dev, 0, sizeof(uninit_dev));
    TEST_ASSERT_NULL(bme280_get_calibration(&uninit_dev));
    TEST_ASSERT_NULL(bme280_get_calibration(NULL));
}

/**
 * @brief Defensive NULL pointer and invalid parameter checking.
 */
static void test_bme280_null_and_invalid_params(void) {
    bme280_dev_t dev;
    uint8_t chip_id = 0;

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_init(NULL, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, bme280_init(&dev, 0x12U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_chip_id(NULL, &chip_id));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_chip_id(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_calibration(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_validate_calibration(NULL));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bme280_read_chip_id_success);
    RUN_TEST(test_bme280_read_chip_id_mismatch);
    RUN_TEST(test_bme280_read_calibration_burst_structure);
    RUN_TEST(test_bme280_temperature_calibration_unpacking);
    RUN_TEST(test_bme280_pressure_calibration_unpacking);
    RUN_TEST(test_bme280_humidity_calibration_unpacking);
    RUN_TEST(test_bme280_humidity_bit_split_signed_negative);
    RUN_TEST(test_bme280_corrupt_nvm_detection);
    RUN_TEST(test_bme280_i2c_bus_faults);
    RUN_TEST(test_bme280_init_and_getters);
    RUN_TEST(test_bme280_null_and_invalid_params);

    return UNITY_END();
}
