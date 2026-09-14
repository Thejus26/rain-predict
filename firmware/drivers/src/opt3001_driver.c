/**
 * @file    opt3001_driver.c
 * @brief   Texas Instruments OPT3001 Ambient Light Sensor driver implementation.
 * @details Implements 16-bit big-endian register I/O, device ID verification,
 *          single-shot forced-mode trigger, bounded conversion completion polling,
 *          and raw exponent/mantissa register readout.
 *          Adheres to C99 standards, MISRA-C guidelines, and zero-dynamic-memory allocation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)
#include "stm32wlxx_hal.h"
#else
#ifndef HAL_Delay
#define HAL_Delay(ms)   ((void)(ms))
#endif
#endif

#include "i2c_bus.h"
#include "opt3001_driver.h"

/* ========================================================================== */
/* Public Driver API Implementation                                           */
/* ========================================================================== */

status_t opt3001_read_device_id(opt3001_dev_t *dev, uint16_t *p_mfg_id, uint16_t *p_dev_id) {
    if (dev == NULL || p_mfg_id == NULL || p_dev_id == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t mfg_id = 0;
    uint16_t dev_id = 0;

    /* Read Manufacturer ID (0x7E) */
    status_t status = i2c_bus_read16(dev->i2c_address,
                                    OPT3001_REG_MANUFACTURER_ID,
                                    &mfg_id,
                                    OPT3001_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Read Device ID (0x7F) */
    status = i2c_bus_read16(dev->i2c_address,
                            OPT3001_REG_DEVICE_ID,
                            &dev_id,
                            OPT3001_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    *p_mfg_id = mfg_id;
    *p_dev_id = dev_id;
    dev->manufacturer_id = mfg_id;
    dev->device_id = dev_id;

    return STATUS_OK;
}

status_t opt3001_init(opt3001_dev_t *dev, uint8_t i2c_addr) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (i2c_addr < 0x44U || i2c_addr > 0x47U) {
        return STATUS_ERR_INVALID_PARAM;
    }

    (void)memset(dev, 0, sizeof(opt3001_dev_t));
    dev->i2c_address = i2c_addr;

    /* Verify Manufacturer and Device IDs */
    uint16_t mfg_id = 0;
    uint16_t dev_id = 0;
    status_t status = opt3001_read_device_id(dev, &mfg_id, &dev_id);
    if (status != STATUS_OK) {
        return status;
    }

    if (mfg_id != OPT3001_EXPECTED_MFG_ID || dev_id != OPT3001_EXPECTED_DEV_ID) {
        return STATUS_ERR_HARDWARE;
    }

    dev->is_initialized = true;
    return STATUS_OK;
}

status_t opt3001_trigger_single_shot(opt3001_dev_t *dev) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t cmd = OPT3001_CONFIG_SINGLE_SHOT_CMD;
    return i2c_bus_write16(dev->i2c_address,
                           OPT3001_REG_CONFIG,
                           cmd,
                           OPT3001_I2C_TIMEOUT_MS);
}

status_t opt3001_is_conversion_ready(opt3001_dev_t *dev, bool *p_is_ready) {
    if (dev == NULL || p_is_ready == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t config_reg = 0;
    status_t status = i2c_bus_read16(dev->i2c_address,
                                    OPT3001_REG_CONFIG,
                                    &config_reg,
                                    OPT3001_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    *p_is_ready = ((config_reg & OPT3001_CONFIG_CRF_BIT) != 0U);
    return STATUS_OK;
}

status_t opt3001_wait_for_completion(opt3001_dev_t *dev, uint32_t timeout_ms) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint32_t elapsed_ms = 0;
    const uint32_t initial_delay_ms = (timeout_ms >= 95U) ? 95U : 0U;
    const uint32_t poll_interval_ms = 5U;

    /* Initial wait for ~100ms integration */
    HAL_Delay(initial_delay_ms);
    elapsed_ms += initial_delay_ms;

    while (elapsed_ms <= timeout_ms) {
        bool ready = false;
        status_t status = opt3001_is_conversion_ready(dev, &ready);
        if (status != STATUS_OK) {
            return status;
        }

        if (ready) {
            return STATUS_OK;
        }

        HAL_Delay(poll_interval_ms);
        elapsed_ms += poll_interval_ms;
    }

    return STATUS_ERR_TIMEOUT;
}

status_t opt3001_read_raw_result(opt3001_dev_t *dev, opt3001_raw_data_t *p_raw) {
    if (dev == NULL || p_raw == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t result = 0;
    status_t status = i2c_bus_read16(dev->i2c_address,
                                    OPT3001_REG_RESULT,
                                    &result,
                                    OPT3001_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    p_raw->raw_result = result;
    p_raw->exponent   = (uint8_t)((result >> 12) & 0x0FU);
    p_raw->mantissa   = (uint16_t)(result & 0x0FFFU);

    return STATUS_OK;
}

status_t opt3001_sample_forced_raw(opt3001_dev_t *dev, opt3001_raw_data_t *p_raw) {
    if (dev == NULL || p_raw == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* 1. Trigger single-shot conversion */
    status_t status = opt3001_trigger_single_shot(dev);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Wait for Conversion Ready Flag (CRF == 1) */
    status = opt3001_wait_for_completion(dev, OPT3001_CONVERSION_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* 3. Read and unpack result register */
    return opt3001_read_raw_result(dev, p_raw);
}

float opt3001_raw_to_lux(uint16_t raw_result) {
    uint8_t exponent = (uint8_t)((raw_result >> 12) & 0x0FU);
    uint16_t mantissa = (uint16_t)(raw_result & 0x0FFFU);

    if (exponent > OPT3001_MAX_EXPONENT) {
        exponent = OPT3001_MAX_EXPONENT; /* Clamp exponent to 11 */
    }

    /* Lux = 0.01f * (1 << E) * (float)R */
    float lsb_size = 0.01f * (float)(1U << exponent);
    float lux = lsb_size * (float)mantissa;

    if (lux > OPT3001_MAX_LUX) {
        lux = OPT3001_MAX_LUX;
    }

    return lux;
}

uint32_t opt3001_raw_to_centi_lux(uint16_t raw_result) {
    uint8_t exponent = (uint8_t)((raw_result >> 12) & 0x0FU);
    uint16_t mantissa = (uint16_t)(raw_result & 0x0FFFU);

    if (exponent > OPT3001_MAX_EXPONENT) {
        exponent = OPT3001_MAX_EXPONENT;
    }

    /* Centi_Lux = mantissa << exponent */
    return ((uint32_t)mantissa << exponent);
}

float opt3001_lux_to_irradiance(float lux) {
    if (lux <= 0.0f) {
        return 0.0f;
    }
    return (lux / OPT3001_SOLAR_LUMINOUS_EFFICACY);
}

uint16_t opt3001_lux_to_telemetry_u16(float lux) {
    if (lux <= 0.0f) {
        return 0U;
    }

    /* Scale: 2.0 Lux per count with half-LSB rounding */
    uint32_t scaled = (uint32_t)((lux + 1.0f) / OPT3001_TELEMETRY_LUX_SCALE);

    if (scaled > OPT3001_TELEMETRY_MAX_RAW) {
        scaled = OPT3001_TELEMETRY_MAX_RAW;
    }

    return (uint16_t)scaled;
}

status_t opt3001_convert_raw(const opt3001_raw_data_t *p_raw, opt3001_reading_t *p_out) {
    if (p_raw == NULL || p_out == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    float lux = opt3001_raw_to_lux(p_raw->raw_result);
    p_out->lux              = lux;
    p_out->irradiance_w_m2  = opt3001_lux_to_irradiance(lux);
    p_out->centi_lux        = opt3001_raw_to_centi_lux(p_raw->raw_result);
    p_out->telemetry_raw    = opt3001_lux_to_telemetry_u16(lux);
    p_out->is_valid         = (p_raw->exponent <= OPT3001_MAX_EXPONENT);

    return STATUS_OK;
}

status_t opt3001_read_lux(opt3001_dev_t *dev, opt3001_reading_t *p_out) {
    if (dev == NULL || p_out == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    opt3001_raw_data_t raw = {0};
    status_t status = opt3001_sample_forced_raw(dev, &raw);
    if (status != STATUS_OK) {
        p_out->is_valid = false;
        return status;
    }

    return opt3001_convert_raw(&raw, p_out);
}
