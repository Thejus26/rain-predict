/**
 * @file    stm32wlxx_hal_conf.h
 * @brief   STM32CubeWL HAL module configuration for Tea Plantation Rain Prediction Station.
 * @details Tailored compile-time filter enabling strictly required peripheral modules,
 *          accurate oscillator constants, cache parameters, and defensive assert hooks.
 */

#ifndef STM32WLXX_HAL_CONF_H
#define STM32WLXX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================================
 * 1. Module Selection (Enabled HAL Modules)
 * ============================================================================ */

#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_SUBGHZ_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* Disabled Modules: COMP, CRC, CRYP, DAC, GTZC, HSEM, IPCC, IRDA, LPTIM, RNG, SMARTCARD, SMBUS, TIM, WWDG */

/* ============================================================================
 * 2. Hardware Oscillator & Voltage Parameter Definitions
 * ============================================================================ */

#if !defined(HSE_VALUE)
#define HSE_VALUE               32000000UL  /*!< Frequency of the High-Speed External TCXO in Hz */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT     100U        /*!< Timeout for HSE start up, in ms */
#endif

#if !defined(MSI_VALUE)
#define MSI_VALUE               48000000UL  /*!< Frequency of the Multi-Speed Internal RC in Hz */
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE               16000000UL  /*!< Frequency of the High-Speed Internal RC in Hz */
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE               32768UL     /*!< Frequency of the Low-Speed External Quartz in Hz */
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT     5000U       /*!< Timeout for LSE start up, in ms */
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE               32000UL     /*!< Frequency of the Low-Speed Internal RC in Hz */
#endif

#if !defined(VDD_VALUE)
#define VDD_VALUE               3300UL      /*!< VDD value in mV */
#endif

#if !defined(TICK_INT_PRIORITY)
#define TICK_INT_PRIORITY       0x00U       /*!< SysTick interrupt priority (0 = Highest) */
#endif

#if !defined(USE_RTOS)
#define USE_RTOS                0U          /*!< Bare-metal deterministic execution */
#endif

#if !defined(PREFETCH_ENABLE)
#define PREFETCH_ENABLE         1U          /*!< Flash prefetch buffer enabled */
#endif

#if !defined(INSTRUCTION_CACHE_ENABLE)
#define INSTRUCTION_CACHE_ENABLE 1U         /*!< Instruction cache enabled */
#endif

#if !defined(DATA_CACHE_ENABLE)
#define DATA_CACHE_ENABLE       1U          /*!< Data cache enabled */
#endif

/* ============================================================================
 * 3. Defensive Parameter Assertion Configuration
 * ============================================================================ */

#if defined(DEBUG) && !defined(USE_FULL_ASSERT)
  #define USE_FULL_ASSERT       1U
#elif !defined(USE_FULL_ASSERT)
  #define USE_FULL_ASSERT       0U
#endif

#if (defined(USE_FULL_ASSERT) && (USE_FULL_ASSERT == 1U))
  /**
   * @brief  The assert_param macro is used for function parameter validation.
   * @param  expr If expr is false, calls assert_failed function with source file and line.
   */
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

/* ============================================================================
 * 4. Active HAL Module Header Inclusions
 * ============================================================================ */

#if defined(__has_include)
  #if __has_include("stm32wlxx_hal_rcc.h")
    #define HAVE_STM32WLXX_PERIPH_HEADERS 1
  #endif
#endif

#if defined(HAVE_STM32WLXX_PERIPH_HEADERS)

#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32wlxx_hal_rcc.h"
  #include "stm32wlxx_hal_rcc_ex.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32wlxx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32wlxx_hal_dma.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32wlxx_hal_cortex.h"
#endif

#ifdef HAL_ADC_MODULE_ENABLED
  #include "stm32wlxx_hal_adc.h"
  #include "stm32wlxx_hal_adc_ex.h"
#endif

#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32wlxx_hal_exti.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32wlxx_hal_flash.h"
  #include "stm32wlxx_hal_flash_ex.h"
#endif

#ifdef HAL_I2C_MODULE_ENABLED
  #include "stm32wlxx_hal_i2c.h"
  #include "stm32wlxx_hal_i2c_ex.h"
#endif

#ifdef HAL_IWDG_MODULE_ENABLED
  #include "stm32wlxx_hal_iwdg.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32wlxx_hal_pwr.h"
  #include "stm32wlxx_hal_pwr_ex.h"
#endif

#ifdef HAL_RTC_MODULE_ENABLED
  #include "stm32wlxx_hal_rtc.h"
  #include "stm32wlxx_hal_rtc_ex.h"
#endif

#ifdef HAL_SPI_MODULE_ENABLED
  #include "stm32wlxx_hal_spi.h"
#endif

#ifdef HAL_SUBGHZ_MODULE_ENABLED
  #include "stm32wlxx_hal_subghz.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32wlxx_hal_uart.h"
  #include "stm32wlxx_hal_uart_ex.h"
#endif

#endif /* HAVE_STM32WLXX_PERIPH_HEADERS */

#ifdef __cplusplus
}
#endif

#endif /* STM32WLXX_HAL_CONF_H */
