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

opt3001_day_state_t opt3001_classify_day_state(float current_lux, opt3001_day_state_t prev_state) {
    switch (prev_state) {
        case OPT3001_STATE_NIGHT:
            if (current_lux >= OPT3001_DAYLIGHT_CONFIRM_LUX) {
                return OPT3001_STATE_DAYLIGHT;
            } else if (current_lux >= OPT3001_DAWN_THRESHOLD_LUX) {
                return OPT3001_STATE_TWILIGHT;
            }
            return OPT3001_STATE_NIGHT;

        case OPT3001_STATE_TWILIGHT:
            if (current_lux >= OPT3001_DAYLIGHT_CONFIRM_LUX) {
                return OPT3001_STATE_DAYLIGHT;
            } else if (current_lux < OPT3001_NIGHT_THRESHOLD_LUX) {
                return OPT3001_STATE_NIGHT;
            }
            return OPT3001_STATE_TWILIGHT;

        case OPT3001_STATE_DAYLIGHT:
            if (current_lux < OPT3001_NIGHT_THRESHOLD_LUX) {
                return OPT3001_STATE_NIGHT;
            } else if (current_lux < OPT3001_DUSK_THRESHOLD_LUX) {
                return OPT3001_STATE_TWILIGHT;
            }
            return OPT3001_STATE_DAYLIGHT;

        default:
            return (current_lux >= OPT3001_DAYLIGHT_CONFIRM_LUX) ? OPT3001_STATE_DAYLIGHT : OPT3001_STATE_NIGHT;
    }
}

bool opt3001_is_daylight(float current_lux, bool prev_is_daylight) {
    if (prev_is_daylight) {
        /* Exit daylight only if drops below dusk threshold (40 Lux) */
        return (current_lux >= OPT3001_DUSK_THRESHOLD_LUX);
    } else {
        /* Enter daylight only if exceeds confirmation threshold (50 Lux) */
        return (current_lux >= OPT3001_DAYLIGHT_CONFIRM_LUX);
    }
}

uint8_t opt3001_evaluate_solar_attenuation(float current_lux,
                                           float history_30m_lux,
                                           bool is_daylight,
                                           float *p_drop_ratio,
                                           bool *p_solar_alarm) {
    float drop_ratio = 0.0f;
    bool solar_alarm = false;
    uint8_t score = 0;

    if (!is_daylight || history_30m_lux < OPT3001_ATTENUATION_MIN_HIST_LUX) {
        if (p_drop_ratio != NULL) {
            *p_drop_ratio = 0.0f;
        }
        if (p_solar_alarm != NULL) {
            *p_solar_alarm = false;
        }
        return 0; /* Night or dawn/dusk: suppress scoring */
    }

    /* Calculate fractional drop over past 30 minutes */
    if (history_30m_lux > current_lux) {
        drop_ratio = (history_30m_lux - current_lux) / history_30m_lux;
    } else {
        drop_ratio = 0.0f; /* Sunlight steady or increasing */
    }

    if (drop_ratio >= OPT3001_DROP_RATIO_SEVERE && current_lux < OPT3001_ATTENUATION_SEVERE_MAX_LUX) {
        score = 100; /* Severe darkening (>70% drop to <3000 lux) */
        solar_alarm = true;
    } else if (drop_ratio >= OPT3001_DROP_RATIO_MODERATE) {
        score = 65;  /* Moderate darkening (>= 50% drop) */
        solar_alarm = true;
    } else if (drop_ratio >= OPT3001_DROP_RATIO_MINOR) {
        score = 30;  /* Minor darkening (>= 30% drop) */
        solar_alarm = false;
    } else {
        score = 0;
        solar_alarm = false;
    }

    if (p_drop_ratio != NULL) {
        *p_drop_ratio = drop_ratio;
    }
    if (p_solar_alarm != NULL) {
        *p_solar_alarm = solar_alarm;
    }

    return score;
}

status_t opt3001_update_solar_context(float current_lux, float history_30m_lux, opt3001_solar_context_t *p_ctx) {
    if (p_ctx == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    p_ctx->day_state = opt3001_classify_day_state(current_lux, p_ctx->day_state);
    p_ctx->is_daylight = (p_ctx->day_state == OPT3001_STATE_DAYLIGHT);

    p_ctx->attenuation_score = opt3001_evaluate_solar_attenuation(
        current_lux,
        history_30m_lux,
        p_ctx->is_daylight,
        &p_ctx->drop_ratio,
        &p_ctx->solar_drop_alarm
    );

    if (p_ctx->attenuation_score >= 100) {
        p_ctx->attenuation_level = OPT3001_ATTENUATION_SEVERE;
    } else if (p_ctx->attenuation_score >= 65) {
        p_ctx->attenuation_level = OPT3001_ATTENUATION_MODERATE;
    } else if (p_ctx->attenuation_score >= 30) {
        p_ctx->attenuation_level = OPT3001_ATTENUATION_MINOR;
    } else {
        p_ctx->attenuation_level = OPT3001_ATTENUATION_NONE;
    }

    return STATUS_OK;
}
