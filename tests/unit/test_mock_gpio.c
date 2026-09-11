/**
 * @file    test_mock_gpio.c
 * @brief   Unit test verification suite for Mock GPIO & EXTI Interrupt Simulator.
 * @details Validates pin I/O, edge counters, power rail monitoring, and EXTI pulse injection.
 */

#include "unity.h"
#include "mock_gpio.h"

static volatile uint32_t s_exti_call_count = 0;
static volatile uint16_t s_last_exti_pin = 0xFFFF;

static void test_exti_isr(uint16_t pin) {
    s_exti_call_count++;
    s_last_exti_pin = pin;
}

void setUp(void) {
    mock_gpio_reset();
    s_exti_call_count = 0;
    s_last_exti_pin = 0xFFFF;
}

void tearDown(void) {
    /* No dynamically allocated resources to release */
}

/**
 * @brief TC-S1-T2.4-01: Verify basic GPIO pin write and read operations.
 */
static void test_mock_gpio_write_and_read(void) {
    /* Initial state after reset for Port B LED */
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_B, PIN_LED_RED));

    /* Write logic SET */
    gpio_write_pin(GPIO_PORT_B, PIN_LED_RED, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, gpio_read_pin(GPIO_PORT_B, PIN_LED_RED));

    /* Write logic RESET */
    gpio_write_pin(GPIO_PORT_B, PIN_LED_RED, GPIO_PIN_RESET);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_B, PIN_LED_RED));

    /* Test mock input helper */
    mock_gpio_set_input(GPIO_PORT_A, PIN_RS485_DE_RE, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, gpio_read_pin(GPIO_PORT_A, PIN_RS485_DE_RE));
}

/**
 * @brief TC-S1-T2.4-02: Verify toggle behavior and edge transition accounting.
 */
static void test_mock_gpio_toggle_and_edge_accounting(void) {
    /* Initially 0 toggles / edges */
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_toggle_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_rising_edge_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_falling_edge_count(GPIO_PORT_B, PIN_LED_GREEN));

    /* Toggle 1: 0 -> 1 (Rising) */
    gpio_toggle_pin(GPIO_PORT_B, PIN_LED_GREEN);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, gpio_read_pin(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(1, mock_gpio_get_toggle_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(1, mock_gpio_get_rising_edge_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_falling_edge_count(GPIO_PORT_B, PIN_LED_GREEN));

    /* Toggle 2: 1 -> 0 (Falling) */
    gpio_toggle_pin(GPIO_PORT_B, PIN_LED_GREEN);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(2, mock_gpio_get_toggle_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(1, mock_gpio_get_rising_edge_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(1, mock_gpio_get_falling_edge_count(GPIO_PORT_B, PIN_LED_GREEN));

    /* Toggle 3 & 4 */
    gpio_toggle_pin(GPIO_PORT_B, PIN_LED_GREEN);
    gpio_toggle_pin(GPIO_PORT_B, PIN_LED_GREEN);
    TEST_ASSERT_EQUAL_UINT32(4, mock_gpio_get_toggle_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(2, mock_gpio_get_rising_edge_count(GPIO_PORT_B, PIN_LED_GREEN));
    TEST_ASSERT_EQUAL_UINT32(2, mock_gpio_get_falling_edge_count(GPIO_PORT_B, PIN_LED_GREEN));
}

/**
 * @brief TC-S1-T2.4-03: Verify EXTI callback registration and interrupt dispatch.
 */
static void test_mock_gpio_exti_dispatch(void) {
    status_t status = gpio_register_exti_callback(PIN_RAIN_GAUGE_EXTI, test_exti_isr);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Trigger interrupt */
    mock_gpio_trigger_exti(PIN_RAIN_GAUGE_EXTI);
    TEST_ASSERT_EQUAL_UINT32(1, s_exti_call_count);
    TEST_ASSERT_EQUAL_UINT16(PIN_RAIN_GAUGE_EXTI, s_last_exti_pin);

    /* Trigger again */
    mock_gpio_trigger_exti(PIN_RAIN_GAUGE_EXTI);
    TEST_ASSERT_EQUAL_UINT32(2, s_exti_call_count);
}

/**
 * @brief TC-S1-T2.4-04: Verify continuous rain pulse train injection fidelity.
 */
static void test_mock_gpio_pulse_train_injection(void) {
    status_t status = gpio_register_exti_callback(PIN_RAIN_GAUGE_EXTI, test_exti_isr);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Inject 50 rain tips (representing 10.0 mm of rainfall at 0.2mm/tip) */
    mock_gpio_inject_pulse_train(PIN_RAIN_GAUGE_EXTI, 50, 100);
    TEST_ASSERT_EQUAL_UINT32(50, s_exti_call_count);
    TEST_ASSERT_EQUAL_UINT16(PIN_RAIN_GAUGE_EXTI, s_last_exti_pin);
}

/**
 * @brief Verify mechanical contact bounce chatter injection.
 */
static void test_mock_gpio_contact_bounce_injection(void) {
    status_t status = gpio_register_exti_callback(PIN_RAIN_GAUGE_EXTI, test_exti_isr);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Inject contact bounce */
    mock_gpio_inject_contact_bounce(PIN_RAIN_GAUGE_EXTI, 25);
    TEST_ASSERT_EQUAL_UINT32(3, s_exti_call_count);
}

/**
 * @brief TC-S1-T2.4-05: Verify high-side sensor power gate monitor helper logic.
 */
static void test_mock_gpio_power_rail_state(void) {
    /* Default state after reset is PB2 HIGH -> Power Rail is OFF / De-energized */
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, gpio_read_pin(GPIO_PORT_B, PIN_SENSOR_PWR_GATE));
    TEST_ASSERT_FALSE(mock_gpio_is_power_rail_energized());

    /* Drive PB2 LOW -> High-Side P-MOSFET conducts, rail is energized */
    gpio_write_pin(GPIO_PORT_B, PIN_SENSOR_PWR_GATE, GPIO_PIN_RESET);
    TEST_ASSERT_TRUE(mock_gpio_is_power_rail_energized());

    /* Drive PB2 HIGH -> Rail disabled */
    gpio_write_pin(GPIO_PORT_B, PIN_SENSOR_PWR_GATE, GPIO_PIN_SET);
    TEST_ASSERT_FALSE(mock_gpio_is_power_rail_energized());
}

/**
 * @brief TC-S1-T2.4-06: Verify test reset isolation.
 */
static void test_mock_gpio_reset_isolation(void) {
    /* Mutate state across multiple ports and register callback */
    gpio_write_pin(GPIO_PORT_A, 0, GPIO_PIN_SET);
    gpio_write_pin(GPIO_PORT_B, PIN_SENSOR_PWR_GATE, GPIO_PIN_RESET);
    gpio_write_pin(GPIO_PORT_C, 13, GPIO_PIN_SET);
    gpio_register_exti_callback(PIN_RAIN_GAUGE_EXTI, test_exti_isr);

    /* Call reset */
    mock_gpio_reset();

    /* Check reset values */
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_A, 0));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, gpio_read_pin(GPIO_PORT_B, PIN_SENSOR_PWR_GATE));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, gpio_read_pin(GPIO_PORT_B, PIN_RAIN_GAUGE_EXTI));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_C, 13));
    TEST_ASSERT_FALSE(mock_gpio_is_power_rail_energized());

    /* Verify callbacks cleared */
    mock_gpio_trigger_exti(PIN_RAIN_GAUGE_EXTI);
    TEST_ASSERT_EQUAL_UINT32(0, s_exti_call_count);
}

/**
 * @brief Verify boundary and out-of-bounds argument safety.
 */
static void test_mock_gpio_boundary_and_error_handling(void) {
    /* Invalid Port */
    gpio_init_pin(GPIO_PORT_MAX, 0, GPIO_MODE_OUTPUT_PP);
    gpio_write_pin(GPIO_PORT_MAX, 0, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_MAX, 0));
    gpio_toggle_pin(GPIO_PORT_MAX, 0);
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_toggle_count(GPIO_PORT_MAX, 0));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_rising_edge_count(GPIO_PORT_MAX, 0));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_falling_edge_count(GPIO_PORT_MAX, 0));

    /* Invalid Pin */
    gpio_init_pin(GPIO_PORT_A, MOCK_GPIO_MAX_PINS, GPIO_MODE_OUTPUT_PP);
    gpio_write_pin(GPIO_PORT_A, MOCK_GPIO_MAX_PINS, GPIO_PIN_SET);
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_RESET, gpio_read_pin(GPIO_PORT_A, MOCK_GPIO_MAX_PINS));
    gpio_toggle_pin(GPIO_PORT_A, MOCK_GPIO_MAX_PINS);
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_toggle_count(GPIO_PORT_A, MOCK_GPIO_MAX_PINS));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_rising_edge_count(GPIO_PORT_A, MOCK_GPIO_MAX_PINS));
    TEST_ASSERT_EQUAL_UINT32(0, mock_gpio_get_falling_edge_count(GPIO_PORT_A, MOCK_GPIO_MAX_PINS));

    /* Invalid EXTI Pin Callback Registration */
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_INVALID_PARAM, gpio_register_exti_callback(MOCK_GPIO_MAX_PINS, test_exti_isr));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mock_gpio_write_and_read);
    RUN_TEST(test_mock_gpio_toggle_and_edge_accounting);
    RUN_TEST(test_mock_gpio_exti_dispatch);
    RUN_TEST(test_mock_gpio_pulse_train_injection);
    RUN_TEST(test_mock_gpio_contact_bounce_injection);
    RUN_TEST(test_mock_gpio_power_rail_state);
    RUN_TEST(test_mock_gpio_reset_isolation);
    RUN_TEST(test_mock_gpio_boundary_and_error_handling);
    return UNITY_END();
}
