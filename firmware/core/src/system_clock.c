/**
 * @file    system_clock.c
 * @brief   System clock tree initialization and low-power switching implementation.
 * @details Implements 48 MHz MSI core configuration, LSE quartz startup, Stop 2 deep sleep
 *          clock transitions, and on-demand Sub-GHz radio TCXO gating.
 */

#include "system_clock.h"

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

status_t system_clock_init(void) {
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};
    RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

    /* 1. Configure Power Regulator to Range 1 (High Performance: 1.2V core) */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* 2. Enable and stabilize LSE 32.768 kHz Quartz Crystal */
    status_t lse_status = system_clock_lse_init();

    /* 3. Configure Multi-Speed Internal (MSI) RC Oscillator to 48 MHz (Range 11) */
    rcc_osc_init.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    rcc_osc_init.MSIState            = RCC_MSI_ON;
    rcc_osc_init.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    rcc_osc_init.MSIClockRange       = RCC_MSIRANGE_11; /* 48 MHz */

    /* Enable MSI Auto-Calibration via LSE if crystal started successfully */
    if (lse_status == STATUS_OK) {
        rcc_osc_init.MSIPLLMode      = RCC_MSIPLL_MODE_ENABLE;
    } else {
        rcc_osc_init.MSIPLLMode      = RCC_MSIPLL_MODE_DISABLE;
    }

    rcc_osc_init.PLL.PLLState        = RCC_PLL_NONE; /* Direct MSI SYSCLK */

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        return STATUS_ERR_GENERIC;
    }

    /* 4. Configure SYSCLK, HCLK, PCLK1, PCLK2 with 2 Flash Wait States */
    rcc_clk_init.ClockType      = RCC_CLOCKTYPE_HCLK   |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1  |
                                  RCC_CLOCKTYPE_PCLK2;
    rcc_clk_init.SYSCLKSource   = RCC_SYSCLKSOURCE_MSI;
    rcc_clk_init.AHBCLKDivider  = RCC_SYSCLK_DIV1;  /* HCLK = 48 MHz */
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV1;    /* PCLK1 = 48 MHz */
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;    /* PCLK2 = 48 MHz */

    if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_2) != HAL_OK) {
        return STATUS_ERR_GENERIC;
    }

    /* 5. Configure Peripheral Clock Multiplexers */
    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_I2C1   |
                                           RCC_PERIPHCLK_USART1 |
                                           RCC_PERIPHCLK_RTC;
    periph_clk_init.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_PCLK1;
    periph_clk_init.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    periph_clk_init.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;

    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK) {
        return STATUS_ERR_GENERIC;
    }

    /* 6. Enable Instruction Cache, Data Cache, and Flash Prefetch Buffer */
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();

    /* 7. Update CMSIS SystemCoreClock variable */
    SystemCoreClockUpdate();

    return STATUS_OK;
}

status_t system_clock_lse_init(void) {
    RCC_OscInitTypeDef rcc_osc_init = {0};

    /* Enable write access to Backup Domain (RTC & LSE control) */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    rcc_osc_init.LSEState       = RCC_LSE_ON;

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        return STATUS_ERR_TIMEOUT;
    }

    return STATUS_OK;
}

status_t system_clock_sleep_prepare(void) {
    /* Configure MSI at 4 MHz as default wake-up clock */
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);
    return STATUS_OK;
}

status_t system_clock_wake_restore(void) {
    /* Upon Stop 2 wake, set 2 Flash Wait States before raising MSI to 48 MHz */
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_2);

    /* Scale MSI back to 48 MHz (Range 11) */
    __HAL_RCC_MSI_RANGE_CONFIG(RCC_MSIRANGE_11);

    /* Re-enable MSI Auto-Calibration via LSE */
    SET_BIT(RCC->CR, RCC_CR_MSIPLLEN);

    SystemCoreClockUpdate();
    return STATUS_OK;
}

status_t system_clock_hse_radio_enable(bool enable) {
    RCC_OscInitTypeDef rcc_osc_init = {0};

    if (enable) {
        /* Enable HSE in TCXO mode for Sub-GHz Radio */
        rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        rcc_osc_init.HSEState       = RCC_HSE_TCXO; /* TCXO supply on STM32WLE5 */

        if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
            return STATUS_ERR_TIMEOUT;
        }
    } else {
        /* Shut down HSE TCXO to save energy */
        rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        rcc_osc_init.HSEState       = RCC_HSE_OFF;

        if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
            return STATUS_ERR_GENERIC;
        }
    }

    return STATUS_OK;
}

uint32_t system_clock_get_sysclk(void) {
    return SYSTEM_CLOCK_SYSCLK_FREQ_HZ;
}

uint32_t system_clock_get_hclk(void) {
    return SYSTEM_CLOCK_HCLK_FREQ_HZ;
}

uint32_t system_clock_get_pclk1(void) {
    return SYSTEM_CLOCK_PCLK1_FREQ_HZ;
}

uint32_t system_clock_get_pclk2(void) {
    return SYSTEM_CLOCK_PCLK2_FREQ_HZ;
}

#else

/* ============================================================================
 * Host Unit Test & Simulation Implementation
 * ============================================================================ */

static uint32_t s_sysclk_hz = 4000000UL;
static uint32_t s_hclk_hz   = 4000000UL;
static uint32_t s_pclk1_hz  = 4000000UL;
static uint32_t s_pclk2_hz  = 4000000UL;

static uint32_t s_flash_latency  = 0;
static uint32_t s_voltage_scale  = 1;
static bool s_lse_ready          = false;
static bool s_hse_radio_ready    = false;
static bool s_msi_pll_enabled    = false;
static bool s_icache_enabled     = false;
static bool s_dcache_enabled     = false;
static bool s_prefetch_enabled   = false;

static bool s_lse_fault_sim      = false;
static bool s_hse_fault_sim      = false;

void system_clock_test_reset(void) {
    s_sysclk_hz         = 4000000UL;
    s_hclk_hz           = 4000000UL;
    s_pclk1_hz          = 4000000UL;
    s_pclk2_hz          = 4000000UL;
    s_flash_latency     = 0;
    s_voltage_scale     = 1;
    s_lse_ready         = false;
    s_hse_radio_ready   = false;
    s_msi_pll_enabled   = false;
    s_icache_enabled    = false;
    s_dcache_enabled    = false;
    s_prefetch_enabled  = false;
    s_lse_fault_sim     = false;
    s_hse_fault_sim     = false;
}

void system_clock_test_set_lse_fault(bool fault) {
    s_lse_fault_sim = fault;
}

void system_clock_test_set_hse_fault(bool fault) {
    s_hse_fault_sim = fault;
}

bool system_clock_test_is_lse_ready(void) {
    return s_lse_ready;
}

bool system_clock_test_is_hse_radio_ready(void) {
    return s_hse_radio_ready;
}

bool system_clock_test_is_msi_pll_enabled(void) {
    return s_msi_pll_enabled;
}

uint32_t system_clock_test_get_flash_latency(void) {
    return s_flash_latency;
}

bool system_clock_test_is_icache_enabled(void) {
    return s_icache_enabled;
}

bool system_clock_test_is_dcache_enabled(void) {
    return s_dcache_enabled;
}

bool system_clock_test_is_prefetch_enabled(void) {
    return s_prefetch_enabled;
}

uint32_t system_clock_test_get_voltage_scale(void) {
    return s_voltage_scale;
}

status_t system_clock_init(void) {
    s_voltage_scale = 1;

    status_t lse_status = system_clock_lse_init();

    s_sysclk_hz = SYSTEM_CLOCK_SYSCLK_FREQ_HZ;
    s_hclk_hz   = SYSTEM_CLOCK_HCLK_FREQ_HZ;
    s_pclk1_hz  = SYSTEM_CLOCK_PCLK1_FREQ_HZ;
    s_pclk2_hz  = SYSTEM_CLOCK_PCLK2_FREQ_HZ;

    s_flash_latency    = 2;
    s_msi_pll_enabled  = (lse_status == STATUS_OK);
    s_icache_enabled   = true;
    s_dcache_enabled   = true;
    s_prefetch_enabled = true;

    return STATUS_OK;
}

status_t system_clock_lse_init(void) {
    if (s_lse_fault_sim) {
        s_lse_ready = false;
        return STATUS_ERR_TIMEOUT;
    }
    s_lse_ready = true;
    return STATUS_OK;
}

status_t system_clock_sleep_prepare(void) {
    s_sysclk_hz = 4000000UL;
    s_hclk_hz   = 4000000UL;
    s_pclk1_hz  = 4000000UL;
    s_pclk2_hz  = 4000000UL;
    s_flash_latency = 0;
    return STATUS_OK;
}

status_t system_clock_wake_restore(void) {
    s_flash_latency = 2;
    s_sysclk_hz = SYSTEM_CLOCK_SYSCLK_FREQ_HZ;
    s_hclk_hz   = SYSTEM_CLOCK_HCLK_FREQ_HZ;
    s_pclk1_hz  = SYSTEM_CLOCK_PCLK1_FREQ_HZ;
    s_pclk2_hz  = SYSTEM_CLOCK_PCLK2_FREQ_HZ;
    s_msi_pll_enabled = true;
    return STATUS_OK;
}

status_t system_clock_hse_radio_enable(bool enable) {
    if (enable) {
        if (s_hse_fault_sim) {
            s_hse_radio_ready = false;
            return STATUS_ERR_TIMEOUT;
        }
        s_hse_radio_ready = true;
    } else {
        s_hse_radio_ready = false;
    }
    return STATUS_OK;
}

uint32_t system_clock_get_sysclk(void) {
    return s_sysclk_hz;
}

uint32_t system_clock_get_hclk(void) {
    return s_hclk_hz;
}

uint32_t system_clock_get_pclk1(void) {
    return s_pclk1_hz;
}

uint32_t system_clock_get_pclk2(void) {
    return s_pclk2_hz;
}

#endif
