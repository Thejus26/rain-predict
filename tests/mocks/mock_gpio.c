/**
 * @file    mock_gpio.c
 * @brief   Implementation of Host Mock GPIO & EXTI Interrupt Simulator.
 * @details Emulates pin state machine, edge transitions, power rail gates, and EXTI dispatch.
 */

#include "mock_gpio.h"
#include <string.h>

/**
 * @brief Internal representation of a virtual GPIO pin.
 */
typedef struct {
    gpio_pin_state_t state;
    uint32_t toggle_count;
    uint32_t rising_edges;
    uint32_t falling_edges;
} mock_pin_t;

/**
 * @brief Internal representation of a virtual GPIO port.
 */
typedef struct {
    mock_pin_t pins[MOCK_GPIO_MAX_PINS];
} mock_port_t;

static mock_port_t s_ports[GPIO_PORT_MAX];
static gpio_exti_callback_t s_exti_callbacks[MOCK_GPIO_MAX_PINS];

void mock_gpio_init(void) {
    mock_gpio_reset();
}

void mock_gpio_reset(void) {
    (void)memset(s_ports, 0, sizeof(s_ports));
    (void)memset(s_exti_callbacks, 0, sizeof(s_exti_callbacks));

    /* Default high-side sensor power switch PB2 to HIGH (Off / De-energized) */
    s_ports[GPIO_PORT_B].pins[PIN_SENSOR_PWR_GATE].state = GPIO_PIN_SET;

    /* Default rain gauge input PB0 to HIGH (Pulled Up via internal/external pull-up) */
    s_ports[GPIO_PORT_B].pins[PIN_RAIN_GAUGE_EXTI].state = GPIO_PIN_SET;
}

void gpio_init_pin(gpio_port_t port, uint16_t pin, uint32_t mode) {
    (void)mode;
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return;
    }
}

void gpio_write_pin(gpio_port_t port, uint16_t pin, gpio_pin_state_t state) {
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return;
    }

    mock_pin_t *p_pin = &s_ports[port].pins[pin];

    if (p_pin->state != state) {
        p_pin->toggle_count++;
        if (state == GPIO_PIN_SET) {
            p_pin->rising_edges++;
        } else {
            p_pin->falling_edges++;
        }
        p_pin->state = state;
    }
}

gpio_pin_state_t gpio_read_pin(gpio_port_t port, uint16_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return GPIO_PIN_RESET;
    }
    return s_ports[port].pins[pin].state;
}

void gpio_toggle_pin(gpio_port_t port, uint16_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return;
    }
    gpio_pin_state_t current_state = s_ports[port].pins[pin].state;
    gpio_pin_state_t new_state = (current_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    gpio_write_pin(port, pin, new_state);
}

status_t gpio_register_exti_callback(uint16_t pin, gpio_exti_callback_t callback) {
    if (pin >= MOCK_GPIO_MAX_PINS) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_exti_callbacks[pin] = callback;
    return STATUS_OK;
}

/* --- Mock Control Implementations --- */

void mock_gpio_set_input(gpio_port_t port, uint16_t pin, gpio_pin_state_t state) {
    gpio_write_pin(port, pin, state);
}

void mock_gpio_trigger_exti(uint16_t pin) {
    if (pin < MOCK_GPIO_MAX_PINS && s_exti_callbacks[pin] != NULL) {
        s_exti_callbacks[pin](pin);
    }
}

void mock_gpio_inject_pulse_train(uint16_t pin, uint32_t pulse_count, uint32_t period_ms) {
    (void)period_ms;
    for (uint32_t i = 0; i < pulse_count; i++) {
        mock_gpio_trigger_exti(pin);
    }
}

void mock_gpio_inject_contact_bounce(uint16_t pin, uint32_t bounce_duration_ms) {
    (void)bounce_duration_ms;
    /* Rapidly trigger EXTI 3 times within a single tick window to simulate contact chatter */
    mock_gpio_trigger_exti(pin);
    mock_gpio_trigger_exti(pin);
    mock_gpio_trigger_exti(pin);
}

uint32_t mock_gpio_get_toggle_count(gpio_port_t port, uint16_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return 0;
    }
    return s_ports[port].pins[pin].toggle_count;
}

uint32_t mock_gpio_get_rising_edge_count(gpio_port_t port, uint16_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return 0;
    }
    return s_ports[port].pins[pin].rising_edges;
}

uint32_t mock_gpio_get_falling_edge_count(gpio_port_t port, uint16_t pin) {
    if (port >= GPIO_PORT_MAX || pin >= MOCK_GPIO_MAX_PINS) {
        return 0;
    }
    return s_ports[port].pins[pin].falling_edges;
}

bool mock_gpio_is_power_rail_energized(void) {
    /* PB2 low energizes high-side P-MOSFET switch */
    return (s_ports[GPIO_PORT_B].pins[PIN_SENSOR_PWR_GATE].state == GPIO_PIN_RESET);
}
