/**
 * @file    board_config.h
 * @brief   Target board pin mapping and hardware configuration for STM32WLE5 SoC.
 * @details Establishes central hardware pin assignments, electrical modes, power gating,
 *          and low-power Stop 2 conditioning for the Tea Plantation Rain Prediction System.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#if defined(STM32WLE5xx) || defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"
#else
/* Standard port/pin types for host unit testing & simulation */
typedef void* GPIO_TypeDef;
#define GPIOA ((GPIO_TypeDef)0x48000000U)
#define GPIOB ((GPIO_TypeDef)0x48000400U)
#define GPIOC ((GPIO_TypeDef)0x48000800U)

#define GPIO_PIN_0   ((uint16_t)0x0001U)
#define GPIO_PIN_1   ((uint16_t)0x0002U)
#define GPIO_PIN_2   ((uint16_t)0x0004U)
#define GPIO_PIN_3   ((uint16_t)0x0008U)
#define GPIO_PIN_4   ((uint16_t)0x0010U)
#define GPIO_PIN_5   ((uint16_t)0x0020U)
#define GPIO_PIN_6   ((uint16_t)0x0040U)
#define GPIO_PIN_7   ((uint16_t)0x0080U)
#define GPIO_PIN_8   ((uint16_t)0x0100U)
#define GPIO_PIN_9   ((uint16_t)0x0200U)
#define GPIO_PIN_10  ((uint16_t)0x0400U)
#define GPIO_PIN_11  ((uint16_t)0x0800U)
#define GPIO_PIN_12  ((uint16_t)0x1000U)
#define GPIO_PIN_13  ((uint16_t)0x2000U)
#define GPIO_PIN_14  ((uint16_t)0x4000U)
#define GPIO_PIN_15  ((uint16_t)0x8000U)
#define GPIO_PIN_All ((uint16_t)0xFFFFU)

typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET   = 1
} GPIO_PinState;

#define GPIO_MODE_INPUT                 0x00000000U
#define GPIO_MODE_OUTPUT_PP             0x00000001U
#define GPIO_MODE_OUTPUT_OD             0x00000011U
#define GPIO_MODE_AF_PP                 0x00000002U
#define GPIO_MODE_AF_OD                 0x00000012U
#define GPIO_MODE_ANALOG                0x00000003U
#define GPIO_MODE_IT_RISING             0x10110000U
#define GPIO_MODE_IT_FALLING            0x10210000U
#define GPIO_MODE_IT_RISING_FALLING     0x10310000U

#define GPIO_NOPULL                     0x00000000U
#define GPIO_PULLUP                     0x00000001U
#define GPIO_PULLDOWN                   0x00000002U

#define GPIO_SPEED_FREQ_LOW             0x00000000U
#define GPIO_SPEED_FREQ_MEDIUM          0x00000001U
#define GPIO_SPEED_FREQ_HIGH            0x00000002U
#define GPIO_SPEED_FREQ_VERY_HIGH       0x00000003U

#define GPIO_AF4_I2C1                   0x00000004U
#define GPIO_AF5_SPI1                   0x00000005U
#define GPIO_AF7_USART1                 0x00000007U
#define GPIO_AF8_LPUART1                0x00000008U

#define EXTI0_IRQn                      6
#endif

/* ============================================================================
 * Hardware Pin Definitions & Port Mappings (UFQFPN48 STM32WLE5)
 * ============================================================================ */

/* --- Switched Sensor Power Rail Gate --- */
#define PIN_PWR_SENS_PORT                   GPIOA
#define PIN_PWR_SENS_PIN                    GPIO_PIN_4
#define BOARD_POWER_RAIL_STABILIZATION_MS   20U

/* --- Rain Gauge EXTI Pulse Input --- */
#define PIN_RAIN_GAUGE_PORT                 GPIOA
#define PIN_RAIN_GAUGE_PIN                  GPIO_PIN_0
#define PIN_RAIN_GAUGE_EXTI_IRQn            EXTI0_IRQn

/* --- RS-485 Modbus RTU Bus (USART1) --- */
#define PIN_RS485_DIR_PORT                  GPIOA
#define PIN_RS485_DIR_PIN                   GPIO_PIN_1
#define PIN_RS485_TX_PORT                   GPIOA
#define PIN_RS485_TX_PIN                    GPIO_PIN_2
#define PIN_RS485_TX_AF                     GPIO_AF7_USART1
#define PIN_RS485_RX_PORT                   GPIOA
#define PIN_RS485_RX_PIN                    GPIO_PIN_3
#define PIN_RS485_RX_AF                     GPIO_AF7_USART1

/* --- I2C Sensor Bus (I2C1 - BME280 / OPT3001 / PCA9615) --- */
#define PIN_I2C1_SCL_PORT                   GPIOB
#define PIN_I2C1_SCL_PIN                    GPIO_PIN_6
#define PIN_I2C1_SCL_AF                     GPIO_AF4_I2C1
#define PIN_I2C1_SDA_PORT                   GPIOB
#define PIN_I2C1_SDA_PIN                    GPIO_PIN_7
#define PIN_I2C1_SDA_AF                     GPIO_AF4_I2C1

/* --- Battery Voltage Measurement (ADC_IN1) --- */
#define PIN_VBAT_MEAS_PORT                  GPIOB
#define PIN_VBAT_MEAS_PIN                   GPIO_PIN_0
#define PIN_VBAT_DIV_EN_PORT                GPIOB
#define PIN_VBAT_DIV_EN_PIN                 GPIO_PIN_1

/* --- Local Status Indicators & Alarms --- */
#define PIN_LED_OK_PORT                     GPIOB
#define PIN_LED_OK_PIN                      GPIO_PIN_8
#define PIN_LED_WARN_PORT                   GPIOB
#define PIN_LED_WARN_PIN                    GPIO_PIN_9
#define PIN_BUZZER_PORT                     GPIOB
#define PIN_BUZZER_PIN                      GPIO_PIN_2
#define PIN_RELAY_PORT                      GPIOB
#define PIN_RELAY_PIN                       GPIO_PIN_4

/* --- SDI-12 Auxiliary Bus (LPUART1) --- */
#define PIN_SDI12_TX_PORT                   GPIOC
#define PIN_SDI12_TX_PIN                    GPIO_PIN_0
#define PIN_SDI12_TX_AF                     GPIO_AF8_LPUART1
#define PIN_SDI12_RX_PORT                   GPIOC
#define PIN_SDI12_RX_PIN                    GPIO_PIN_1
#define PIN_SDI12_RX_AF                     GPIO_AF8_LPUART1
#define PIN_SDI12_DIR_PORT                  GPIOC
#define PIN_SDI12_DIR_PIN                   GPIO_PIN_2

/* --- Sub-GHz RF Switch Control --- */
#define PIN_FE_CTRL1_PORT                   GPIOC
#define PIN_FE_CTRL1_PIN                    GPIO_PIN_4
#define PIN_FE_CTRL2_PORT                   GPIOC
#define PIN_FE_CTRL2_PIN                    GPIO_PIN_5
#define PIN_FE_CTRL3_PORT                   GPIOC
#define PIN_FE_CTRL3_PIN                    GPIO_PIN_3

/* --- User Field Test Push-Button --- */
#define PIN_USER_BTN_PORT                   GPIOC
#define PIN_USER_BTN_PIN                    GPIO_PIN_13

/* ============================================================================
 * Enumerations & Type Definitions
 * ============================================================================ */

/**
 * @brief Board Status LED Identifiers.
 */
typedef enum {
    BOARD_LED_OK = 0,   /**< Green Heartbeat / Normal Status LED (PB8) */
    BOARD_LED_WARN      /**< Red Warning / Storm Alert LED (PB9) */
} board_led_t;

/**
 * @brief RS-485 Half-Duplex Direction Modes.
 */
typedef enum {
    BOARD_RS485_DIR_RX = 0, /**< Receiver Active / Transmitter Tri-Stated (PA1 LOW) */
    BOARD_RS485_DIR_TX = 1  /**< Transmitter Active (PA1 HIGH) */
} board_rs485_dir_t;

/**
 * @brief SDI-12 Direction Modes.
 */
typedef enum {
    BOARD_SDI12_DIR_RX = 0, /**< Listening / Tri-State (PC2 LOW) */
    BOARD_SDI12_DIR_TX = 1  /**< Driving Bus Line (PC2 HIGH) */
} board_sdi12_dir_t;

/**
 * @brief Sub-GHz RF Switch Operating Modes.
 */
typedef enum {
    RF_SWITCH_SHUTDOWN = 0, /**< All paths isolated for minimum current */
    RF_SWITCH_RX,           /**< Receiver path selected */
    RF_SWITCH_TX_LP,        /**< Low-Power Transmitter path selected (+14 dBm) */
    RF_SWITCH_TX_HP         /**< High-Power Transmitter path selected (+22 dBm) */
} board_rf_mode_t;

/* ============================================================================
 * Public Board Lifecycle & GPIO Function Prototypes
 * ============================================================================ */

/**
 * @brief  Initializes all target MCU GPIO ports, default output states, and EXTI interrupts.
 * @return status_t STATUS_OK on success, error code otherwise.
 */
status_t board_gpio_init(void);

/**
 * @brief  Prepares all GPIO pins for ultra-low-leakage Stop 2 deep sleep mode (< 3.0 uA).
 * @return status_t STATUS_OK on success.
 */
status_t board_gpio_sleep_prepare(void);

/**
 * @brief  Restores operational GPIO pin modes and peripheral multiplexing upon wake from Stop 2.
 * @return status_t STATUS_OK on success.
 */
status_t board_gpio_wake_restore(void);

/**
 * @brief  Enables or disables switched sensor power rail (VSENS_SW on PA4).
 * @param[in] enable true to energize rail with 20ms stabilization; false to de-energize.
 * @return status_t STATUS_OK on success.
 */
status_t board_sensor_power_enable(bool enable);

/**
 * @brief  Enables or disables the battery voltage measurement divider (PB1).
 * @param[in] enable true to connect divider (PB1 LOW); false to disconnect (PB1 HIGH).
 * @return status_t STATUS_OK on success.
 */
status_t board_vbat_divider_enable(bool enable);

/**
 * @brief  Sets status LED state.
 * @param[in] led   LED identifier (BOARD_LED_OK or BOARD_LED_WARN).
 * @param[in] state true for ON (illuminated), false for OFF.
 */
void board_led_set(board_led_t led, bool state);

/**
 * @brief  Toggles status LED state.
 * @param[in] led LED identifier.
 */
void board_led_toggle(board_led_t led);

/**
 * @brief  Drives local alarm buzzer output gate (PB2).
 * @param[in] state true to sound buzzer, false to silence.
 */
void board_buzzer_set(bool state);

/**
 * @brief  Drives local alert relay optocoupler gate (PB4).
 * @param[in] state true to energize relay, false to de-energize.
 */
void board_relay_set(bool state);

/**
 * @brief  Sets RS-485 transceiver half-duplex direction (PA1).
 * @param[in] dir Direction mode (BOARD_RS485_DIR_RX or BOARD_RS485_DIR_TX).
 */
void board_rs485_dir_set(board_rs485_dir_t dir);

/**
 * @brief  Sets SDI-12 bus transceiver direction (PC2).
 * @param[in] dir Direction mode (BOARD_SDI12_DIR_RX or BOARD_SDI12_DIR_TX).
 */
void board_sdi12_dir_set(board_sdi12_dir_t dir);

/**
 * @brief  Sets Sub-GHz RF antenna switch configuration lines (PC3/PC4/PC5).
 * @param[in] mode Desired RF mode.
 */
void board_rf_switch_set(board_rf_mode_t mode);

/**
 * @brief  Reads momentary status of user field test push-button (PC13).
 * @return bool true if button is currently pressed (active LOW).
 */
bool board_button_is_pressed(void);

#if !defined(STM32WLE5xx) && !defined(USE_HAL_DRIVER)
/* ============================================================================
 * Host Simulation & Test Inspection API
 * ============================================================================ */

/**
 * @brief Resets simulated board hardware state for unit test isolation.
 */
void board_test_reset(void);

/**
 * @brief Sets simulated user button state on host.
 * @param[in] pressed true for pressed (LOW), false for released (HIGH).
 */
void board_test_set_button_pressed(bool pressed);

/**
 * @brief Gets simulated pin logic level.
 * @param[in] port Port identifier (GPIOA, GPIOB, GPIOC).
 * @param[in] pin  Pin mask.
 * @return GPIO_PIN_SET or GPIO_PIN_RESET.
 */
GPIO_PinState board_test_get_pin_state(GPIO_TypeDef port, uint16_t pin);

/**
 * @brief Gets simulated pin configured mode.
 * @param[in] port Port identifier (GPIOA, GPIOB, GPIOC).
 * @param[in] pin  Pin mask.
 * @return Pin mode (GPIO_MODE_ANALOG, GPIO_MODE_OUTPUT_PP, etc.).
 */
uint32_t board_test_get_pin_mode(GPIO_TypeDef port, uint16_t pin);

/**
 * @brief Gets simulated pin pull configuration.
 * @param[in] port Port identifier (GPIOA, GPIOB, GPIOC).
 * @param[in] pin  Pin mask.
 * @return Pull mode (GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN).
 */
uint32_t board_test_get_pin_pull(GPIO_TypeDef port, uint16_t pin);

/**
 * @brief Gets simulated pin alternate function.
 * @param[in] port Port identifier (GPIOA, GPIOB, GPIOC).
 * @param[in] pin  Pin mask.
 * @return Alternate function ID.
 */
uint32_t board_test_get_pin_af(GPIO_TypeDef port, uint16_t pin);

/**
 * @brief Checks if sensor power rail is energized.
 * @return true if rail is ON.
 */
bool board_test_is_sensor_power_enabled(void);

/**
 * @brief Checks if battery divider is enabled.
 * @return true if divider is connected.
 */
bool board_test_is_vbat_divider_enabled(void);

/**
 * @brief Gets current RF switch operating mode.
 * @return board_rf_mode_t current mode.
 */
board_rf_mode_t board_test_get_rf_mode(void);

/**
 * @brief Gets number of times stabilization delay was invoked.
 * @return uint32_t invocation count.
 */
uint32_t board_test_get_delay_call_count(void);

/**
 * @brief Gets last delay duration passed in milliseconds.
 * @return uint32_t duration in ms.
 */
uint32_t board_test_get_last_delay_ms(void);

#endif /* Host Simulation API */

#ifdef __cplusplus
}
#endif

#endif /* BOARD_CONFIG_H */
