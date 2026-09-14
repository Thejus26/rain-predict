/**
 * @file    test_bme280.c
 * @brief   Unit test suite for Bosch BME280 Driver (Calibration, Forced Mode & FPU Compensation).
 * @details Validates Chip ID verification, 2-phase burst NVM readout, little-endian and bit-split unpacking,
 *          signed sign-extension, corrupt NVM validation, forced-mode triggering, bounded conversion polling,
 *          atomic 8-byte raw ADC readout, single-precision FPU mathematical compensation (T, P, RH),
 *          zero-division guards, physical boundary clamping, and fixed-point integer scaling.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "i2c_bus.h"
#include "bme280_driver.h"

/** @brief Local helper macros for integer tolerance assertions within +/- delta */
#define TEST_ASSERT_INT16_WITHIN(delta, expected, actual) \
    TEST_ASSERT_TRUE(((int32_t)(actual) >= ((int32_t)(expected) - (int32_t)(delta))) && \
                     ((int32_t)(actual) <= ((int32_t)(expected) + (int32_t)(delta))))

#define TEST_ASSERT_UINT32_WITHIN(delta, expected, actual) \
    TEST_ASSERT_TRUE(((uint32_t)(actual) >= ((uint32_t)(expected) - (uint32_t)(delta))) && \
                     ((uint32_t)(actual) <= ((uint32_t)(expected) + (uint32_t)(delta))))

#define TEST_ASSERT_UINT16_WITHIN(delta, expected, actual) \
    TEST_ASSERT_TRUE(((uint32_t)(actual) >= ((uint32_t)(expected) - (uint32_t)(delta))) && \
                     ((uint32_t)(actual) <= ((uint32_t)(expected) + (uint32_t)(delta))))

/* Standard Bosch Datasheet Trimming Test Vectors (BST-BME280-DS002-15 Appendix 8.1) */
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

/* Raw ADC Test Vectors (BST-BME280-DS002-15 Appendix 8.1) */
#define VECTOR_BOSCH_RAW_ADC_T  519888
#define VECTOR_BOSCH_RAW_ADC_P  415148
#define VECTOR_BOSCH_RAW_ADC_H  25600

#define EXPECTED_BOSCH_COMP_T   25.08f   /* °C */
#define EXPECTED_BOSCH_COMP_P   1006.53f /* hPa */
#define EXPECTED_BOSCH_COMP_H   54.32f   /* %RH */

/* Raw ADC Test Vectors for Table 18 */
#define VECTOR_RAW_PRESS_MSB    0x5DU
#define VECTOR_RAW_PRESS_LSB    0x8AU
#define VECTOR_RAW_PRESS_XLSB   0xC0U
#define EXPECTED_RAW_ADC_P      383148

#define VECTOR_RAW_TEMP_MSB     0x7FU
#define VECTOR_RAW_TEMP_LSB     0x6EU
#define VECTOR_RAW_TEMP_XLSB    0x80U
#define EXPECTED_RAW_ADC_T      521960

#define VECTOR_RAW_HUM_MSB      0x6DU
#define VECTOR_RAW_HUM_LSB      0x3BU
#define EXPECTED_RAW_ADC_H      27963

static void setup_mock_bme280_registers(uint8_t addr) {
    /* Set Chip ID register (0xD0) */
    (void)i2c_bus_test_set_slave_reg(addr, BME280_REG_CHIP_ID, BME280_CHIP_ID);

    /* Set Status register (0xF3) default ready (measuring = 0) */
    (void)i2c_bus_test_set_slave_reg(addr, BME280_REG_STATUS, 0x00U);

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

    /* Data registers (0xF7..0xFE) with Bosch test vector */
    /* adc_P: 415148 = 0x655AC -> F7=0x65, F8=0x5A, F9=0xC0 */
    /* adc_T: 519888 = 0x7EE90 -> FA=0x7E, FB=0xE9, FC=0x00 */
    /* adc_H: 25600  = 0x6400  -> FD=0x64, FE=0x00 */
    uint8_t raw_data[8] = {
        0x65U, 0x5AU, 0xC0U,
        0x7EU, 0xE9U, 0x00U,
        0x64U, 0x00U
    };
    (void)i2c_bus_test_set_slave_regs(addr, BME280_REG_PRESS_MSB, raw_data, sizeof(raw_data));
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
 * @brief TC-S4-T1.2-02: Configuration Register Sequence (ctrl_hum 0xF2, config 0xF5).
 */
static void test_bme280_configure_sequence(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    bme280_config_t config = {
        .osrs_t = BME280_OVERSAMPLING_2X,
        .osrs_p = BME280_OVERSAMPLING_16X,
        .osrs_h = BME280_OVERSAMPLING_1X,
        .filter = BME280_FILTER_COEFF_4
    };

    status_t status = bme280_configure(&dev, &config);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Check ctrl_hum (0xF2): osrs_h = 1 */
    uint8_t ctrl_hum = i2c_bus_test_get_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_CTRL_HUM);
    TEST_ASSERT_EQUAL_UINT8(0x01U, ctrl_hum);

    /* Check config (0xF5): filter = 2 -> (2 << 2) = 0x08 */
    uint8_t config_reg = i2c_bus_test_get_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_CONFIG);
    TEST_ASSERT_EQUAL_UINT8(0x08U, config_reg);
}

/**
 * @brief TC-S4-T1.2-03: Forced-Mode Trigger Byte (ctrl_meas 0xF4).
 */
static void test_bme280_trigger_forced_mode(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    dev.config.osrs_t = BME280_OVERSAMPLING_2X;  /* 2 -> (2 << 5) = 0x40 */
    dev.config.osrs_p = BME280_OVERSAMPLING_16X; /* 5 -> (5 << 2) = 0x14 */
    dev.config.osrs_h = BME280_OVERSAMPLING_1X;
    dev.config.filter = BME280_FILTER_COEFF_4;

    status_t status = bme280_trigger_forced_mode(&dev);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* ctrl_meas = 0x40 | 0x14 | 0x01 = 0x55 */
    uint8_t ctrl_meas = i2c_bus_test_get_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_CTRL_MEAS);
    TEST_ASSERT_EQUAL_UINT8(0x55U, ctrl_meas);
}

/**
 * @brief TC-S4-T1.2-04 & 05: Conversion Status Polling (is_measuring).
 */
static void test_bme280_is_measuring(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    bool is_measuring = false;

    /* 1. Active conversion (bit 3 = 1) */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_STATUS, BME280_REG_STATUS_MEASURING_BIT);
    status_t status = bme280_is_measuring(&dev, &is_measuring);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(is_measuring);

    /* 2. Completed conversion (bit 3 = 0) */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_STATUS, 0x00U);
    status = bme280_is_measuring(&dev, &is_measuring);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(is_measuring);
}

/**
 * @brief TC-S4-T1.2-10: Bounded Status Polling Timeout (wait_for_completion).
 */
static void test_bme280_wait_for_completion(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    /* 1. Complete immediately */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_STATUS, 0x00U);
    status_t status = bme280_wait_for_completion(&dev, 60U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* 2. Timeout when measuring bit held active */
    (void)i2c_bus_test_set_slave_reg(BME280_I2C_ADDR_PRIMARY, BME280_REG_STATUS, BME280_REG_STATUS_MEASURING_BIT);
    status = bme280_wait_for_completion(&dev, 10U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
}

/**
 * @brief TC-S4-T1.2-06..09: 8-Byte Burst Read & 20-bit/16-bit Raw ADC Unpacking.
 */
static void test_bme280_read_raw_data_and_burst(void) {
    bme280_dev_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.i2c_address = BME280_I2C_ADDR_PRIMARY;

    uint8_t raw_data[8] = {
        VECTOR_RAW_PRESS_MSB,  VECTOR_RAW_PRESS_LSB,  VECTOR_RAW_PRESS_XLSB,
        VECTOR_RAW_TEMP_MSB,   VECTOR_RAW_TEMP_LSB,   VECTOR_RAW_TEMP_XLSB,
        VECTOR_RAW_HUM_MSB,    VECTOR_RAW_HUM_LSB
    };
    (void)i2c_bus_test_set_slave_regs(BME280_I2C_ADDR_PRIMARY, BME280_REG_PRESS_MSB, raw_data, sizeof(raw_data));

    uint32_t reads_before = i2c_bus_test_get_read_count(BME280_I2C_ADDR_PRIMARY);

    bme280_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));

    status_t status = bme280_read_raw_data(&dev, &raw);
    uint32_t reads_after = i2c_bus_test_get_read_count(BME280_I2C_ADDR_PRIMARY);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    /* Exactly 1 burst read transaction for all 8 registers */
    TEST_ASSERT_EQUAL_UINT32(1U, reads_after - reads_before);

    /* Verify unpacked 20-bit Pressure, 20-bit Temperature, 16-bit Humidity */
    TEST_ASSERT_EQUAL_INT32(EXPECTED_RAW_ADC_P, raw.adc_P);
    TEST_ASSERT_EQUAL_INT32(EXPECTED_RAW_ADC_T, raw.adc_T);
    TEST_ASSERT_EQUAL_INT32(EXPECTED_RAW_ADC_H, raw.adc_H);
}

/**
 * @brief TC-S4-T1.3-02: Bosch Reference Vector Temperature Compensation.
 */
static void test_bme280_compensation_temperature_vector(void) {
    bme280_calib_data_t calib = {
        .dig_T1 = VECTOR_DIG_T1,
        .dig_T2 = VECTOR_DIG_T2,
        .dig_T3 = VECTOR_DIG_T3
    };

    float t_fine = 0.0f;
    float temp_c = bme280_compensate_temperature(VECTOR_BOSCH_RAW_ADC_T, &calib, &t_fine);

    TEST_ASSERT_FLOAT_WITHIN(0.02f, EXPECTED_BOSCH_COMP_T, temp_c);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 128422.0f, t_fine);
}

/**
 * @brief TC-S4-T1.3-03: Bosch Reference Vector Pressure Compensation.
 */
static void test_bme280_compensation_pressure_vector(void) {
    bme280_calib_data_t calib = {
        .dig_T1 = VECTOR_DIG_T1,
        .dig_T2 = VECTOR_DIG_T2,
        .dig_T3 = VECTOR_DIG_T3,
        .dig_P1 = VECTOR_DIG_P1,
        .dig_P2 = VECTOR_DIG_P2,
        .dig_P3 = VECTOR_DIG_P3,
        .dig_P4 = VECTOR_DIG_P4,
        .dig_P5 = VECTOR_DIG_P5,
        .dig_P6 = VECTOR_DIG_P6,
        .dig_P7 = VECTOR_DIG_P7,
        .dig_P8 = VECTOR_DIG_P8,
        .dig_P9 = VECTOR_DIG_P9
    };

    float t_fine = 0.0f;
    (void)bme280_compensate_temperature(VECTOR_BOSCH_RAW_ADC_T, &calib, &t_fine);

    float press_hpa = bme280_compensate_pressure(VECTOR_BOSCH_RAW_ADC_P, &calib, t_fine);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, EXPECTED_BOSCH_COMP_P, press_hpa);
}

/**
 * @brief TC-S4-T1.3-04: Bosch Reference Vector Humidity Compensation.
 */
static void test_bme280_compensation_humidity_vector(void) {
    bme280_calib_data_t calib = {
        .dig_T1 = VECTOR_DIG_T1,
        .dig_T2 = VECTOR_DIG_T2,
        .dig_T3 = VECTOR_DIG_T3,
        .dig_H1 = VECTOR_DIG_H1,
        .dig_H2 = VECTOR_DIG_H2,
        .dig_H3 = VECTOR_DIG_H3,
        .dig_H4 = VECTOR_DIG_H4,
        .dig_H5 = VECTOR_DIG_H5,
        .dig_H6 = VECTOR_DIG_H6
    };

    float t_fine = 0.0f;
    (void)bme280_compensate_temperature(VECTOR_BOSCH_RAW_ADC_T, &calib, &t_fine);

    float hum_pct = bme280_compensate_humidity(VECTOR_BOSCH_RAW_ADC_H, &calib, t_fine);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, EXPECTED_BOSCH_COMP_H, hum_pct);
}

/**
 * @brief TC-S4-T1.3-05: Sub-Zero Negative Temperature Handling.
 */
static void test_bme280_compensation_negative_temperature(void) {
    bme280_calib_data_t calib = {
        .dig_T1 = VECTOR_DIG_T1,
        .dig_T2 = VECTOR_DIG_T2,
        .dig_T3 = VECTOR_DIG_T3
    };

    float t_fine = 0.0f;
    /* adc_T = 400000 is sub-zero */
    float temp_c = bme280_compensate_temperature(400000, &calib, &t_fine);

    TEST_ASSERT_TRUE(temp_c < 0.0f);
    TEST_ASSERT_TRUE(temp_c >= BME280_TEMP_MIN_C);
}

/**
 * @brief TC-S4-T1.3-06: Zero-Division Protection in Pressure Math.
 */
static void test_bme280_compensation_zero_division_guard(void) {
    bme280_calib_data_t calib = {0};
    calib.dig_P1 = 0; /* Zero multiplier causing var1 <= 0 */

    float press_hpa = bme280_compensate_pressure(VECTOR_BOSCH_RAW_ADC_P, &calib, 128422.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, press_hpa);
}

/**
 * @brief TC-S4-T1.3-07 & 08: Humidity Boundary Clamping ([0.0%, 100.0%]).
 */
static void test_bme280_compensation_humidity_clamping(void) {
    bme280_calib_data_t calib = {
        .dig_H1 = 10,
        .dig_H2 = 1000,
        .dig_H3 = 0,
        .dig_H4 = 100,
        .dig_H5 = 10,
        .dig_H6 = 10
    };

    /* High humidity saturation */
    float high_hum = bme280_compensate_humidity(60000, &calib, 128422.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, high_hum);

    /* Low/negative unconstrained */
    float low_hum = bme280_compensate_humidity(0, &calib, 128422.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, low_hum);
}

/**
 * @brief TC-S4-T1.3-09: Fixed-Point Scaled Integer Conversion.
 */
static void test_bme280_compensation_fixed_point_scaling(void) {
    bme280_calib_data_t calib = {
        .dig_T1 = VECTOR_DIG_T1,
        .dig_T2 = VECTOR_DIG_T2,
        .dig_T3 = VECTOR_DIG_T3,
        .dig_P1 = VECTOR_DIG_P1,
        .dig_P2 = VECTOR_DIG_P2,
        .dig_P3 = VECTOR_DIG_P3,
        .dig_P4 = VECTOR_DIG_P4,
        .dig_P5 = VECTOR_DIG_P5,
        .dig_P6 = VECTOR_DIG_P6,
        .dig_P7 = VECTOR_DIG_P7,
        .dig_P8 = VECTOR_DIG_P8,
        .dig_P9 = VECTOR_DIG_P9,
        .dig_H1 = VECTOR_DIG_H1,
        .dig_H2 = VECTOR_DIG_H2,
        .dig_H3 = VECTOR_DIG_H3,
        .dig_H4 = VECTOR_DIG_H4,
        .dig_H5 = VECTOR_DIG_H5,
        .dig_H6 = VECTOR_DIG_H6
    };

    bme280_raw_data_t raw = {
        .adc_T = VECTOR_BOSCH_RAW_ADC_T,
        .adc_P = VECTOR_BOSCH_RAW_ADC_P,
        .adc_H = VECTOR_BOSCH_RAW_ADC_H
    };

    bme280_fixed_data_t fixed_data;
    status_t status = bme280_compensate_raw_fixed(&raw, &calib, &fixed_data);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(fixed_data.is_valid);
    /* 25.08 °C -> 2508 centi-°C */
    TEST_ASSERT_INT16_WITHIN(5, 2508, fixed_data.temp_centi_c);
    /* 1006.53 hPa -> 100653 Pa */
    TEST_ASSERT_UINT32_WITHIN(10, 100653U, fixed_data.press_pascals);
    /* 54.32 % -> 5432 centi-% */
    TEST_ASSERT_UINT16_WITHIN(10, 5432U, fixed_data.hum_centi_percent);
}

/**
 * @brief TC-S4-T1.3-10: End-to-End read_data workflow.
 */
static void test_bme280_read_data_end_to_end(void) {
    bme280_dev_t dev;
    status_t status = bme280_init(&dev, BME280_I2C_ADDR_PRIMARY);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    bme280_data_t data;
    memset(&data, 0, sizeof(data));

    status = bme280_read_data(&dev, &data);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(data.is_valid);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, EXPECTED_BOSCH_COMP_T, data.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, EXPECTED_BOSCH_COMP_P, data.pressure_hpa);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, EXPECTED_BOSCH_COMP_H, data.humidity_percent);
}

/**
 * @brief Defensive NULL pointer and invalid parameter checking.
 */
static void test_bme280_null_and_invalid_params(void) {
    bme280_dev_t dev;
    uint8_t chip_id = 0;
    bool measuring = false;
    bme280_raw_data_t raw;
    bme280_config_t config;
    bme280_data_t data;
    bme280_fixed_data_t fixed_data;
    float t_fine = 0.0f;

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_init(NULL, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, bme280_init(&dev, 0x12U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_chip_id(NULL, &chip_id));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_chip_id(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_calibration(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_validate_calibration(NULL));

    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_configure(NULL, &config));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_configure(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_trigger_forced_mode(NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_is_measuring(NULL, &measuring));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_is_measuring(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_wait_for_completion(NULL, 60U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_raw_data(NULL, &raw));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_raw_data(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_sample_forced_raw(NULL, &raw));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_sample_forced_raw(&dev, NULL));

    /* Compensation NULL guards */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bme280_compensate_temperature(100, NULL, &t_fine));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bme280_compensate_temperature(100, &dev.calib, NULL));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bme280_compensate_pressure(100, NULL, 1000.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bme280_compensate_humidity(100, NULL, 1000.0f));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_compensate_raw(NULL, &dev.calib, &data));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_compensate_raw(&raw, NULL, &data));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_compensate_raw(&raw, &dev.calib, NULL));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_compensate_raw_fixed(NULL, &dev.calib, &fixed_data));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_data(NULL, &data));
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_NULL_PTR, bme280_read_data(&dev, NULL));
}

int main(void) {
    UNITY_BEGIN();

    /* Calibration & ID Tests (S4-T1.1) */
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

    /* Forced-Mode & Raw Readout Tests (S4-T1.2) */
    RUN_TEST(test_bme280_configure_sequence);
    RUN_TEST(test_bme280_trigger_forced_mode);
    RUN_TEST(test_bme280_is_measuring);
    RUN_TEST(test_bme280_wait_for_completion);
    RUN_TEST(test_bme280_read_raw_data_and_burst);

    /* Mathematical Compensation Tests (S4-T1.3) */
    RUN_TEST(test_bme280_compensation_temperature_vector);
    RUN_TEST(test_bme280_compensation_pressure_vector);
    RUN_TEST(test_bme280_compensation_humidity_vector);
    RUN_TEST(test_bme280_compensation_negative_temperature);
    RUN_TEST(test_bme280_compensation_zero_division_guard);
    RUN_TEST(test_bme280_compensation_humidity_clamping);
    RUN_TEST(test_bme280_compensation_fixed_point_scaling);
    RUN_TEST(test_bme280_read_data_end_to_end);

    /* Defensive Parameter Guards */
    RUN_TEST(test_bme280_null_and_invalid_params);

    return UNITY_END();
}
