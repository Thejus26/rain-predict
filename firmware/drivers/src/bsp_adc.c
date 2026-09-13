/**
 * @file    bsp_adc.c
 * @brief   Low-power ADC driver implementation for STM32WLE5 SoC.
 * @details Implements internal VREFINT reference calibration, gated battery divider
 *          timing control, 8x oversampled averaging, and supply-compensated battery math.
 */

#include "bsp_adc.h"
#include "bsp_power_rails.h"
#include "board_config.h"

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

static ADC_HandleTypeDef s_hadc;
static bool s_adc_initialized = false;

status_t bsp_adc_init(void) {
    if (s_adc_initialized) {
        return STATUS_OK;
    }

    __HAL_RCC_ADC_CLK_ENABLE();

    s_hadc.Instance                   = ADC;
    s_hadc.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    s_hadc.Init.Resolution            = ADC_RESOLUTION_12B;
    s_hadc.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    s_hadc.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    s_hadc.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    s_hadc.Init.LowPowerAutoWait      = DISABLE;
    s_hadc.Init.LowPowerAutoPowerOff  = DISABLE;
    s_hadc.Init.ContinuousConvMode    = DISABLE;
    s_hadc.Init.NbrOfConversion       = 1;
    s_hadc.Init.DiscontinuousConvMode = DISABLE;
    s_hadc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    s_hadc.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    s_hadc.Init.DMAContinuousRequests = DISABLE;
    s_hadc.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    s_hadc.Init.SamplingTimeCommon1   = ADC_SAMPLETIME_160CYCLES_5;

    if (HAL_ADC_Init(&s_hadc) != HAL_OK) {
        return STATUS_ERR_INVALID_STATE;
    }

    /* Perform ADC self-calibration */
    if (HAL_ADCEx_Calibration_Start(&s_hadc) != HAL_OK) {
        return STATUS_ERR_INVALID_STATE;
    }

    s_adc_initialized = true;
    return STATUS_OK;
}

status_t bsp_adc_deinit(void) {
    if (!s_adc_initialized) {
        return STATUS_OK;
    }

    HAL_ADC_DeInit(&s_hadc);
    __HAL_RCC_ADC_CLK_DISABLE();
    s_adc_initialized = false;
    return STATUS_OK;
}

uint16_t bsp_adc_get_vrefint_factory_cal(void) {
    uint16_t cal = *BSP_ADC_VREFINT_CAL_ADDR;
    if (cal < 1000U || cal > 3000U) {
        return BSP_ADC_DEFAULT_VREFINT_CAL;
    }
    return cal;
}

status_t bsp_adc_read_vrefint_raw(uint16_t *p_raw_vref) {
    if (p_raw_vref == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (!s_adc_initialized) {
        status_t status = bsp_adc_init();
        if (status != STATUS_OK) {
            return status;
        }
    }

    ADC_ChannelConfTypeDef s_config = {0};
    s_config.Channel      = ADC_CHANNEL_VREFINT;
    s_config.Rank         = ADC_REGULAR_RANK_1;
    s_config.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;

    if (HAL_ADC_ConfigChannel(&s_hadc, &s_config) != HAL_OK) {
        return STATUS_ERR_INVALID_STATE;
    }

    uint32_t accumulator = 0;
    for (uint32_t i = 0; i < BSP_ADC_OVERSAMPLING_COUNT; i++) {
        HAL_ADC_Start(&s_hadc);
        if (HAL_ADC_PollForConversion(&s_hadc, 10) != HAL_OK) {
            HAL_ADC_Stop(&s_hadc);
            return STATUS_ERR_TIMEOUT;
        }
        accumulator += HAL_ADC_GetValue(&s_hadc);
    }
    HAL_ADC_Stop(&s_hadc);

    *p_raw_vref = (uint16_t)(accumulator / BSP_ADC_OVERSAMPLING_COUNT);
    return STATUS_OK;
}

status_t bsp_adc_read_vbat_raw(uint16_t *p_raw_vbat) {
    if (p_raw_vbat == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (!s_adc_initialized) {
        status_t status = bsp_adc_init();
        if (status != STATUS_OK) {
            return status;
        }
    }

    /* 1. Gate P-MOSFET divider (PB1 = LOW) */
    status_t rail_status = bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, true);
    if (rail_status != STATUS_OK) {
        (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
        return rail_status;
    }

    /* 2. Configure ADC Channel 1 (PB0 / ADC1_IN1) */
    ADC_ChannelConfTypeDef s_config = {0};
    s_config.Channel      = ADC_CHANNEL_1;
    s_config.Rank         = ADC_REGULAR_RANK_1;
    s_config.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;

    if (HAL_ADC_ConfigChannel(&s_hadc, &s_config) != HAL_OK) {
        (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
        return STATUS_ERR_INVALID_STATE;
    }

    /* 3. 8x oversampled conversion with error safety */
    uint32_t accumulator = 0;
    for (uint32_t i = 0; i < BSP_ADC_OVERSAMPLING_COUNT; i++) {
        HAL_ADC_Start(&s_hadc);
        if (HAL_ADC_PollForConversion(&s_hadc, 10) != HAL_OK) {
            HAL_ADC_Stop(&s_hadc);
            (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
            return STATUS_ERR_TIMEOUT;
        }
        accumulator += HAL_ADC_GetValue(&s_hadc);
    }
    HAL_ADC_Stop(&s_hadc);

    /* 4. Immediately un-gate P-MOSFET divider (PB1 = HIGH) */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);

    *p_raw_vbat = (uint16_t)(accumulator / BSP_ADC_OVERSAMPLING_COUNT);
    return STATUS_OK;
}

#else

/* ============================================================================
 * Host Simulation & Unit Testing Backend
 * ============================================================================ */

static bool     s_sim_initialized      = false;
static uint16_t s_sim_cal_val          = BSP_ADC_DEFAULT_VREFINT_CAL;
static uint16_t s_sim_raw_vref         = 1660U;
static uint16_t s_sim_raw_vbat         = 2048U;
static status_t s_sim_injected_error   = STATUS_OK;
static uint32_t s_sim_conversion_count = 0;

void bsp_adc_test_reset(void) {
    s_sim_initialized      = false;
    s_sim_cal_val          = BSP_ADC_DEFAULT_VREFINT_CAL;
    s_sim_raw_vref         = 1660U;
    s_sim_raw_vbat         = 2048U;
    s_sim_injected_error   = STATUS_OK;
    s_sim_conversion_count = 0;
}

void bsp_adc_test_set_factory_cal(uint16_t cal_val) {
    s_sim_cal_val = cal_val;
}

void bsp_adc_test_set_vrefint_raw(uint16_t raw_vref) {
    s_sim_raw_vref = raw_vref;
}

void bsp_adc_test_set_vbat_raw(uint16_t raw_vbat) {
    s_sim_raw_vbat = raw_vbat;
}

void bsp_adc_test_inject_error(status_t error_code) {
    s_sim_injected_error = error_code;
}

bool bsp_adc_test_is_initialized(void) {
    return s_sim_initialized;
}

uint32_t bsp_adc_test_get_conversion_count(void) {
    return s_sim_conversion_count;
}

status_t bsp_adc_init(void) {
    s_sim_initialized = true;
    return STATUS_OK;
}

status_t bsp_adc_deinit(void) {
    s_sim_initialized = false;
    return STATUS_OK;
}

uint16_t bsp_adc_get_vrefint_factory_cal(void) {
    if (s_sim_cal_val < 1000U || s_sim_cal_val > 3000U) {
        return BSP_ADC_DEFAULT_VREFINT_CAL;
    }
    return s_sim_cal_val;
}

status_t bsp_adc_read_vrefint_raw(uint16_t *p_raw_vref) {
    if (p_raw_vref == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (s_sim_injected_error != STATUS_OK) {
        status_t err = s_sim_injected_error;
        s_sim_injected_error = STATUS_OK;
        return err;
    }

    if (!s_sim_initialized) {
        (void)bsp_adc_init();
    }

    s_sim_conversion_count += BSP_ADC_OVERSAMPLING_COUNT;
    *p_raw_vref = s_sim_raw_vref;
    return STATUS_OK;
}

status_t bsp_adc_read_vbat_raw(uint16_t *p_raw_vbat) {
    if (p_raw_vbat == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* 1. Gate P-MOSFET divider (PB1 = LOW) */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, true);

    if (s_sim_injected_error != STATUS_OK) {
        (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);
        status_t err = s_sim_injected_error;
        s_sim_injected_error = STATUS_OK;
        return err;
    }

    if (!s_sim_initialized) {
        (void)bsp_adc_init();
    }

    s_sim_conversion_count += BSP_ADC_OVERSAMPLING_COUNT;
    *p_raw_vbat = s_sim_raw_vbat;

    /* 2. Immediately un-gate P-MOSFET divider (PB1 = HIGH) */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_VBAT_SENSE, false);

    return STATUS_OK;
}

#endif /* HAVE_STM32WLXX_HAL */

status_t bsp_adc_read_vbat_mv(uint16_t *p_vbat_mv) {
    if (p_vbat_mv == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t raw_vref = 0;
    uint16_t raw_vbat = 0;

    status_t status = bsp_adc_read_vrefint_raw(&raw_vref);
    if (status != STATUS_OK) {
        return status;
    }

    status = bsp_adc_read_vbat_raw(&raw_vbat);
    if (status != STATUS_OK) {
        return status;
    }

    if (raw_vref == 0U) {
        return STATUS_ERR_INVALID_STATE;
    }

    uint16_t cal_val = bsp_adc_get_vrefint_factory_cal();

    /* Compensated fixed-point arithmetic:
     * Vbat_mV = floor((2 * 3000 * cal_val * raw_vbat + (4095 * raw_vref / 2)) / (4095 * raw_vref))
     */
    uint64_t numerator = (uint64_t)BSP_ADC_VBAT_DIVIDER_SCALE *
                         (uint64_t)BSP_ADC_VREFINT_CAL_VOLTAGE_MV *
                         (uint64_t)cal_val *
                         (uint64_t)raw_vbat;

    uint32_t denominator = (uint32_t)BSP_ADC_FULL_SCALE_12BIT * (uint32_t)raw_vref;
    uint32_t rounded_mv = (uint32_t)((numerator + ((uint64_t)denominator / 2ULL)) / denominator);

    if (rounded_mv > 5000U) {
        rounded_mv = 5000U;
    }

    *p_vbat_mv = (uint16_t)rounded_mv;
    return STATUS_OK;
}
