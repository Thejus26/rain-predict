/**
 * @file    test_power_mgr.c
 * @brief   Unit test verification suite for Stop 2 Deep Sleep & RTC Wakeup Manager (S3-T4.1 & S3-T4.2).
 * @details Validates bus pin analog tri-stating, floating CMOS shoot-through suppression,
 *          wakeup/oscillator exemption preservation, post-wake pin restoration, diagnostic leakage checks,
 *          RTC periodic wakeup timer configuration, multi-source wakeup detection (RTC, Rain EXTI0, Button),
 *          fast clock restoration to 48 MHz MSI, cumulative sleep metrics tracking, and shelf-storage Standby mode.
 */

#include "unity.h"
#include "power_mgr.h"
#include "board_config.h"
#include "system_clock.h"
#include "bsp_power_rails.h"
#include "bsp_indicators.h"

void setUp(void) {
    board_test_reset();
    system_clock_test_reset();
    bsp_power_rails_test_reset();
    bsp_indicators_test_reset();
    power_mgr_test_reset();

    (void)board_gpio_init();
    (void)system_clock_init();
    (void)bsp_power_rails_init();
    (void)bsp_indicators_init();
    (void)power_mgr_init();
}

void tearDown(void) {
    /* Safe cleanup */
    (void)power_mgr_gpio_sleep_prepare();
}

/**
 * @brief TC-S3-T4.1-01: Header Inclusion, Type Definitions & C99 Compilation.
 */
static void test_power_mgr_init_and_states(void) {
    /* Verify power state enum constants */
    TEST_ASSERT_EQUAL_INT(0, (int)POWER_STATE_RUN);
    TEST_ASSERT_EQUAL_INT(1, (int)POWER_STATE_LP_RUN);
    TEST_ASSERT_EQUAL_INT(2, (int)POWER_STATE_STOP2);
    TEST_ASSERT_EQUAL_INT(3, (int)POWER_STATE_STANDBY);

    /* Verify wake reason enum constants */
    TEST_ASSERT_EQUAL_INT(0, (int)POWER_WAKE_REASON_UNKNOWN);
    TEST_ASSERT_EQUAL_INT(1, (int)POWER_WAKE_REASON_RTC);
    TEST_ASSERT_EQUAL_INT(2, (int)POWER_WAKE_REASON_RAIN_EXTI);
    TEST_ASSERT_EQUAL_INT(3, (int)POWER_WAKE_REASON_BUTTON);

    /* Verify timing interval constants */
    TEST_ASSERT_EQUAL_UINT32(600U, POWER_MGR_DEFAULT_SLEEP_SEC);
    TEST_ASSERT_EQUAL_UINT32(300U, POWER_MGR_WATCH_SLEEP_SEC);
    TEST_ASSERT_EQUAL_UINT32(120U, POWER_MGR_STORM_SLEEP_SEC);
    TEST_ASSERT_EQUAL_UINT32(900U, POWER_MGR_LOW_BAT_SLEEP_SEC);
    TEST_ASSERT_EQUAL_UINT32(3600U, POWER_MGR_CRITICAL_BAT_SLEEP_SEC);
    TEST_ASSERT_EQUAL_UINT32(1U, POWER_MGR_MIN_SLEEP_SEC);
    TEST_ASSERT_EQUAL_UINT32(65535U, POWER_MGR_MAX_SLEEP_SEC);

    /* Verify power manager initialization */
    status_t status = power_mgr_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_STATE_RUN, power_mgr_test_get_state());
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_REASON_UNKNOWN, power_mgr_get_wake_reason());
    TEST_ASSERT_EQUAL_UINT32(0U, power_mgr_get_total_sleep_time_sec());
    TEST_ASSERT_FALSE(power_mgr_test_is_rtc_wakeup_armed());
}

/**
 * @brief TC-S3-T4.1-02: Bus Pin Analog Isolation.
 */
static void test_power_mgr_isolate_sensor_buses(void) {
    /* 1. Energize switched sensor rail and activate bus AF modes */
    status_t status = board_sensor_power_enable(true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_OD, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_OD, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));

    /* 2. Execute sensor bus analog isolation */
    status = power_mgr_isolate_sensor_buses();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* 3. Verify I2C1 (PB6/PB7) isolated to Analog No-Pull */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));

    /* 4. Verify USART1 RS-485 (PA2/PA3) isolated to Analog No-Pull */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));

    /* 5. Verify LPUART1 SDI-12 (PC0/PC1) isolated to Analog No-Pull */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));

    /* 6. Verify SPI1 (PA5/PA6/PA7) isolated to Analog No-Pull */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, GPIO_PIN_5));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(GPIOA, GPIO_PIN_5));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, GPIO_PIN_6));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(GPIOA, GPIO_PIN_6));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, GPIO_PIN_7));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(GPIOA, GPIO_PIN_7));
}

/**
 * @brief TC-S3-T4.1-03: Parasitic Leakage Prevention & Power Rail Coordination.
 */
static void test_power_mgr_parasitic_leakage_prevention(void) {
    /* De-energize switched sensor rail */
    status_t status = bsp_power_rails_all_off();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));

    /* Isolate communication buses */
    status = power_mgr_isolate_sensor_buses();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify all bus pins are high-Z analog to eliminate parasitic back-powering via ESD diodes */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SDA_PORT, PIN_I2C1_SDA_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_RX_PORT, PIN_RS485_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_RX_PORT, PIN_SDI12_RX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, GPIO_PIN_5));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, GPIO_PIN_6));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, GPIO_PIN_7));
}

/**
 * @brief TC-S3-T4.1-04: Unused Pin Shoot-Through Suppression.
 */
static void test_power_mgr_unused_pin_conditioning(void) {
    /* Perform pre-sleep GPIO conditioning */
    status_t status = power_mgr_gpio_sleep_prepare();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_STATE_STOP2, power_mgr_test_get_state());

    /* 1. Inspect Port A unrouted pins: PA8, PA9, PA10, PA11, PA12, PA15 */
    static const uint16_t unused_a[] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_15};
    for (size_t i = 0; i < sizeof(unused_a) / sizeof(unused_a[0]); i++) {
        TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOA, unused_a[i]));
        TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(GPIOA, unused_a[i]));
    }

    /* 2. Inspect Port B unrouted pins & ADC: PB0, PB3, PB5, PB10, PB11, PB12, PB13, PB14, PB15 */
    static const uint16_t unused_b[] = {GPIO_PIN_0, GPIO_PIN_3, GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_11,
                                         GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15};
    for (size_t i = 0; i < sizeof(unused_b) / sizeof(unused_b[0]); i++) {
        TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOB, unused_b[i]));
        TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(GPIOB, unused_b[i]));
    }

    /* 3. Inspect Port C unrouted pins & Button: PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13 */
    static const uint16_t unused_c[] = {GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9,
                                         GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13};
    for (size_t i = 0; i < sizeof(unused_c) / sizeof(unused_c[0]); i++) {
        TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(GPIOC, unused_c[i]));
        TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,      board_test_get_pin_pull(GPIOC, unused_c[i]));
    }
}

/**
 * @brief TC-S3-T4.1-05: Rain Gauge EXTI0 Exemption.
 */
static void test_power_mgr_rain_gauge_exti_exemption(void) {
    /* Perform pre-sleep GPIO conditioning */
    status_t status = power_mgr_gpio_sleep_prepare();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify PA0 remains in EXTI falling edge mode with No-Pull for wake capture */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_IT_FALLING, board_test_get_pin_mode(PIN_RAIN_GAUGE_PORT, PIN_RAIN_GAUGE_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_NOPULL,          board_test_get_pin_pull(PIN_RAIN_GAUGE_PORT, PIN_RAIN_GAUGE_PIN));
}

/**
 * @brief TC-S3-T4.1-06: Essential Hardware Output & Gate Exemptions.
 */
static void test_power_mgr_gate_and_actuator_exemptions(void) {
    /* Turn on multiple peripherals and actuators before sleep */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, true);
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, true);
    bsp_led_set(BSP_LED_GREEN, true);
    bsp_led_set(BSP_LED_RED, true);
    bsp_buzzer_set(true);
    bsp_relay_set(true);
    board_rf_switch_set(RF_SWITCH_TX_HP);

    /* Verify active states */
    TEST_ASSERT_TRUE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_TRUE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_TRUE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_TRUE(bsp_buzzer_get());
    TEST_ASSERT_TRUE(bsp_relay_get());
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_TX_HP, board_test_get_rf_mode());

    /* Execute pre-sleep conditioning */
    status_t status = power_mgr_gpio_sleep_prepare();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify power rail gates are safely de-energized */
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_SENSORS));
    TEST_ASSERT_FALSE(bsp_power_rail_is_enabled(BSP_POWER_RAIL_VBAT_SENSE));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET,   board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));

    /* Verify actuators and LEDs are dark and silent */
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_GREEN));
    TEST_ASSERT_FALSE(bsp_led_get(BSP_LED_RED));
    TEST_ASSERT_FALSE(bsp_buzzer_get());
    TEST_ASSERT_FALSE(bsp_relay_get());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN));

    /* Verify RF switch lines are in shutdown mode */
    TEST_ASSERT_EQUAL_INT(RF_SWITCH_SHUTDOWN, board_test_get_rf_mode());
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL2_PORT, PIN_FE_CTRL2_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_FE_CTRL3_PORT, PIN_FE_CTRL3_PIN));
}

/**
 * @brief TC-S3-T4.1-07: Post-Wake GPIO Restoration.
 */
static void test_power_mgr_wake_restore_gpio(void) {
    /* 1. Condition for sleep */
    (void)power_mgr_gpio_sleep_prepare();
    TEST_ASSERT_EQUAL_INT(POWER_STATE_STOP2, power_mgr_test_get_state());
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));

    /* 2. Execute post-wake restoration */
    status_t status = power_mgr_gpio_wake_restore();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_STATE_RUN, power_mgr_test_get_state());

    /* 3. Verify user button restored to Input Pullup */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_INPUT, board_test_get_pin_mode(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_PULLUP,     board_test_get_pin_pull(PIN_USER_BTN_PORT, PIN_USER_BTN_PIN));

    /* 4. Verify I2C1, RS-485, SDI-12, and SPI1 bus pins restored to active Alternate Function modes */
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

    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(GPIOA, GPIO_PIN_5));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF5_SPI1,   board_test_get_pin_af(GPIOA, GPIO_PIN_5));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(GPIOA, GPIO_PIN_6));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF5_SPI1,   board_test_get_pin_af(GPIOA, GPIO_PIN_6));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_AF_PP, board_test_get_pin_mode(GPIOA, GPIO_PIN_7));
    TEST_ASSERT_EQUAL_HEX32(GPIO_AF5_SPI1,   board_test_get_pin_af(GPIOA, GPIO_PIN_7));
}

/**
 * @brief TC-S3-T4.1-08: Leakage State Diagnostic Check.
 */
static void test_power_mgr_verify_leakage_state(void) {
    /* 1. Condition for sleep -> verify diagnostic passes */
    (void)power_mgr_gpio_sleep_prepare();
    status_t status = power_mgr_verify_leakage_state();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* 2. Injected Fault: PA4 HIGH (switched sensor rail left ON) */
    board_test_set_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 3. Injected Fault: PB1 LOW (battery divider left ON) */
    board_test_set_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 4. Injected Fault: Buzzer left active */
    board_test_set_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_state(PIN_BUZZER_PORT, PIN_BUZZER_PIN, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 5. Injected Fault: Relay left energized */
    board_test_set_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_state(PIN_RELAY_PORT, PIN_RELAY_PIN, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 6. Injected Fault: LED OK left illuminated */
    board_test_set_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_state(PIN_LED_OK_PORT, PIN_LED_OK_PIN, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 7. Injected Fault: LED WARN left illuminated */
    board_test_set_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_state(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 8. Injected Fault: RF Switch not in shutdown */
    board_rf_switch_set(RF_SWITCH_TX_HP);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_rf_switch_set(RF_SWITCH_SHUTDOWN);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 9. Injected Fault: I2C1 SCL pin left in AF mode instead of Analog */
    board_test_set_pin_config(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN, GPIO_MODE_AF_OD, GPIO_NOPULL, PIN_I2C1_SCL_AF);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_config(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 10. Injected Fault: RS-485 TX pin left in AF mode instead of Analog */
    board_test_set_pin_config(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN, GPIO_MODE_AF_PP, GPIO_NOPULL, PIN_RS485_TX_AF);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_config(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());

    /* 11. Injected Fault: Unused pin PA8 left floating / not analog */
    board_test_set_pin_config(GPIOA, GPIO_PIN_8, GPIO_MODE_INPUT, GPIO_NOPULL, 0);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_STATE, power_mgr_verify_leakage_state());
    board_test_set_pin_config(GPIOA, GPIO_PIN_8, GPIO_MODE_ANALOG, GPIO_NOPULL, 0);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, power_mgr_verify_leakage_state());
}

/**
 * @brief TC-S3-T4.2-01: RTC Wakeup Timer Configuration & Cancellation.
 */
static void test_power_mgr_rtc_wakeup_config(void) {
    /* 1. Arm RTC wakeup timer for 600s (normal sampling interval) */
    status_t status = power_mgr_set_rtc_wakeup(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(power_mgr_test_is_rtc_wakeup_armed());
    TEST_ASSERT_EQUAL_UINT32(600U, power_mgr_test_get_rtc_wakeup_interval());

    /* 2. Cancel RTC wakeup timer */
    status = power_mgr_cancel_rtc_wakeup();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(power_mgr_test_is_rtc_wakeup_armed());
    TEST_ASSERT_EQUAL_UINT32(0U, power_mgr_test_get_rtc_wakeup_interval());

    /* 3. Arm RTC wakeup timer with storm mode interval (120s) */
    status = power_mgr_set_rtc_wakeup(120U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(power_mgr_test_is_rtc_wakeup_armed());
    TEST_ASSERT_EQUAL_UINT32(120U, power_mgr_test_get_rtc_wakeup_interval());

    /* 4. Arm RTC wakeup timer with max allowed 16-bit interval (65535s) */
    status = power_mgr_set_rtc_wakeup(65535U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(power_mgr_test_is_rtc_wakeup_armed());
    TEST_ASSERT_EQUAL_UINT32(65535U, power_mgr_test_get_rtc_wakeup_interval());

    /* 5. Parameter Validation: interval = 0 must fail */
    status = power_mgr_set_rtc_wakeup(0U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);

    /* 6. Parameter Validation: interval > 65535 must fail */
    status = power_mgr_set_rtc_wakeup(65536U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);
}

/**
 * @brief TC-S3-T4.2-02: Stop 2 Mode Entry & Cumulative Sleep Metric Logging.
 */
static void test_power_mgr_stop2_cycle_and_metrics(void) {
    TEST_ASSERT_EQUAL_UINT32(0U, power_mgr_get_total_sleep_time_sec());
    TEST_ASSERT_EQUAL_UINT32(0U, power_mgr_test_get_sleep_cycle_count());

    /* 1. First Stop 2 sleep cycle (600s / 10 min normal) */
    status_t status = power_mgr_enter_stop2(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(600U, power_mgr_get_total_sleep_time_sec());
    TEST_ASSERT_EQUAL_UINT32(1U, power_mgr_test_get_sleep_cycle_count());
    TEST_ASSERT_EQUAL_INT(POWER_STATE_RUN, power_mgr_test_get_state());

    /* 2. Second Stop 2 sleep cycle (300s / 5 min watch) */
    status = power_mgr_enter_stop2(300U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(900U, power_mgr_get_total_sleep_time_sec());
    TEST_ASSERT_EQUAL_UINT32(2U, power_mgr_test_get_sleep_cycle_count());

    /* 3. Third Stop 2 sleep cycle (120s / 2 min storm alert) */
    status = power_mgr_enter_stop2(120U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1020U, power_mgr_get_total_sleep_time_sec());
    TEST_ASSERT_EQUAL_UINT32(3U, power_mgr_test_get_sleep_cycle_count());

    /* 4. Reset cumulative sleep metrics */
    power_mgr_reset_total_sleep_time();
    TEST_ASSERT_EQUAL_UINT32(0U, power_mgr_get_total_sleep_time_sec());

    /* 5. Parameter Validation: duration = 0 must fail */
    status = power_mgr_enter_stop2(0U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);

    /* 6. Parameter Validation: duration > 65535 must fail */
    status = power_mgr_enter_stop2(70000U);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, status);
}

/**
 * @brief TC-S3-T4.2-03: Multi-Source Wakeup Reason Detection.
 */
static void test_power_mgr_multi_source_wake_reasons(void) {
    /* 1. RTC Wakeup Event */
    power_mgr_test_inject_wake_event(POWER_WAKE_REASON_RTC);
    status_t status = power_mgr_enter_stop2(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_REASON_RTC, power_mgr_get_wake_reason());

    /* 2. Asynchronous Rain Gauge Pulse on PA0 (EXTI0) Wakeup Event */
    power_mgr_test_inject_wake_event(POWER_WAKE_REASON_RAIN_EXTI);
    status = power_mgr_enter_stop2(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_REASON_RAIN_EXTI, power_mgr_get_wake_reason());

    /* 3. Diagnostic User Push-Button on PC13 (EXTI13) Wakeup Event */
    power_mgr_test_inject_wake_event(POWER_WAKE_REASON_BUTTON);
    status = power_mgr_enter_stop2(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_REASON_BUTTON, power_mgr_get_wake_reason());

    /* 4. Unknown / Reset Wakeup Reason */
    power_mgr_test_inject_wake_event(POWER_WAKE_REASON_UNKNOWN);
    status = power_mgr_enter_stop2(600U);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_REASON_UNKNOWN, power_mgr_get_wake_reason());
}

/**
 * @brief TC-S3-T4.2-04: Post-Wake Clock Restoration & System Clocks.
 */
static void test_power_mgr_clock_restoration(void) {
    /* 1. Pre-sleep clock configuration */
    (void)system_clock_sleep_prepare();
    TEST_ASSERT_EQUAL_UINT32(4000000UL, system_clock_get_sysclk());
    TEST_ASSERT_EQUAL_UINT32(0U, system_clock_test_get_flash_latency());

    /* 2. Execute full wake restore */
    status_t status = power_mgr_wake_restore();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* 3. Verify clocks restored to 48 MHz MSI with 2 Flash wait states */
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_sysclk());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_hclk());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_pclk1());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_pclk2());
    TEST_ASSERT_EQUAL_UINT32(2U, system_clock_test_get_flash_latency());
    TEST_ASSERT_TRUE(system_clock_test_is_msi_pll_enabled());
}

/**
 * @brief TC-S3-T4.2-05: Shelf-Storage Standby Mode Entry.
 */
static void test_power_mgr_standby_mode(void) {
    /* Enter Standby mode */
    status_t status = power_mgr_enter_standby();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(POWER_STATE_STANDBY, power_mgr_test_get_state());

    /* Verify all pins are sleep-conditioned */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_I2C1_SCL_PORT, PIN_I2C1_SCL_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_RS485_TX_PORT, PIN_RS485_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_ANALOG, board_test_get_pin_mode(PIN_SDI12_TX_PORT, PIN_SDI12_TX_PIN));
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUTPUT_PP, board_test_get_pin_mode(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, board_test_get_pin_state(PIN_PWR_SENS_PORT, PIN_PWR_SENS_PIN));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, board_test_get_pin_state(PIN_VBAT_DIV_EN_PORT, PIN_VBAT_DIV_EN_PIN));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_power_mgr_init_and_states);
    RUN_TEST(test_power_mgr_isolate_sensor_buses);
    RUN_TEST(test_power_mgr_parasitic_leakage_prevention);
    RUN_TEST(test_power_mgr_unused_pin_conditioning);
    RUN_TEST(test_power_mgr_rain_gauge_exti_exemption);
    RUN_TEST(test_power_mgr_gate_and_actuator_exemptions);
    RUN_TEST(test_power_mgr_wake_restore_gpio);
    RUN_TEST(test_power_mgr_verify_leakage_state);
    RUN_TEST(test_power_mgr_rtc_wakeup_config);
    RUN_TEST(test_power_mgr_stop2_cycle_and_metrics);
    RUN_TEST(test_power_mgr_multi_source_wake_reasons);
    RUN_TEST(test_power_mgr_clock_restoration);
    RUN_TEST(test_power_mgr_standby_mode);
    return UNITY_END();
}
