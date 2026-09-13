/**
 * @file    test_board_config.c
 * @brief   Unit test suite for Target Board GPIO Pin Mappings and Configuration.
 * @details Validates peripheral pin mappings, default states, deep sleep low-leakage conditioning,
 *          switched power rail sequencing, battery divider control, and field actuators.
 */

#include "unity.h"
#include "board_config.h"

void setUp(void) {
    board_test_reset();
}

void tearDown(void) {
    /* No dynamic memory to free */
}

/**
 * @brief TC-S3-T1.1-01: Verify board GPIO initialization sets safe default output states & modes.
 */
static void test_board_gpio_init_default_states(void) {
    status_t status = board_gpio_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* 1. Verify safe initial output logic levels */
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET,   board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_SHUTDOWN, board_test_get_rf_mode());

    /* 2. Verify output pin modes */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_BUZZER_PORT, PIN_BUZZER_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_RELAY_PORT, PIN_RELAY_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));

    /* 3. Verify input & EXTI pin modes */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_INPUT, board_test_get_pin_mode(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_PULLUP, board_test_get_pin_pull(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));

    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_IT_FALLING, board_test_get_pin_mode(PIN_RAIN_GAUGE_PORT, PIN_RAIN_GAUGE_PIN));

    /* 4. Verify unpowered digital buses & ADC configured to Analog (zero leakage) */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_VBAT_MEAS_PORT, PIN_VBAT_MEAS_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));
}

/**
 * @brief TC-S3-T1.1-02: Verify switched sensor power rail sequencing, delay, and bus AF mode transitions.
 */
static void test_board_sensor_power_sequencing(void) {
    status_t status = board_gpio_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Initially rail is disabled */
    TEST_ASSERT_FALSE(board_test_is_sensor_power_enabled());
    TEST_ASSERT_EQUAL_UINT32(0, board_test_get_delay_call_count());

    /* Enable switched sensor rail */
    status = board_sensor_power_enable(true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(board_test_is_sensor_power_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));

    /* Verify 20ms stabilization delay occurred */
    TEST_ASSERT_EQUAL_UINT32(1, board_test_get_delay_call_count());
    TEST_ASSERT_EQUAL_UINT32(BOARD_POWER_RAIL_STABILIZATION_MS, board_test_get_last_delay_ms());

    /* Verify bus pins switched to active Alternate Function modes */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_OD, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF4_I2C1,   board_test_get_pin_af(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_OD, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF4_I2C1,   board_test_get_pin_af(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));

    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF7_USART1, board_test_get_pin_af(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF7_USART1, board_test_get_pin_af(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));

    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP,  board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF8_LPUART1, board_test_get_pin_af(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP,  board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF8_LPUART1, board_test_get_pin_af(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));

    /* Disable switched sensor rail */
    status = board_sensor_power_enable(false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(board_test_is_sensor_power_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));

    /* Verify bus pins safely isolated back to Analog No-Pull to prevent phantom leakage */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));
}

/**
 * @brief TC-S3-T1.1-03: Verify battery voltage divider gate control (active-LOW logic).
 */
static void test_board_vbat_divider_control(void) {
    (void)board_gpio_init();

    /* Initially divider is disabled (PB1 HIGH) */
    TEST_ASSERT_FALSE(board_test_is_vbat_divider_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));

    /* Enable battery divider (PB1 driven LOW) */
    status_t status = board_vbat_divider_enable(true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(board_test_is_vbat_divider_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));

    /* Disable battery divider (PB1 driven HIGH to prevent standby drain) */
    status = board_vbat_divider_enable(false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(board_test_is_vbat_divider_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));
}

/**
 * @brief TC-S3-T1.1-04: Verify Stop 2 deep sleep conditioning and post-wake pin restoration.
 */
static void test_board_sleep_prepare_and_wake_restore(void) {
    (void)board_gpio_init();

    /* Turn on multiple peripherals, actuators, and RF mode */
    (void)board_sensor_power_enable(true);
    (void)board_vbat_divider_enable(true);
    board_led_set(BOARD_LED_OK, true);
    board_led_set(BOARD_LED_WARN, true);
    board_buzzer_set(true);
    board_relay_set(true);
    board_rs485_dir_set(BOARD_RS485_DIR_TX);
    board_sdi12_dir_set(BOARD_SDI12_DIR_TX);
    board_rf_switch_set(RF_SWITCH_TX_HP);

    /* Verify active states */
    TEST_ASSERT_TRUE(board_test_is_sensor_power_enabled());
    TEST_ASSERT_TRUE(board_test_is_vbat_divider_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN));
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_TX_HP, board_test_get_rf_mode());

    /* Condition board for Stop 2 Deep Sleep */
    status_t status = board_gpio_sleep_prepare();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify all outputs and loads de-energized */
    TEST_ASSERT_FALSE(board_test_is_sensor_power_enabled());
    TEST_ASSERT_FALSE(board_test_is_vbat_divider_enabled());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET,   board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_SHUTDOWN, board_test_get_rf_mode());

    /* Verify bus pins, ADC, SPI, and Button isolated to Analog No-Pull */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_VBAT_MEAS_PORT, PIN_VBAT_MEAS_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));

    /* Verify EXTI0 rain gauge wake interrupt remains configured */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_IT_FALLING, board_test_get_pin_mode(PIN_RAIN_GAUGE_PORT, PIN_RAIN_GAUGE_PIN));

    /* Perform wake restore */
    status = board_gpio_wake_restore();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify button restored to Input Pullup */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_INPUT, board_test_get_pin_mode(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_PULLUP, board_test_get_pin_pull(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));
}

/**
 * @brief TC-S3-T1.1-05: Verify status LED operations (set, toggle, and discrete LED channels).
 */
static void test_board_led_control(void) {
    (void)board_gpio_init();

    /* LED OK (Green) Set & Clear */
    board_led_set(BOARD_LED_OK, true);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));

    board_led_set(BOARD_LED_OK, false);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));

    /* LED WARN (Red) Set & Clear */
    board_led_set(BOARD_LED_WARN, true);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));

    board_led_set(BOARD_LED_WARN, false);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));

    /* Toggle operations */
    board_led_toggle(BOARD_LED_OK);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    board_led_toggle(BOARD_LED_OK);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));

    board_led_toggle(BOARD_LED_WARN);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    board_led_toggle(BOARD_LED_WARN);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
}

/**
 * @brief TC-S3-T1.1-06: Verify alarm buzzer and estate siren relay gate drivers.
 */
static void test_board_buzzer_and_relay(void) {
    (void)board_gpio_init();

    /* Buzzer control */
    board_buzzer_set(true);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN));
    board_buzzer_set(false);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN));

    /* Relay control */
    board_relay_set(true);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN));
    board_relay_set(false);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN));
}

/**
 * @brief TC-S3-T1.1-07: Verify RS-485 and SDI-12 transceiver direction switching.
 */
static void test_board_bus_direction_control(void) {
    (void)board_gpio_init();

    /* RS-485 DE/RE direction */
    board_rs485_dir_set(BOARD_RS485_DIR_TX);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN));

    board_rs485_dir_set(BOARD_RS485_DIR_RX);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN));

    /* SDI-12 direction */
    board_sdi12_dir_set(BOARD_SDI12_DIR_TX);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN));

    board_sdi12_dir_set(BOARD_SDI12_DIR_RX);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN));
}

/**
 * @brief TC-S3-T1.1-08: Verify Sub-GHz RF switch truth table across all operational modes.
 */
static void test_board_rf_switch_truth_table(void) {
    (void)board_gpio_init();

    /* RF_SWITCH_RX (CTRL1=1, CTRL2=0, CTRL3=0) */
    board_rf_switch_set(RF_SWITCH_RX);
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_RX, board_test_get_rf_mode());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET,   board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));

    /* RF_SWITCH_TX_LP (CTRL1=0, CTRL2=1, CTRL3=0) */
    board_rf_switch_set(RF_SWITCH_TX_LP);
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_TX_LP, board_test_get_rf_mode());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET,   board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));

    /* RF_SWITCH_TX_HP (CTRL1=0, CTRL2=0, CTRL3=1) */
    board_rf_switch_set(RF_SWITCH_TX_HP);
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_TX_HP, board_test_get_rf_mode());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET,   board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));

    /* RF_SWITCH_SHUTDOWN (CTRL1=0, CTRL2=0, CTRL3=0) */
    board_rf_switch_set(RF_SWITCH_SHUTDOWN);
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_SHUTDOWN, board_test_get_rf_mode());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));

    /* Default / invalid fallback */
    board_rf_switch_set((board_rf_mode_t)99);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));
}

/**
 * @brief TC-S3-T1.1-09: Verify user diagnostic button input sensing (active-LOW logic).
 */
static void test_board_button_sensing(void) {
    (void)board_gpio_init();

    /* Default unpressed (pulled HIGH) */
    board_test_set_button_pressed(false);
    TEST_ASSERT_FALSE(board_button_is_pressed());

    /* Pressed (pulled LOW to GND) */
    board_test_set_button_pressed(true);
    TEST_ASSERT_TRUE(board_button_is_pressed());

    /* Released again */
    board_test_set_button_pressed(false);
    TEST_ASSERT_FALSE(board_button_is_pressed());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_board_gpio_init_default_states);
    RUN_TEST(test_board_sensor_power_sequencing);
    RUN_TEST(test_board_vbat_divider_control);
    RUN_TEST(test_board_sleep_prepare_and_wake_restore);
    RUN_TEST(test_board_led_control);
    RUN_TEST(test_board_buzzer_and_relay);
    RUN_TEST(test_board_bus_direction_control);
    RUN_TEST(test_board_rf_switch_truth_table);
    RUN_TEST(test_board_button_sensing);
    return UNITY_END();
}
