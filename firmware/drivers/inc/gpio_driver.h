/**
 * @file    gpio_driver.h
 * @brief   Hardware-independent GPIO and EXTI Master Driver Interface.
 * @details Declares pin control operations, interrupt registration, and board pin mappings.
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#define GPIO_MAX_PINS_PER_PORT  16

/**
 * @brief GPIO pin logic levels.
 */
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET   = 1
} gpio_pin_state_t;

/**
 * @brief GPIO hardware port identifiers.
 */
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B = 1,
    GPIO_PORT_C = 2,
    GPIO_PORT_MAX
} gpio_port_t;

/**
 * @brief GPIO pin operating modes (HAL-compatible).
 */
typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT_PP,
    GPIO_MODE_OUTPUT_OD,
    GPIO_MODE_AF_PP,
    GPIO_MODE_AF_OD,
    GPIO_MODE_ANALOG,
    GPIO_MODE_IT_RISING,
    GPIO_MODE_IT_FALLING,
    GPIO_MODE_IT_RISING_FALLING
} gpio_mode_t;

/* Dedicated Hardware Pin Aliases from schematics-and-pinout.md */
#define PIN_RAIN_GAUGE_EXTI     0  /**< Port B Pin 0: Tipping-Bucket EXTI */
#define PIN_SENSOR_PWR_GATE     2  /**< Port B Pin 2: High-Side Sensor Power Gate (Active Low) */
#define PIN_LED_GREEN           5  /**< Port B Pin 5: Status OK LED */
#define PIN_LED_RED             6  /**< Port B Pin 6: Alert Storm LED */
#define PIN_BUZZER_RELAY        7  /**< Port B Pin 7: Piezo Buzzer & Estate Relay */
#define PIN_RS485_DE_RE         4  /**< Port A Pin 4: RS-485 Direction Enable */

/**
 * @brief EXTI interrupt line callback signature.
 * @param[in] pin GPIO pin number triggering the interrupt (0-15).
 */
typedef void (*gpio_exti_callback_t)(uint16_t pin);

/**
 * @brief Initializes a GPIO pin with specified mode configuration.
 *
 * @param[in] port GPIO port identifier.
 * @param[in] pin  Pin number (0-15).
 * @param[in] mode Operating mode.
 */
void gpio_init_pin(gpio_port_t port, uint16_t pin, uint32_t mode);

/**
 * @brief Sets the output state of a GPIO pin.
 *
 * @param[in] port  GPIO port identifier.
 * @param[in] pin   Pin number (0-15).
 * @param[in] state Logic state to write (GPIO_PIN_RESET or GPIO_PIN_SET).
 */
void gpio_write_pin(gpio_port_t port, uint16_t pin, gpio_pin_state_t state);

/**
 * @brief Reads the current logic state of a GPIO pin.
 *
 * @param[in] port GPIO port identifier.
 * @param[in] pin  Pin number (0-15).
 * @return GPIO_PIN_SET or GPIO_PIN_RESET. Returns GPIO_PIN_RESET if parameters invalid.
 */
gpio_pin_state_t gpio_read_pin(gpio_port_t port, uint16_t pin);

/**
 * @brief Toggles the output state of a GPIO pin.
 *
 * @param[in] port GPIO port identifier.
 * @param[in] pin  Pin number (0-15).
 */
void gpio_toggle_pin(gpio_port_t port, uint16_t pin);

/**
 * @brief Registers an EXTI interrupt callback for a specific pin line.
 *
 * @param[in] pin      EXTI line / pin number (0-15).
 * @param[in] callback Function pointer to ISR callback handler.
 * @return STATUS_OK on success, STATUS_ERR_INVALID_PARAM if pin >= 16.
 */
status_t gpio_register_exti_callback(uint16_t pin, gpio_exti_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_DRIVER_H */
