/**
 * @file    test_hal_conf.c
 * @brief   Unit test verification suite for STM32CubeWL HAL Module Configuration.
 * @details Validates module whitelist/blacklist definitions, oscillator constants,
 *          cache acceleration settings, and assertion macro safety.
 */

#include <stdbool.h>
#include "unity.h"
#include "stm32wlxx_hal_conf.h"

void setUp(void) {
    /* No state setup required */
}

void tearDown(void) {
    /* No cleanup required */
}

/**
 * @brief TC-S3-T1.3-01: Verify required peripheral HAL modules are enabled.
 */
static void test_hal_conf_enabled_modules(void) {
#ifndef HAL_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_ADC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_ADC_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_CORTEX_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_CORTEX_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_DMA_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_DMA_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_EXTI_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_EXTI_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_FLASH_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_FLASH_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_GPIO_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_GPIO_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_I2C_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_I2C_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_IWDG_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_IWDG_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_PWR_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_PWR_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_RCC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_RCC_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_RTC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_RTC_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_SPI_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_SPI_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_SUBGHZ_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_SUBGHZ_MODULE_ENABLED must be defined");
#endif
#ifndef HAL_UART_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_UART_MODULE_ENABLED must be defined");
#endif
    TEST_ASSERT_TRUE(true);
}

/**
 * @brief TC-S3-T1.3-02: Verify unused peripheral HAL modules are disabled (zero dead-code bloat).
 */
static void test_hal_conf_disabled_modules(void) {
#ifdef HAL_COMP_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_COMP_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_CRC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_CRC_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_CRYP_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_CRYP_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_DAC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_DAC_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_GTZC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_GTZC_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_HSEM_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_HSEM_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_IPCC_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_IPCC_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_IRDA_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_IRDA_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_LPTIM_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_LPTIM_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_RNG_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_RNG_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_SMARTCARD_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_SMARTCARD_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_SMBUS_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_SMBUS_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_TIM_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_TIM_MODULE_ENABLED should be disabled");
#endif
#ifdef HAL_WWDG_MODULE_ENABLED
    TEST_FAIL_MESSAGE("HAL_WWDG_MODULE_ENABLED should be disabled");
#endif
    TEST_ASSERT_TRUE(true);
}

/**
 * @brief TC-S3-T1.3-03: Verify oscillator frequency and electrical voltage constants.
 */
static void test_hal_conf_hardware_constants(void) {
    TEST_ASSERT_EQUAL_UINT32(32000000UL, HSE_VALUE);
    TEST_ASSERT_EQUAL_UINT32(48000000UL, MSI_VALUE);
    TEST_ASSERT_EQUAL_UINT32(16000000UL, HSI_VALUE);
    TEST_ASSERT_EQUAL_UINT32(32768UL,    LSE_VALUE);
    TEST_ASSERT_EQUAL_UINT32(32000UL,    LSI_VALUE);
    TEST_ASSERT_EQUAL_UINT32(3300UL,     VDD_VALUE);
    TEST_ASSERT_EQUAL_UINT32(100U,       HSE_STARTUP_TIMEOUT);
    TEST_ASSERT_EQUAL_UINT32(5000U,      LSE_STARTUP_TIMEOUT);
}

/**
 * @brief TC-S3-T1.3-04: Verify execution determinism, SysTick priority, and cache acceleration.
 */
static void test_hal_conf_performance_settings(void) {
    TEST_ASSERT_EQUAL_UINT32(1U, PREFETCH_ENABLE);
    TEST_ASSERT_EQUAL_UINT32(1U, INSTRUCTION_CACHE_ENABLE);
    TEST_ASSERT_EQUAL_UINT32(1U, DATA_CACHE_ENABLE);
    TEST_ASSERT_EQUAL_UINT32(0x00U, TICK_INT_PRIORITY);
    TEST_ASSERT_EQUAL_UINT32(0U, USE_RTOS);
}

/**
 * @brief TC-S3-T1.3-05: Verify assert_param macro evaluation.
 */
static void test_hal_conf_assert_param_macro(void) {
    /* Verify assert_param macro compiles and passes for true expressions */
    assert_param(1);
    assert_param(HSE_VALUE == 32000000UL);
    assert_param(LSE_VALUE == 32768UL);
    TEST_ASSERT_TRUE(true);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hal_conf_enabled_modules);
    RUN_TEST(test_hal_conf_disabled_modules);
    RUN_TEST(test_hal_conf_hardware_constants);
    RUN_TEST(test_hal_conf_performance_settings);
    RUN_TEST(test_hal_conf_assert_param_macro);
    return UNITY_END();
}
