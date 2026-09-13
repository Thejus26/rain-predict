/**
 * @file    test_system_clock.c
 * @brief   Unit test suite for STM32WLE5 System Clock Tree and Low-Power Oscillators.
 * @details Validates 48 MHz MSI core configuration, LSE quartz startup, Stop 2 deep sleep
 *          clock scaling and wake restoration, and on-demand HSE TCXO radio gating.
 */

#include "unity.h"
#include "system_clock.h"

void setUp(void) {
    system_clock_test_reset();
}

void tearDown(void) {
    /* No dynamic resources to clean */
}

/**
 * @brief TC-S3-T1.2-01: Verify nominal clock tree initialization (MSI @ 48 MHz, LSE @ 32.768 kHz).
 */
static void test_system_clock_init_nominal(void) {
    status_t status = system_clock_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);

    /* Verify 48 MHz bus frequencies */
    TEST_ASSERT_EQUAL_UINT32(SYSTEM_CLOCK_SYSCLK_FREQ_HZ, system_clock_get_sysclk());
    TEST_ASSERT_EQUAL_UINT32(SYSTEM_CLOCK_HCLK_FREQ_HZ,   system_clock_get_hclk());
    TEST_ASSERT_EQUAL_UINT32(SYSTEM_CLOCK_PCLK1_FREQ_HZ,  system_clock_get_pclk1());
    TEST_ASSERT_EQUAL_UINT32(SYSTEM_CLOCK_PCLK2_FREQ_HZ,  system_clock_get_pclk2());

    /* Verify 2 Flash wait states for 48 MHz */
    TEST_ASSERT_EQUAL_UINT32(2, system_clock_test_get_flash_latency());

    /* Verify Core Voltage Scaling Range 1 (1.2V core) */
    TEST_ASSERT_EQUAL_UINT32(1, system_clock_test_get_voltage_scale());

    /* Verify LSE crystal is active and MSI PLL auto-calibration is locked */
    TEST_ASSERT_TRUE(system_clock_test_is_lse_ready());
    TEST_ASSERT_TRUE(system_clock_test_is_msi_pll_enabled());

    /* Verify instruction cache, data cache, and prefetch buffer are enabled */
    TEST_ASSERT_TRUE(system_clock_test_is_icache_enabled());
    TEST_ASSERT_TRUE(system_clock_test_is_dcache_enabled());
    TEST_ASSERT_TRUE(system_clock_test_is_prefetch_enabled());
}

/**
 * @brief TC-S3-T1.2-02: Verify frequency query accessors consistency.
 */
static void test_system_clock_frequency_getters(void) {
    (void)system_clock_init();

    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_sysclk());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_hclk());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_pclk1());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_pclk2());
}

/**
 * @brief TC-S3-T1.2-03: Verify LSE quartz oscillator initialization and timeout safeguard.
 */
static void test_system_clock_lse_init_and_fault(void) {
    /* Nominal startup */
    status_t status = system_clock_lse_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(system_clock_test_is_lse_ready());

    /* Simulated crystal fault / timeout */
    system_clock_test_reset();
    system_clock_test_set_lse_fault(true);
    status = system_clock_lse_init();
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
    TEST_ASSERT_FALSE(system_clock_test_is_lse_ready());

    /* System init with LSE fault should still complete with MSI uncalibrated fallback */
    status = system_clock_init();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_sysclk());
    TEST_ASSERT_FALSE(system_clock_test_is_msi_pll_enabled());
}

/**
 * @brief TC-S3-T1.2-04: Verify Stop 2 deep sleep preparation and fast wake restoration.
 */
static void test_system_clock_sleep_and_wake_transition(void) {
    (void)system_clock_init();

    /* Prepare for Stop 2 Deep Sleep */
    status_t status = system_clock_sleep_prepare();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(4000000UL, system_clock_get_sysclk());
    TEST_ASSERT_EQUAL_UINT32(0, system_clock_test_get_flash_latency());

    /* Wake restore */
    status = system_clock_wake_restore();
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_sysclk());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_hclk());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_pclk1());
    TEST_ASSERT_EQUAL_UINT32(48000000UL, system_clock_get_pclk2());
    TEST_ASSERT_EQUAL_UINT32(2, system_clock_test_get_flash_latency());
    TEST_ASSERT_TRUE(system_clock_test_is_msi_pll_enabled());
}

/**
 * @brief TC-S3-T1.2-05: Verify on-demand Sub-GHz LoRa radio HSE TCXO 32 MHz clock gating.
 */
static void test_system_clock_hse_radio_gating(void) {
    (void)system_clock_init();

    /* Initially radio clock is OFF */
    TEST_ASSERT_FALSE(system_clock_test_is_hse_radio_ready());

    /* Enable radio clock */
    status_t status = system_clock_hse_radio_enable(true);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_TRUE(system_clock_test_is_hse_radio_ready());

    /* Disable radio clock after packet transmission */
    status = system_clock_hse_radio_enable(false);
    TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
    TEST_ASSERT_FALSE(system_clock_test_is_hse_radio_ready());
}

/**
 * @brief TC-S3-T1.2-06: Verify HSE TCXO radio fault handling.
 */
static void test_system_clock_hse_radio_fault(void) {
    (void)system_clock_init();

    system_clock_test_set_hse_fault(true);
    status_t status = system_clock_hse_radio_enable(true);
    TEST_ASSERT_EQUAL_INT(STATUS_ERR_TIMEOUT, status);
    TEST_ASSERT_FALSE(system_clock_test_is_hse_radio_ready());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_system_clock_init_nominal);
    RUN_TEST(test_system_clock_frequency_getters);
    RUN_TEST(test_system_clock_lse_init_and_fault);
    RUN_TEST(test_system_clock_sleep_and_wake_transition);
    RUN_TEST(test_system_clock_hse_radio_gating);
    RUN_TEST(test_system_clock_hse_radio_fault);
    return UNITY_END();
}
