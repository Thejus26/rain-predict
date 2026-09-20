/**
 * @file    app_fault_handler.c
 * @brief   System fault monitoring, self-healing, and graceful degradation implementation.
 * @details Manages hardware communication recovery, sensor fallbacks, and diagnostic registries.
 *
 * Target: STM32WLE5 (ARM Cortex-M4 @ 48 MHz)
 */

#include "app_fault_handler.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "i2c_bus.h"
#include "bsp_power_rails.h"

/* ========================================================================== */
/* Internal State Context                                                     */
/* ========================================================================== */

static fault_handler_status_t s_fault_ctx;
static bool                   s_initialized = false;

/* ========================================================================== */
/* Public API Implementations                                                 */
/* ========================================================================== */

status_t app_fault_handler_init(void) {
    memset(&s_fault_ctx, 0, sizeof(s_fault_ctx));
    s_fault_ctx.last_valid_pressure_hpa  = FAULT_PRESSURE_NEUTRAL_DEFAULT_HPA; /* 950.0 hPa */
    s_fault_ctx.last_valid_temperature_c = 22.0f;                              /* Standard ambient baseline */
    s_fault_ctx.last_valid_humidity_pct  = 80.0f;                              /* Standard humid highland baseline */
    s_initialized = true;
    return STATUS_OK;
}

void app_fault_handler_report(uint16_t fault_bit, bool success) {
    if (!s_initialized) {
        return;
    }

    if (success) {
        /* Increment self-healing counter */
        if (fault_bit == FAULT_MASK_BME280_COMM) {
            s_fault_ctx.bme280_consecutive_clean++;
            if (s_fault_ctx.bme280_consecutive_clean >= FAULT_MAX_CONSECUTIVE_HEAL) {
                s_fault_ctx.active_fault_mask &= (uint16_t)~FAULT_MASK_BME280_COMM;
                s_fault_ctx.pressure_stale_count = 0U;
            }
        } else if (fault_bit == FAULT_MASK_OPT3001_COMM) {
            s_fault_ctx.opt3001_consecutive_clean++;
            if (s_fault_ctx.opt3001_consecutive_clean >= FAULT_MAX_CONSECUTIVE_HEAL) {
                s_fault_ctx.active_fault_mask &= (uint16_t)~FAULT_MASK_OPT3001_COMM;
            }
        } else {
            s_fault_ctx.active_fault_mask &= (uint16_t)~fault_bit;
        }
    } else {
        /* Latch active fault and increment failure metrics */
        s_fault_ctx.active_fault_mask  |= fault_bit;
        s_fault_ctx.latched_fault_mask |= fault_bit;

        if (fault_bit == FAULT_MASK_BME280_COMM) {
            s_fault_ctx.bme280_fail_count++;
            s_fault_ctx.bme280_consecutive_clean = 0U;
        } else if (fault_bit == FAULT_MASK_OPT3001_COMM) {
            s_fault_ctx.opt3001_fail_count++;
            s_fault_ctx.opt3001_consecutive_clean = 0U;
        } else if (fault_bit == FAULT_MASK_I2C_BUS_LOCKUP) {
            s_fault_ctx.i2c_lockup_count++;
        } else if (fault_bit == FAULT_MASK_LORA_TX_TIMEOUT) {
            s_fault_ctx.lora_timeout_count++;
        } else if (fault_bit == FAULT_MASK_FLASH_WRITE) {
            s_fault_ctx.flash_error_count++;
        }
    }
}

void app_fault_handler_set_last_valid_bme280(float temp_c, float rh_pct, float press_hpa) {
    if (!s_initialized) {
        return;
    }
    s_fault_ctx.last_valid_temperature_c = temp_c;
    s_fault_ctx.last_valid_humidity_pct  = rh_pct;
    s_fault_ctx.last_valid_pressure_hpa  = press_hpa;
    s_fault_ctx.pressure_stale_count     = 0U;
}

status_t app_fault_handler_recover_i2c_bus(void) {
    if (!s_initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

    /* Record that an I2C lockup incident occurred */
    s_fault_ctx.i2c_lockup_count++;
    s_fault_ctx.latched_fault_mask |= FAULT_MASK_I2C_BUS_LOCKUP;

    /* 1. Attempt 9-clock bus cycling */
    status_t rc = i2c_bus_recover();
    if (rc == STATUS_OK) {
        s_fault_ctx.active_fault_mask &= (uint16_t)~FAULT_MASK_I2C_BUS_LOCKUP;
        return STATUS_OK;
    }

    /* 2. Hard rail reset if 9-clock cycle failed */
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, false);
    (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_SENSORS);
    (void)bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, true);
    (void)bsp_power_rail_stabilize(BSP_POWER_RAIL_SENSORS);

    rc = i2c_bus_recover();
    if (rc == STATUS_OK) {
        s_fault_ctx.active_fault_mask &= (uint16_t)~FAULT_MASK_I2C_BUS_LOCKUP;
    } else {
        s_fault_ctx.active_fault_mask |= FAULT_MASK_I2C_BUS_LOCKUP;
    }
    return rc;
}

bool app_fault_handler_get_bme280_fallback(float *p_temp_c, float *p_rh_pct, float *p_press) {
    if (p_temp_c == NULL || p_rh_pct == NULL || p_press == NULL) {
        return false;
    }

    if (!s_initialized) {
        *p_temp_c = FAULT_TEMP_NEUTRAL_DEFAULT_C;
        *p_rh_pct = FAULT_HUM_NEUTRAL_DEFAULT_PCT;
        *p_press  = FAULT_PRESSURE_NEUTRAL_DEFAULT_HPA;
        return false;
    }

    /* Check if stale cache is still within allowable window (3 cycles) */
    if (s_fault_ctx.pressure_stale_count < FAULT_MAX_PRESSURE_STALE_CYCLES) {
        *p_temp_c = s_fault_ctx.last_valid_temperature_c;
        *p_rh_pct = s_fault_ctx.last_valid_humidity_pct;
        *p_press  = s_fault_ctx.last_valid_pressure_hpa;
        s_fault_ctx.pressure_stale_count++;
        return true;
    }

    /* Fallback to neutral default values */
    *p_temp_c = FAULT_TEMP_NEUTRAL_DEFAULT_C;
    *p_rh_pct = FAULT_HUM_NEUTRAL_DEFAULT_PCT;
    *p_press  = FAULT_PRESSURE_NEUTRAL_DEFAULT_HPA;
    return false;
}

void app_fault_handler_get_opt3001_fallback(uint8_t rtc_hour, float *p_lux) {
    if (p_lux == NULL) {
        return;
    }

    /* Daytime estimate (06:00 to 18:00) */
    if (rtc_hour >= 6U && rtc_hour <= 18U) {
        *p_lux = FAULT_LUX_DAYTIME_ESTIMATE;  /* Neutral diffuse sunlight */
    } else {
        *p_lux = FAULT_LUX_NIGHTTIME_ESTIMATE; /* Nighttime darkness */
    }
}

bool app_fault_handler_is_system_fault_active(void) {
    if (!s_initialized) {
        return false;
    }
    uint16_t critical_mask = (uint16_t)(FAULT_MASK_BME280_COMM |
                                       FAULT_MASK_OPT3001_COMM |
                                       FAULT_MASK_I2C_BUS_LOCKUP);
    return ((s_fault_ctx.active_fault_mask & critical_mask) != 0U);
}

status_t app_fault_handler_get_status(fault_handler_status_t *p_status) {
    if (p_status == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    if (!s_initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }
    *p_status = s_fault_ctx;
    return STATUS_OK;
}

void app_fault_handler_reset(void) {
    (void)app_fault_handler_init();
}
