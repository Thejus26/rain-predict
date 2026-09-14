/**
 * @file    test_sdi12.c
 * @brief   ThrowTheSwitch Unity unit test suite for SDI-12 Agricultural Bus Driver (S4-T5.1).
 * @details Validates break/mark timing, direction toggling, command transmission,
 *          and ASCII response acquisition under host simulation with mock UART & GPIO.
 */

#include "unity.h"
#include "sdi12_driver.h"
#include "uart_bus.h"
#include "gpio_driver.h"
#include "mock_uart_bus.h"
#include "mock_gpio.h"
#include "board_config.h"
#include "status.h"
#include <string.h>

/* ============================================================================
 * Test Setup & Teardown
 * ============================================================================ */

void setUp(void) {
    mock_uart_reset();
    mock_gpio_reset();
    uart_bus_test_reset();
    sdi12_test_reset();
    (void)sdi12_init();
}

void tearDown(void) {
    /* Verify direction pin is always restored to RX mode (PC2 = LOW) */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
}

/* ============================================================================
 * Physical Layer & Direction Control Tests (Task S4-T5.1)
 * ============================================================================ */

static void test_sdi12_init_defaults(void) {
    /* Verify direction pin PC2 is initialized to LOW (RX Mode) */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
    TEST_ASSERT_TRUE(uart_bus_test_is_initialized(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT32(1200U, uart_bus_test_get_baud_rate(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(7U, uart_bus_test_get_data_bits(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)UART_PARITY_EVEN, uart_bus_test_get_parity(UART_PORT_SDI12));
    TEST_ASSERT_EQUAL_UINT8(1U, uart_bus_test_get_stop_bits(UART_PORT_SDI12));
}

static void test_sdi12_direction_control(void) {
    sdi12_set_direction(SDI12_DIR_TX);
    TEST_ASSERT_EQUAL_INT(1, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_TX, sdi12_test_get_direction());

    sdi12_set_direction(SDI12_DIR_RX);
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_send_break_and_mark_execution(void) {
    sdi12_send_break_and_mark();

    /* Verify break/mark leaves direction pin in TX mode before command TX */
    TEST_ASSERT_EQUAL_INT(1, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_TX, sdi12_test_get_direction());
    TEST_ASSERT_TRUE(sdi12_test_get_break_sent());
    TEST_ASSERT_TRUE(uart_bus_test_get_break_sent());
    TEST_ASSERT_EQUAL_UINT32(13U, uart_bus_test_get_break_duration_ms());
    TEST_ASSERT_EQUAL_UINT32(9U, uart_bus_test_get_mark_duration_ms());

    /* Clean up to RX mode for tearDown check */
    sdi12_set_direction(SDI12_DIR_RX);
}

static void test_sdi12_transmit_command_valid(void) {
    status_t status = sdi12_transmit_command("0M!");
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify "0M!" was transmitted to mock UART */
    TEST_ASSERT_EQUAL_UINT16(3, mock_uart_get_tx_count(UART_PORT_SDI12));
    uint8_t tx_buf[8] = {0};
    (void)mock_uart_get_tx_bytes(UART_PORT_SDI12, tx_buf, sizeof(tx_buf));
    TEST_ASSERT_EQUAL_STRING("0M!", (char *)tx_buf);

    /* Verify line direction returned to RX listening mode */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_transmit_command_missing_exclamation(void) {
    /* Missing trailing '!' */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command("0M"));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command("0D0"));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_transmit_command(NULL));
}

static void test_sdi12_transmit_command_empty_and_overflow(void) {
    /* Empty command */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command(""));

    /* Command exceeding SDI12_MAX_BUFFER_SIZE */
    char long_cmd[80];
    (void)memset(long_cmd, 'A', sizeof(long_cmd) - 1);
    long_cmd[sizeof(long_cmd) - 2] = '!';
    long_cmd[sizeof(long_cmd) - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command(long_cmd));
}

static void test_sdi12_wake_and_transmit_valid(void) {
    status_t status = sdi12_wake_and_transmit("0D0!");
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify break sequence was emitted */
    TEST_ASSERT_TRUE(sdi12_test_get_break_sent());
    TEST_ASSERT_TRUE(uart_bus_test_get_break_sent());

    /* Verify command was transmitted */
    TEST_ASSERT_EQUAL_UINT16(4, mock_uart_get_tx_count(UART_PORT_SDI12));
    uint8_t tx_buf[8] = {0};
    (void)mock_uart_get_tx_bytes(UART_PORT_SDI12, tx_buf, sizeof(tx_buf));
    TEST_ASSERT_EQUAL_STRING("0D0!", (char *)tx_buf);

    /* Verify line direction returned to RX listening mode */
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_wake_and_transmit_null_and_invalid(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_wake_and_transmit(NULL));
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_wake_and_transmit("0D0"));
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

static void test_sdi12_receive_response_valid(void) {
    const char *mock_resp = "00023\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)mock_resp, (uint16_t)strlen(mock_resp));

    char rx_buf[16] = {0};
    uint16_t rx_len = 0;

    status_t status = sdi12_receive_response(rx_buf, sizeof(rx_buf), &rx_len, 1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT16(7U, rx_len);
    TEST_ASSERT_EQUAL_STRING("00023\r\n", rx_buf);
}

static void test_sdi12_receive_response_missing_crlf(void) {
    const char *bad_resp = "00023";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)bad_resp, (uint16_t)strlen(bad_resp));

    char rx_buf[16] = {0};
    uint16_t rx_len = 0;
    status_t status = sdi12_receive_response(rx_buf, sizeof(rx_buf), &rx_len, 1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, status);
}

static void test_sdi12_receive_response_timeout(void) {
    mock_uart_inject_fault(UART_PORT_SDI12, MOCK_UART_FAULT_TIMEOUT);

    char rx_buf[16] = {0};
    uint16_t rx_len = 0;
    status_t status = sdi12_receive_response(rx_buf, sizeof(rx_buf), &rx_len, 1000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_TIMEOUT, status);
}

static void test_sdi12_receive_response_null_and_overflow(void) {
    char rx_buf[16];
    uint16_t rx_len;

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_receive_response(NULL, sizeof(rx_buf), &rx_len, 1000U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_receive_response(rx_buf, sizeof(rx_buf), NULL, 1000U));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_BUFFER_OVERFLOW,
                          sdi12_receive_response(rx_buf, 2U, &rx_len, 1000U));
}

static void test_sdi12_deinit(void) {
    sdi12_set_direction(SDI12_DIR_TX);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_deinit());
    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
}

/* ============================================================================
 * Command Formatting & Response Parsing Unit Tests (Task S4-T5.2)
 * ============================================================================ */

/**
 * @brief TC-SDI2-01: Standard Command Formatting.
 */
static void test_sdi12_format_command_standard(void) {
    char buf[16] = {0};

    /* Address '0', Command 'M' -> "0M!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('0', "M", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("0M!", buf);

    /* Address 'a', Command '!' -> "a!!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('a', "!", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("a!!", buf);

    /* Address '9', Command 'I' -> "9I!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('9', "I", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("9I!", buf);
}

/**
 * @brief TC-SDI2-02: Data Command & Extended Formatting.
 */
static void test_sdi12_format_command_data_and_extended(void) {
    char buf[16] = {0};

    /* Address '1', Command 'D0' -> "1D0!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('1', "D0", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("1D0!", buf);

    /* Address '2', Command 'C' -> "2C!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('2', "C", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("2C!", buf);

    /* Address '0', Change Address 'A1' -> "0A1!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('0', "A1", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("0A1!", buf);

    /* Address Discovery '?', Command '' -> "?!" */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('?', "", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("?!", buf);
}

/**
 * @brief Defensive parameter & buffer validation for command formatting.
 */
static void test_sdi12_format_command_defensive(void) {
    char buf[16];

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_format_command('0', NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_format_command('0', "M", NULL, sizeof(buf)));

    /* Invalid address characters */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_format_command('#', "M", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_format_command('!', "M", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_format_command(' ', "M", buf, sizeof(buf)));

    /* Buffer overflow checks */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_BUFFER_OVERFLOW, sdi12_format_command('0', "M", buf, 3U));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_format_command('0', "M", buf, 4U));
    TEST_ASSERT_EQUAL_STRING("0M!", buf);
}

/**
 * @brief TC-SDI2-03: Measurement Info Parsing.
 */
static void test_sdi12_parse_measurement_info_valid(void) {
    uint16_t wait_sec = 0U;
    uint8_t  val_count = 0U;

    /* "00023\r\n" -> wait=2s, count=3 */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_parse_measurement_info("00023\r\n", '0', &wait_sec, &val_count));
    TEST_ASSERT_EQUAL_UINT16(2U, wait_sec);
    TEST_ASSERT_EQUAL_UINT8(3U, val_count);

    /* "10309\r\n" -> wait=30s, count=9 */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_parse_measurement_info("10309\r\n", '1', &wait_sec, &val_count));
    TEST_ASSERT_EQUAL_UINT16(30U, wait_sec);
    TEST_ASSERT_EQUAL_UINT8(9U, val_count);

    /* Wildcard address matching */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_parse_measurement_info("50001\r\n", '?', &wait_sec, &val_count));
    TEST_ASSERT_EQUAL_UINT16(0U, wait_sec);
    TEST_ASSERT_EQUAL_UINT8(1U, val_count);
}

/**
 * @brief TC-SDI2-04: Measurement Info Address Mismatch.
 */
static void test_sdi12_parse_measurement_info_address_mismatch(void) {
    uint16_t wait_sec = 0U;
    uint8_t  val_count = 0U;

    /* Expected '0', Received '1' */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_measurement_info("10023\r\n", '0', &wait_sec, &val_count));
}

/**
 * @brief Defensive checks for measurement info parsing.
 */
static void test_sdi12_parse_measurement_info_defensive(void) {
    uint16_t wait_sec = 0U;
    uint8_t  val_count = 0U;

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_parse_measurement_info(NULL, '0', &wait_sec, &val_count));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_parse_measurement_info("00023\r\n", '0', NULL, &val_count));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_parse_measurement_info("00023\r\n", '0', &wait_sec, NULL));

    /* String too short */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_measurement_info("0002\r\n", '0', &wait_sec, &val_count));

    /* Missing CRLF */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_measurement_info("00023AB", '0', &wait_sec, &val_count));

    /* Non-digit wait time or count */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_measurement_info("00a23\r\n", '0', &wait_sec, &val_count));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_measurement_info("0002x\r\n", '0', &wait_sec, &val_count));
}

/**
 * @brief TC-SDI2-05: 3-Value Floating Point Parsing.
 */
static void test_sdi12_parse_data_response_three_values(void) {
    float values[SDI12_MAX_VALUES_PER_CMD] = {0};
    uint8_t actual_count = 0U;

    status_t status = sdi12_parse_data_response("0+0.354+22.10+1.24\r\n", '0',
                                               values, SDI12_MAX_VALUES_PER_CMD, &actual_count);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(3U, actual_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.354f, values[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 22.10f, values[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.24f, values[2]);
}

/**
 * @brief TC-SDI2-06: Negative Value Token Parsing.
 */
static void test_sdi12_parse_data_response_negative_values(void) {
    float values[SDI12_MAX_VALUES_PER_CMD] = {0};
    uint8_t actual_count = 0U;

    status_t status = sdi12_parse_data_response("0-12.50+88.20-1.05\r\n", '0',
                                               values, SDI12_MAX_VALUES_PER_CMD, &actual_count);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(3U, actual_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -12.50f, values[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 88.20f, values[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -1.05f, values[2]);
}

/**
 * @brief TC-SDI2-07: Scientific Notation Token Parsing.
 */
static void test_sdi12_parse_data_response_scientific_notation(void) {
    float values[SDI12_MAX_VALUES_PER_CMD] = {0};
    uint8_t actual_count = 0U;

    status_t status = sdi12_parse_data_response("0+1.23E-01-4.56e+01\r\n", '0',
                                               values, SDI12_MAX_VALUES_PER_CMD, &actual_count);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(2U, actual_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.123f, values[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -45.60f, values[1]);
}

/**
 * @brief Defensive checks for data response parsing.
 */
static void test_sdi12_parse_data_response_defensive(void) {
    float values[SDI12_MAX_VALUES_PER_CMD] = {0};
    uint8_t actual_count = 0U;

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_parse_data_response(NULL, '0', values, 4U, &actual_count));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_parse_data_response("0+1.0\r\n", '0', NULL, 4U, &actual_count));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
                          sdi12_parse_data_response("0+1.0\r\n", '0', values, 4U, NULL));

    /* max_values = 0 */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM,
                          sdi12_parse_data_response("0+1.0\r\n", '0', values, 0U, &actual_count));

    /* Address mismatch */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_data_response("1+1.0\r\n", '0', values, 4U, &actual_count));

    /* Missing CRLF */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_data_response("0+1.0", '0', values, 4U, &actual_count));

    /* No float tokens present */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          sdi12_parse_data_response("0ABC\r\n", '0', values, 4U, &actual_count));

    /* Max values clamping */
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          sdi12_parse_data_response("0+1.0+2.0+3.0\r\n", '0', values, 2U, &actual_count));
    TEST_ASSERT_EQUAL_UINT8(2U, actual_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, values[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, values[1]);

    /* Wildcard expected address */
    TEST_ASSERT_EQUAL_INT(STATUS_OK,
                          sdi12_parse_data_response("9+42.0\r\n", '?', values, 4U, &actual_count));
    TEST_ASSERT_EQUAL_UINT8(1U, actual_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 42.0f, values[0]);
}

/**
 * @brief TC-SDI2-08: Identification String Parsing.
 */
static void test_sdi12_parse_identification_valid(void) {
    sdi12_sensor_info_t info;
    const char *resp = "014METER   TEROS121001234\r\n";

    status_t status = sdi12_parse_identification(resp, '0', &info);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT('0', info.address);
    TEST_ASSERT_EQUAL_STRING("14", info.sdi_version);
    TEST_ASSERT_EQUAL_STRING("METER   ", info.vendor_id);
    TEST_ASSERT_EQUAL_STRING("TEROS1", info.model_num);
    TEST_ASSERT_EQUAL_STRING("210", info.fw_version);
    TEST_ASSERT_EQUAL_STRING("01234", info.serial_num);
}

/**
 * @brief Identification parsing without serial number and with wildcard address.
 */
static void test_sdi12_parse_identification_no_serial_and_wildcard(void) {
    sdi12_sensor_info_t info;
    const char *resp_no_sn = "113CAMPBELLCR1000100\r\n";

    status_t status = sdi12_parse_identification(resp_no_sn, '?', &info);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT('1', info.address);
    TEST_ASSERT_EQUAL_STRING("13", info.sdi_version);
    TEST_ASSERT_EQUAL_STRING("CAMPBELL", info.vendor_id);
    TEST_ASSERT_EQUAL_STRING("CR1000", info.model_num);
    TEST_ASSERT_EQUAL_STRING("100", info.fw_version);
    TEST_ASSERT_EQUAL_STRING("", info.serial_num);

    /* Long serial number clamping */
    const char *resp_long_sn = "214METER   TEROS12101234567890123456\r\n";
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_parse_identification(resp_long_sn, '2', &info));
    TEST_ASSERT_EQUAL_STRING("1234567890123", info.serial_num);
}

/**
 * @brief Defensive checks for identification parsing.
 */
static void test_sdi12_parse_identification_defensive(void) {
    sdi12_sensor_info_t info;

    /* NULL pointers */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_parse_identification(NULL, '0', &info));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_parse_identification("014METER   TEROS121001234\r\n", '0', NULL));

    /* Frame too short (< 22 bytes) */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, sdi12_parse_identification("014METER   TEROS1\r\n", '0', &info));

    /* Missing CRLF */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, sdi12_parse_identification("014METER   TEROS121001234AB", '0', &info));

    /* Address mismatch */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, sdi12_parse_identification("114METER   TEROS121001234\r\n", '0', &info));
}

/**
 * @brief TC-SDI2-09: Full Soil Probe Query Orchestration.
 */
static void test_sdi12_query_soil_probe_orchestration(void) {
    /* Inject Stage 1 (aM! response) followed by Stage 2 (aD0! response) */
    const char *combined_resp = "00003\r\n0+0.354+22.10+1.24\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)combined_resp, (uint16_t)strlen(combined_resp));

    sdi12_soil_reading_t reading = {0};
    status_t status = sdi12_query_soil_probe('0', &reading, 1000U);

    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT('0', reading.sensor_addr);
    TEST_ASSERT_EQUAL_UINT8(3U, reading.num_values);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.354f, reading.vwc_m3_m3);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 22.10f, reading.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.24f, reading.bulk_ec_ds_m);
}

/**
 * @brief Soil probe query with agronomic clamping and error conditions.
 */
static void test_sdi12_query_soil_probe_clamping_and_errors(void) {
    sdi12_soil_reading_t reading = {0};

    /* Negative VWC and EC clamping */
    const char *resp_neg = "00003\r\n0-0.05+18.50-0.20\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)resp_neg, (uint16_t)strlen(resp_neg));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_query_soil_probe('0', &reading, 1000U));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, reading.vwc_m3_m3);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 18.50f, reading.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, reading.bulk_ec_ds_m);

    /* Excessive VWC clamping (> 1.0 m3/m3) */
    const char *resp_high = "00003\r\n0+1.25+24.00+2.50\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)resp_high, (uint16_t)strlen(resp_high));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_query_soil_probe('0', &reading, 1000U));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, reading.vwc_m3_m3);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 24.00f, reading.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.50f, reading.bulk_ec_ds_m);

    /* NULL pointer */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_query_soil_probe('0', NULL, 1000U));

    /* Stage 1 timeout */
    mock_uart_inject_fault(UART_PORT_SDI12, MOCK_UART_FAULT_TIMEOUT);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_TIMEOUT, sdi12_query_soil_probe('0', &reading, 1000U));
    mock_uart_clear_faults(UART_PORT_SDI12);

    /* Stage 1 zero value count */
    const char *resp_zero_count = "00000\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)resp_zero_count, (uint16_t)strlen(resp_zero_count));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, sdi12_query_soil_probe('0', &reading, 1000U));
}

/**
 * @brief TC-SDI2-10: Address Discovery Query (?!) & Defensive Checks.
 */
static void test_sdi12_query_address_discovery(void) {
    char found_addr = 0;

    /* Discover address '0' */
    const char *resp0 = "0\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)resp0, (uint16_t)strlen(resp0));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_query_address(&found_addr, 1000U));
    TEST_ASSERT_EQUAL_INT('0', found_addr);

    /* Discover address 'B' */
    const char *respB = "B\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)respB, (uint16_t)strlen(respB));
    TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_query_address(&found_addr, 1000U));
    TEST_ASSERT_EQUAL_INT('B', found_addr);

    /* NULL pointer check */
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_query_address(NULL, 1000U));

    /* Timeout error */
    mock_uart_inject_fault(UART_PORT_SDI12, MOCK_UART_FAULT_TIMEOUT);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_TIMEOUT, sdi12_query_address(&found_addr, 1000U));
    mock_uart_clear_faults(UART_PORT_SDI12);

    /* Invalid non-alphanumeric response */
    const char *bad_resp = "#\r\n";
    (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)bad_resp, (uint16_t)strlen(bad_resp));
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, sdi12_query_address(&found_addr, 1000U));
}

/* ============================================================================
 * Main Test Runner Entry Point
 * ============================================================================ */

int main(void) {
    UNITY_BEGIN();

    /* S4-T5.1 Physical Layer & Timing Tests */
    RUN_TEST(test_sdi12_init_defaults);
    RUN_TEST(test_sdi12_direction_control);
    RUN_TEST(test_sdi12_send_break_and_mark_execution);
    RUN_TEST(test_sdi12_transmit_command_valid);
    RUN_TEST(test_sdi12_transmit_command_missing_exclamation);
    RUN_TEST(test_sdi12_transmit_command_empty_and_overflow);
    RUN_TEST(test_sdi12_wake_and_transmit_valid);
    RUN_TEST(test_sdi12_wake_and_transmit_null_and_invalid);
    RUN_TEST(test_sdi12_receive_response_valid);
    RUN_TEST(test_sdi12_receive_response_missing_crlf);
    RUN_TEST(test_sdi12_receive_response_timeout);
    RUN_TEST(test_sdi12_receive_response_null_and_overflow);
    RUN_TEST(test_sdi12_deinit);

    /* S4-T5.2 Command Formatter & Response Parser Tests */
    RUN_TEST(test_sdi12_format_command_standard);
    RUN_TEST(test_sdi12_format_command_data_and_extended);
    RUN_TEST(test_sdi12_format_command_defensive);
    RUN_TEST(test_sdi12_parse_measurement_info_valid);
    RUN_TEST(test_sdi12_parse_measurement_info_address_mismatch);
    RUN_TEST(test_sdi12_parse_measurement_info_defensive);
    RUN_TEST(test_sdi12_parse_data_response_three_values);
    RUN_TEST(test_sdi12_parse_data_response_negative_values);
    RUN_TEST(test_sdi12_parse_data_response_scientific_notation);
    RUN_TEST(test_sdi12_parse_data_response_defensive);
    RUN_TEST(test_sdi12_parse_identification_valid);
    RUN_TEST(test_sdi12_parse_identification_no_serial_and_wildcard);
    RUN_TEST(test_sdi12_parse_identification_defensive);
    RUN_TEST(test_sdi12_query_soil_probe_orchestration);
    RUN_TEST(test_sdi12_query_soil_probe_clamping_and_errors);
    RUN_TEST(test_sdi12_query_address_discovery);

    return UNITY_END();
}
