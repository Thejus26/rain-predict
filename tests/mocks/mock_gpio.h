/**
 * @file    mock_gpio.h
 * @brief   Host Mock GPIO & EXTI External Interrupt Controller.
 * @details Simulates pin states, rain pulse interrupts, and power rail gates.
 */

#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gpio_driver.h"

#define MOCK_GPIO_MAX_PINS      16

/* --- Mock Control API for Unit Tests --- */

/**
 * @brief Initializes virtual port states and clears EXTI table.
 */
void mock_gpio_init(void);

/**
 * @brief Resets all pin states, edge counters, and active callbacks.
 *        Restores default idle state: PB2 HIGH (power off), PB0 HIGH (pull-up).
 */
void mock_gpio_reset(void);

/**
 * @brief Sets simulated input voltage on an external pin.
 *
 * @param[in] port  Target GPIO port.
 * @param[in] pin   Target pin number (0-15).
 * @param[in] state Input logic level.
 */
void mock_gpio_set_input(gpio_port_t port, uint16_t pin, gpio_pin_state_t state);

/**
 * @brief Triggers the registered EXTI interrupt callback for the specified pin.
 *
 * @param[in] pin EXTI line / pin number (0-15).
 */
void mock_gpio_trigger_exti(uint16_t pin);

/**
 * @brief Injects N pulses separated by period_ms to simulate continuous rain.
 *
 * @param[in] pin         EXTI line / pin number (0-15).
 * @param[in] pulse_count Number of pulse events to inject.
 * @param[in] period_ms   Pulse period in milliseconds (simulation timestamp).
 */
void mock_gpio_inject_pulse_train(uint16_t pin, uint32_t pulse_count, uint32_t period_ms);

/**
 * @brief Simulates mechanical switch contact bounce glitches (<50ms).
 *
 * @param[in] pin                EXTI line / pin number (0-15).
 * @param[in] bounce_duration_ms Duration of contact chatter in milliseconds.
 */
void mock_gpio_inject_contact_bounce(uint16_t pin, uint32_t bounce_duration_ms);

/**
 * @brief Returns total times a pin has toggled state.
 *
 * @param[in] port Target GPIO port.
 * @param[in] pin  Target pin number (0-15).
 * @return Cumulative toggle count.
 */
uint32_t mock_gpio_get_toggle_count(gpio_port_t port, uint16_t pin);

/**
 * @brief Returns total rising edge transitions (LOW -> HIGH) on the pin.
 *
 * @param[in] port Target GPIO port.
 * @param[in] pin  Target pin number (0-15).
 * @return Cumulative rising edge count.
 */
uint32_t mock_gpio_get_rising_edge_count(gpio_port_t port, uint16_t pin);

/**
 * @brief Returns total falling edge transitions (HIGH -> LOW) on the pin.
 *
 * @param[in] port Target GPIO port.
 * @param[in] pin  Target pin number (0-15).
 * @return Cumulative falling edge count.
 */
uint32_t mock_gpio_get_falling_edge_count(gpio_port_t port, uint16_t pin);

/**
 * @brief Checks if high-side sensor power gate (PB2) is driven active low (0 = ON / Energized).
 *
 * @return true if power rail is energized (PB2 LOW), false if de-energized (PB2 HIGH).
 */
bool mock_gpio_is_power_rail_energized(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_GPIO_H */
