/**
 * @file    bme280_driver.c
 * @brief   Bosch BME280 sensor driver implementation for STM32WLE5 SoC.
 * @details Implements 2-phase burst NVM calibration readout, hardware ID verification,
 *          forced-mode single-shot trigger, bounded conversion completion polling,
 *          atomic 8-byte raw ADC readout, Cortex-M4 single-precision FPU mathematical
 *          compensation (T, P, RH), and fixed-point telemetry serialization.
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
#include "bme280_driver.h"

/* ========================================================================== */
/* Saturation & Recovery State Tracking                                       */
/* ========================================================================== */

static bme280_saturation_status_t g_sat_status = {
    .is_saturated          = false,
    .condensation_detected = false,
    .recovery_triggered    = false,
    .saturation_cycles     = 0U,
    .state                 = BME280_STATE_NORMAL,
    .debiased_humidity_pct = 0.0f
};

/* ========================================================================== */
/* Public Driver API Implementation                                           */
/* ========================================================================== */

status_t bme280_read_chip_id(bme280_dev_t *dev, uint8_t *p_chip_id) {
    if (dev == NULL || p_chip_id == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t chip_id = 0;
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_CHIP_ID,
                                   &chip_id,
                                   1U,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    *p_chip_id = chip_id;
    dev->chip_id = chip_id;

    if (chip_id != BME280_CHIP_ID) {
        return STATUS_ERR_HARDWARE;
    }

    return STATUS_OK;
}

status_t bme280_validate_calibration(const bme280_calib_data_t *calib) {
    if (calib == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* Check for uninitialized / floating bus patterns (all 0x00 or all 0xFF) */
    if (calib->dig_T1 == 0x0000U || calib->dig_T1 == 0xFFFFU ||
        calib->dig_P1 == 0x0000U || calib->dig_P1 == 0xFFFFU ||
        calib->dig_H1 == 0x00U   || calib->dig_H1 == 0xFFU   ||
        calib->dig_H3 == 0xFFU) {
        return STATUS_ERR_DATA_CORRUPT;
    }

    return STATUS_OK;
}

status_t bme280_read_calibration(bme280_dev_t *dev) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t block1[BME280_CALIB_BLOCK1_LEN];
    uint8_t block2[BME280_CALIB_BLOCK2_LEN];
    memset(block1, 0, sizeof(block1));
    memset(block2, 0, sizeof(block2));

    /* Phase 1: Burst read 26 bytes from 0x88 to 0xA1 */
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_CALIB_00_25,
                                   block1,
                                   BME280_CALIB_BLOCK1_LEN,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Phase 2: Burst read 7 bytes from 0xE1 to 0xE7 */
    status = i2c_bus_read(dev->i2c_address,
                          BME280_REG_CALIB_26_41,
                          block2,
                          BME280_CALIB_BLOCK2_LEN,
                          BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Unpack Temperature Trimming Parameters (Little-Endian) */
    dev->calib.dig_T1 = (uint16_t)(((uint16_t)block1[1] << 8) | (uint16_t)block1[0]);
    dev->calib.dig_T2 = (int16_t)(((int16_t)block1[3] << 8) | (int16_t)block1[2]);
    dev->calib.dig_T3 = (int16_t)(((int16_t)block1[5] << 8) | (int16_t)block1[4]);

    /* Unpack Pressure Trimming Parameters (Little-Endian) */
    dev->calib.dig_P1 = (uint16_t)(((uint16_t)block1[7] << 8) | (uint16_t)block1[6]);
    dev->calib.dig_P2 = (int16_t)(((int16_t)block1[9] << 8) | (int16_t)block1[8]);
    dev->calib.dig_P3 = (int16_t)(((int16_t)block1[11] << 8) | (int16_t)block1[10]);
    dev->calib.dig_P4 = (int16_t)(((int16_t)block1[13] << 8) | (int16_t)block1[12]);
    dev->calib.dig_P5 = (int16_t)(((int16_t)block1[15] << 8) | (int16_t)block1[14]);
    dev->calib.dig_P6 = (int16_t)(((int16_t)block1[17] << 8) | (int16_t)block1[16]);
    dev->calib.dig_P7 = (int16_t)(((int16_t)block1[19] << 8) | (int16_t)block1[18]);
    dev->calib.dig_P8 = (int16_t)(((int16_t)block1[21] << 8) | (int16_t)block1[20]);
    dev->calib.dig_P9 = (int16_t)(((int16_t)block1[23] << 8) | (int16_t)block1[22]);

    /* Unpack Humidity Trimming Parameters */
    dev->calib.dig_H1 = block1[25]; /* Register 0xA1 */
    dev->calib.dig_H2 = (int16_t)(((int16_t)block2[1] << 8) | (int16_t)block2[0]); /* 0xE1 / 0xE2 */
    dev->calib.dig_H3 = block2[2]; /* Register 0xE3 */

    /* Complex Bit-Split for dig_H4 and dig_H5:
     * dig_H4 = (int8_t)reg[0xE4] << 4 | (reg[0xE5] & 0x0F)
     * dig_H5 = (int8_t)reg[0xE6] << 4 | (reg[0xE5] >> 4)
     */
    dev->calib.dig_H4 = (int16_t)(((int16_t)(int8_t)block2[3] << 4) |
                                  (int16_t)(block2[4] & 0x0FU));

    dev->calib.dig_H5 = (int16_t)(((int16_t)(int8_t)block2[5] << 4) |
                                  (int16_t)((block2[4] >> 4) & 0x0FU));

    dev->calib.dig_H6 = (int8_t)block2[6]; /* Register 0xE7 */

    /* Validate parsed coefficients */
    return bme280_validate_calibration(&dev->calib);
}

status_t bme280_init(bme280_dev_t *dev, uint8_t i2c_addr) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    if (i2c_addr != BME280_I2C_ADDR_PRIMARY && i2c_addr != BME280_I2C_ADDR_SECONDARY) {
        return STATUS_ERR_INVALID_PARAM;
    }

    memset(dev, 0, sizeof(bme280_dev_t));
    dev->i2c_address = i2c_addr;

    /* Set default meteorological sampling configuration */
    dev->config.osrs_t = BME280_OVERSAMPLING_2X;
    dev->config.osrs_p = BME280_OVERSAMPLING_16X;
    dev->config.osrs_h = BME280_OVERSAMPLING_1X;
    dev->config.filter = BME280_FILTER_COEFF_4;

    /* 1. Verify Chip ID */
    uint8_t chip_id = 0;
    status_t status = bme280_read_chip_id(dev, &chip_id);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Read and unpack calibration NVM */
    status = bme280_read_calibration(dev);
    if (status != STATUS_OK) {
        return status;
    }

    dev->is_initialized = true;
    return STATUS_OK;
}

const bme280_calib_data_t* bme280_get_calibration(const bme280_dev_t *dev) {
    if (dev == NULL || !dev->is_initialized) {
        return NULL;
    }
    return &dev->calib;
}

status_t bme280_configure(bme280_dev_t *dev, const bme280_config_t *config) {
    if (dev == NULL || config == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    dev->config = *config;

    /* 1. Write ctrl_hum (0xF2) -> osrs_h */
    uint8_t ctrl_hum = (uint8_t)(config->osrs_h & 0x07U);
    status_t status = i2c_bus_write(dev->i2c_address,
                                    BME280_REG_CTRL_HUM,
                                    &ctrl_hum,
                                    1U,
                                    BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Write config (0xF5) -> IIR Filter */
    uint8_t config_reg = (uint8_t)((config->filter & 0x07U) << 2);
    status = i2c_bus_write(dev->i2c_address,
                           BME280_REG_CONFIG,
                           &config_reg,
                           1U,
                           BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    return STATUS_OK;
}

status_t bme280_trigger_forced_mode(bme280_dev_t *dev) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* ctrl_meas: osrs_t[7:5] | osrs_p[4:2] | mode[1:0] */
    uint8_t ctrl_meas = (uint8_t)(((dev->config.osrs_t & 0x07U) << 5) |
                                  ((dev->config.osrs_p & 0x07U) << 2) |
                                  (BME280_MODE_FORCED & 0x03U));

    return i2c_bus_write(dev->i2c_address,
                         BME280_REG_CTRL_MEAS,
                         &ctrl_meas,
                         1U,
                         BME280_I2C_TIMEOUT_MS);
}

status_t bme280_is_measuring(bme280_dev_t *dev, bool *p_is_measuring) {
    if (dev == NULL || p_is_measuring == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t status_reg = 0;
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_STATUS,
                                   &status_reg,
                                   1U,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    *p_is_measuring = ((status_reg & BME280_REG_STATUS_MEASURING_BIT) != 0U);
    return STATUS_OK;
}

status_t bme280_wait_for_completion(bme280_dev_t *dev, uint32_t timeout_ms) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint32_t elapsed_ms = 0;
    const uint32_t poll_interval_ms = 5U;

#if defined(HAVE_STM32WLXX_HAL)
    /* Initial nominal sleep delay (20 ms) before polling */
    HAL_Delay(20);
    elapsed_ms += 20U;
#endif

    while (elapsed_ms <= timeout_ms) {
        bool measuring = true;
        status_t status = bme280_is_measuring(dev, &measuring);
        if (status != STATUS_OK) {
            return status;
        }

        if (!measuring) {
            return STATUS_OK; /* Measurement completed */
        }

#if defined(HAVE_STM32WLXX_HAL)
        HAL_Delay(poll_interval_ms);
#endif
        elapsed_ms += poll_interval_ms;
    }

    return STATUS_ERR_TIMEOUT;
}

status_t bme280_read_raw_data(bme280_dev_t *dev, bme280_raw_data_t *p_raw) {
    if (dev == NULL || p_raw == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint8_t raw_buf[BME280_RAW_BURST_DATA_LEN];
    memset(raw_buf, 0, sizeof(raw_buf));

    /* Burst read 8 registers starting at 0xF7 (press_msb) */
    status_t status = i2c_bus_read(dev->i2c_address,
                                   BME280_REG_PRESS_MSB,
                                   raw_buf,
                                   BME280_RAW_BURST_DATA_LEN,
                                   BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* Unpack 20-bit Pressure: raw_buf[0..2] */
    p_raw->adc_P = (int32_t)((((uint32_t)raw_buf[0]) << 12) |
                             (((uint32_t)raw_buf[1]) << 4)  |
                             (((uint32_t)raw_buf[2]) >> 4));

    /* Unpack 20-bit Temperature: raw_buf[3..5] */
    p_raw->adc_T = (int32_t)((((uint32_t)raw_buf[3]) << 12) |
                             (((uint32_t)raw_buf[4]) << 4)  |
                             (((uint32_t)raw_buf[5]) >> 4));

    /* Unpack 16-bit Humidity: raw_buf[6..7] */
    p_raw->adc_H = (int32_t)((((uint32_t)raw_buf[6]) << 8) |
                             ((uint32_t)raw_buf[7]));

    return STATUS_OK;
}

status_t bme280_sample_forced_raw(bme280_dev_t *dev, bme280_raw_data_t *p_raw) {
    if (dev == NULL || p_raw == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* 1. Apply oversampling and filter configuration */
    status_t status = bme280_configure(dev, &dev->config);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Trigger forced-mode single-shot measurement */
    status = bme280_trigger_forced_mode(dev);
    if (status != STATUS_OK) {
        return status;
    }

    /* 3. Wait for conversion to complete */
    status = bme280_wait_for_completion(dev, BME280_MEASUREMENT_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* 4. Burst-read raw ADC registers */
    return bme280_read_raw_data(dev, p_raw);
}

float bme280_compensate_temperature(int32_t adc_T, const bme280_calib_data_t *calib, float *p_t_fine) {
    if (calib == NULL || p_t_fine == NULL) {
        return 0.0f;
    }

    float var1 = (((float)adc_T) / 16384.0f - ((float)calib->dig_T1) / 1024.0f) * ((float)calib->dig_T2);
    float var2 = ((((float)adc_T) / 131072.0f - ((float)calib->dig_T1) / 8192.0f) *
                  (((float)adc_T) / 131072.0f - ((float)calib->dig_T1) / 8192.0f)) * ((float)calib->dig_T3);

    *p_t_fine = var1 + var2;
    return (*p_t_fine) / 5120.0f;
}

float bme280_compensate_pressure(int32_t adc_P, const bme280_calib_data_t *calib, float t_fine) {
    if (calib == NULL) {
        return 0.0f;
    }

    float var1 = (t_fine / 2.0f) - 64000.0f;
    float var2 = var1 * var1 * ((float)calib->dig_P6) / 32768.0f;
    var2 = var2 + var1 * ((float)calib->dig_P5) * 2.0f;
    var2 = (var2 / 4.0f) + (((float)calib->dig_P4) * 65536.0f);
    var1 = (((float)calib->dig_P3) * var1 * var1 / 524288.0f + ((float)calib->dig_P2) * var1) / 524288.0f;
    var1 = (1.0f + var1 / 32768.0f) * ((float)calib->dig_P1);

    if (var1 <= 0.0f) {
        return 0.0f; /* Avoid division by zero */
    }

    float p = 1048576.0f - (float)adc_P;
    p = (p - (var2 / 4096.0f)) * 6250.0f / var1;
    var1 = ((float)calib->dig_P9) * p * p / 2147483648.0f;
    var2 = p * ((float)calib->dig_P8) / 32768.0f;
    p = p + (var1 + var2 + ((float)calib->dig_P7)) / 16.0f;

    float p_hpa = p / 100.0f;

    /* Physical boundary clamping */
    if (p_hpa < BME280_PRESS_MIN_HPA) {
        p_hpa = BME280_PRESS_MIN_HPA;
    } else if (p_hpa > BME280_PRESS_MAX_HPA) {
        p_hpa = BME280_PRESS_MAX_HPA;
    }

    return p_hpa;
}

float bme280_compensate_humidity(int32_t adc_H, const bme280_calib_data_t *calib, float t_fine) {
    if (calib == NULL) {
        return 0.0f;
    }

    float var_H = t_fine - 76800.0f;
    var_H = ((float)adc_H - (((float)calib->dig_H4) * 64.0f + ((float)calib->dig_H5) / 16384.0f * var_H)) *
            (((float)calib->dig_H2) / 65536.0f * (1.0f + ((float)calib->dig_H6) / 67108864.0f * var_H *
            (1.0f + ((float)calib->dig_H3) / 67108864.0f * var_H)));
    var_H = var_H * (1.0f - ((float)calib->dig_H1) * var_H / 524288.0f);

    /* Clamp relative humidity to [0.0%, 100.0%] */
    if (var_H > BME280_HUM_MAX_PERCENT) {
        var_H = BME280_HUM_MAX_PERCENT;
    } else if (var_H < BME280_HUM_MIN_PERCENT) {
        var_H = BME280_HUM_MIN_PERCENT;
    }

    return var_H;
}

status_t bme280_compensate_raw(const bme280_raw_data_t *raw,
                               const bme280_calib_data_t *calib,
                               bme280_data_t *out_data) {
    if (raw == NULL || calib == NULL || out_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    memset(out_data, 0, sizeof(bme280_data_t));

    float t_fine = 0.0f;
    out_data->temperature_c   = bme280_compensate_temperature(raw->adc_T, calib, &t_fine);
    out_data->pressure_hpa    = bme280_compensate_pressure(raw->adc_P, calib, t_fine);
    out_data->humidity_percent = bme280_compensate_humidity(raw->adc_H, calib, t_fine);

    /* Plausibility verification against physical boundaries */
    if (out_data->temperature_c >= BME280_TEMP_MIN_C &&
        out_data->temperature_c <= BME280_TEMP_MAX_C &&
        out_data->pressure_hpa >= BME280_PRESS_MIN_HPA &&
        out_data->pressure_hpa <= BME280_PRESS_MAX_HPA) {
        out_data->is_valid = true;
        return STATUS_OK;
    }

    out_data->is_valid = false;
    return STATUS_ERR_OUT_OF_RANGE;
}

status_t bme280_compensate_raw_fixed(const bme280_raw_data_t *raw,
                                     const bme280_calib_data_t *calib,
                                     bme280_fixed_data_t *out_data) {
    if (raw == NULL || calib == NULL || out_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    memset(out_data, 0, sizeof(bme280_fixed_data_t));

    bme280_data_t float_data;
    status_t status = bme280_compensate_raw(raw, calib, &float_data);
    if (status != STATUS_OK && status != STATUS_ERR_OUT_OF_RANGE) {
        return status;
    }

    out_data->temp_centi_c      = (int16_t)(float_data.temperature_c * 100.0f);
    out_data->press_pascals     = (uint32_t)(float_data.pressure_hpa * 100.0f);
    out_data->hum_centi_percent = (uint16_t)(float_data.humidity_percent * 100.0f);
    out_data->is_valid          = float_data.is_valid;

    return status;
}

status_t bme280_read_data(bme280_dev_t *dev, bme280_data_t *out_data) {
    if (dev == NULL || out_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    bme280_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));

    status_t status = bme280_sample_forced_raw(dev, &raw);
    if (status != STATUS_OK) {
        out_data->is_valid = false;
        return status;
    }

    return bme280_compensate_raw(&raw, &dev->calib, out_data);
}

status_t bme280_soft_reset(bme280_dev_t *dev) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* 1. Write soft reset command 0xB6 to register 0xE0 */
    uint8_t reset_cmd = BME280_SOFT_RESET_KEY;
    status_t status = i2c_bus_write(dev->i2c_address,
                                    BME280_REG_RESET,
                                    &reset_cmd,
                                    1U,
                                    BME280_I2C_TIMEOUT_MS);
    if (status != STATUS_OK) {
        return status;
    }

    /* 2. Wait 5 ms for internal power-on-reset and NVM copy */
    HAL_Delay(BME280_SOFT_RESET_SETTLE_MS);

    /* 3. Re-read calibration coefficients to guarantee memory integrity */
    status = bme280_read_calibration(dev);
    if (status != STATUS_OK) {
        return status;
    }

    g_sat_status.recovery_triggered = true;
    g_sat_status.state = BME280_STATE_SOFT_RECOVERY;

    return STATUS_OK;
}

status_t bme280_process_saturation(bme280_dev_t *dev,
                                   float current_rh,
                                   float temp_delta_1h,
                                   uint16_t ambient_lux,
                                   uint32_t rain_tips_24h) {
    if (dev == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    g_sat_status.recovery_triggered = false;
    g_sat_status.debiased_humidity_pct = current_rh;

    /* 1. Check for High Humidity Saturation Entry / Exit */
    if (current_rh >= BME280_SATURATION_THRESHOLD_RH) {
        if (g_sat_status.saturation_cycles < 65535U) {
            g_sat_status.saturation_cycles++;
        }

        if (g_sat_status.saturation_cycles >= BME280_SATURATION_MIN_CYCLES_1H) {
            g_sat_status.is_saturated = true;
            g_sat_status.state = BME280_STATE_HIGH_HUMIDITY_SAT;
        }
    } else if (current_rh < BME280_SATURATION_HYSTERESIS_RH) {
        /* Hysteresis exit (< 95% RH) */
        g_sat_status.saturation_cycles = 0U;
        g_sat_status.is_saturated = false;
        g_sat_status.condensation_detected = false;
        g_sat_status.state = BME280_STATE_NORMAL;
        return STATUS_OK;
    }

    /* 2. Check for Condensation Creep (Bright Sun + Warming + Saturated) */
    if (g_sat_status.is_saturated &&
        ambient_lux >= BME280_CONDENSATION_MIN_LUX &&
        temp_delta_1h >= BME280_CONDENSATION_MIN_TEMP_RISE) {
        
        g_sat_status.condensation_detected = true;
        g_sat_status.state = BME280_STATE_CONDENSATION_CREEP;

        /* Apply dynamic -1.5% de-biasing offset */
        float debiased = current_rh - BME280_CONDENSATION_DEBIAS_OFFSET;
        if (debiased < BME280_HUM_MIN_PERCENT) {
            debiased = BME280_HUM_MIN_PERCENT;
        }
        g_sat_status.debiased_humidity_pct = debiased;
    } else {
        g_sat_status.condensation_detected = false;
        if (g_sat_status.is_saturated) {
            g_sat_status.state = BME280_STATE_HIGH_HUMIDITY_SAT;
        }
    }

    /* 3. Check for 24-Hour Sustained Saturation without Rain -> Trigger Soft Reset */
    if (g_sat_status.saturation_cycles >= BME280_SATURATION_MAX_CYCLES_24H && rain_tips_24h == 0U) {
        status_t status = bme280_soft_reset(dev);
        if (status != STATUS_OK) {
            return status;
        }
        /* Reset counter after recovery while maintaining saturation tracking baseline */
        g_sat_status.saturation_cycles = BME280_SATURATION_MIN_CYCLES_1H;
    }

    return STATUS_OK;
}

const bme280_saturation_status_t* bme280_get_saturation_status(const bme280_dev_t *dev) {
    if (dev == NULL) {
        return NULL;
    }
    return &g_sat_status;
}

void bme280_reset_saturation_tracking(bme280_dev_t *dev) {
    (void)dev;
    g_sat_status.is_saturated          = false;
    g_sat_status.condensation_detected = false;
    g_sat_status.recovery_triggered    = false;
    g_sat_status.saturation_cycles     = 0U;
    g_sat_status.state                 = BME280_STATE_NORMAL;
    g_sat_status.debiased_humidity_pct = 0.0f;
}
