/**
 * @file    rain_gauge_driver.c
 * @brief   Tipping-bucket rain gauge driver implementation for STM32WLE5 SoC.
 * @details Handles EXTI0 GPIO falling-edge interrupts, dual-stage chatter rejection,
 *          multi-horizon atomic accumulation registers, instantaneous and interval rain rate
 *          math, peak tracking, meteorological classification, and LoRaWAN Byte 8 telemetry.
 */

#include "rain_gauge_driver.h"
#include "board_config.h"
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)
#include "stm32wlxx_hal.h"
#else
static uint32_t s_mock_primask = 0U;
#define __disable_irq() ((void)(s_mock_primask = 1U))
#define __enable_irq()  ((void)(s_mock_primask = 0U))
#endif

/* ========================================================================== */
/* Private Static Driver State                                                */
/* ========================================================================== */

static volatile uint32_t s_last_pulse_timestamp_ms = 0U;
static volatile uint32_t s_inter_tip_delta_ms      = 0U;
static volatile uint32_t s_rejected_bounce_count   = 0U;
static volatile bool     s_rain_active_flag        = false;
static volatile bool     s_has_first_pulse         = false;
static bool              s_is_initialized          = false;
static bool              s_irq_enabled             = false;

/* Multi-Horizon Volatile Accumulators */
static volatile uint16_t s_interval_tips           = 0U;
static volatile uint32_t s_daily_tips              = 0U;
static volatile uint32_t s_total_lifetime_tips     = 0U;

/* Rolling 1-Hour FIFO Ring Buffer (6 x 10m intervals) */
static uint16_t          s_hourly_fifo[RAIN_GAUGE_HOURLY_FIFO_SIZE] = {0U};
static uint8_t           s_hourly_fifo_index       = 0U;

/* Peak Rate Tracking Registers */
static float             s_peak_instantaneous_mm_hr = 0.0f;
static float             s_peak_interval_mm_hr      = 0.0f;

/* ========================================================================== */
/* Private Helper Functions                                                   */
/* ========================================================================== */

static uint32_t get_rolling_hourly_tips(void) {
    uint32_t sum = 0U;
    for (uint8_t i = 0U; i < RAIN_GAUGE_HOURLY_FIFO_SIZE; i++) {
        sum += s_hourly_fifo[i];
    }
    /* Include current pending interval tips */
    sum += s_interval_tips;
    return sum;
}

/* ========================================================================== */
/* Driver Lifecycle & Configuration                                           */
/* ========================================================================== */

status_t rain_gauge_init(void) {
#if defined(HAVE_STM32WLXX_HAL)
    /* 1. Enable GPIOA Peripheral Clock */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2. Configure PA0 as Falling-Edge EXTI Input with Pull-Up */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin   = PIN_RAIN_GAUGE_PIN;
    gpio_init.Mode  = GPIO_MODE_IT_FALLING;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PIN_RAIN_GAUGE_PORT, &gpio_init);

    /* 3. Configure NVIC for EXTI0 */
    HAL_NVIC_SetPriority((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn,
                         RAIN_GAUGE_NVIC_PREEMPT_PRIO,
                         RAIN_GAUGE_NVIC_SUB_PRIO);
    HAL_NVIC_EnableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);
#endif

    s_last_pulse_timestamp_ms = 0U;
    s_inter_tip_delta_ms      = 0U;
    s_rejected_bounce_count   = 0U;
    s_rain_active_flag        = false;
    s_has_first_pulse         = false;
    s_is_initialized          = true;
    s_irq_enabled             = true;

    rain_gauge_reset_all_accumulators();
    rain_gauge_reset_peak_rates();

    return STATUS_OK;
}

status_t rain_gauge_deinit(void) {
#if defined(HAVE_STM32WLXX_HAL)
    /* 1. Disable NVIC Interrupt */
    HAL_NVIC_DisableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);

    /* 2. Tri-state PA0 to Analog mode to eliminate deep sleep leakage */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin   = PIN_RAIN_GAUGE_PIN;
    gpio_init.Mode  = GPIO_MODE_ANALOG;
    gpio_init.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(PIN_RAIN_GAUGE_PORT, &gpio_init);
#endif

    s_is_initialized = false;
    s_irq_enabled    = false;

    return STATUS_OK;
}

status_t rain_gauge_enable_irq(bool enable) {
    if (!s_is_initialized) {
        return STATUS_ERR_NOT_INITIALIZED;
    }

#if defined(HAVE_STM32WLXX_HAL)
    if (enable) {
        HAL_NVIC_EnableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);
    } else {
        HAL_NVIC_DisableIRQ((IRQn_Type)PIN_RAIN_GAUGE_EXTI_IRQn);
    }
#endif

    s_irq_enabled = enable;
    return STATUS_OK;
}

/* ========================================================================== */
/* Interrupt Service Routine & Debounce Filter                                */
/* ========================================================================== */

void rain_gauge_exti_isr(uint32_t current_tick_ms) {
    if (!s_is_initialized) {
        return;
    }

    if (!s_has_first_pulse) {
        /* First registered physical bucket tip */
        s_last_pulse_timestamp_ms = current_tick_ms;
        s_inter_tip_delta_ms      = 0U;
        s_rain_active_flag        = true;
        s_has_first_pulse         = true;

        if (s_interval_tips < 65535U) {
            s_interval_tips++;
        }
        if (s_daily_tips < 0xFFFFFFFFUL) {
            s_daily_tips++;
        }
        if (s_total_lifetime_tips < 0xFFFFFFFFUL) {
            s_total_lifetime_tips++;
        }
        return;
    }

    /* 32-bit unsigned arithmetic handles SysTick timer rollover seamlessly */
    uint32_t elapsed_ms = current_tick_ms - s_last_pulse_timestamp_ms;

    if (elapsed_ms >= RAIN_GAUGE_DEBOUNCE_MS) {
        /* Valid physical bucket tip (>= 50ms lockout interval) */
        s_inter_tip_delta_ms      = elapsed_ms;
        s_last_pulse_timestamp_ms = current_tick_ms;
        s_rain_active_flag        = true;

        if (s_interval_tips < 65535U) {
            s_interval_tips++;
        }
        if (s_daily_tips < 0xFFFFFFFFUL) {
            s_daily_tips++;
        }
        if (s_total_lifetime_tips < 0xFFFFFFFFUL) {
            s_total_lifetime_tips++;
        }

        /* Calculate and update peak instantaneous rate upon valid tip event */
        float tip_rate = RAIN_RATE_NUMERATOR_MS / (float)elapsed_ms;
        if (tip_rate > RAIN_RATE_MAX_MM_HR) {
            tip_rate = RAIN_RATE_MAX_MM_HR;
        }
        if (tip_rate > s_peak_instantaneous_mm_hr) {
            s_peak_instantaneous_mm_hr = tip_rate;
        }
    } else {
        /* Spurious contact bounce / chatter spike (< 50ms) rejected */
        s_rejected_bounce_count++;
    }
}

/* ========================================================================== */
/* Accumulation & Telemetry APIs                                              */
/* ========================================================================== */

uint16_t rain_gauge_read_and_clear_interval(float *p_interval_mm) {
    uint16_t tips = 0U;

    /* Atomic Read-and-Clear Critical Section (< 6 cycles) */
    __disable_irq();
    tips = s_interval_tips;
    s_interval_tips = 0U;
    __enable_irq();

    if (p_interval_mm != NULL) {
        *p_interval_mm = (float)tips * RAIN_GAUGE_CALIB_MM_PER_TIP;
    }

    return tips;
}

void rain_gauge_update_hourly_history(uint16_t interval_tips) {
    s_hourly_fifo[s_hourly_fifo_index] = interval_tips;
    s_hourly_fifo_index = (uint8_t)((s_hourly_fifo_index + 1U) % RAIN_GAUGE_HOURLY_FIFO_SIZE);

    /* Update peak interval rate on completed interval */
    float rate = ((float)interval_tips * RAIN_RATE_NUMERATOR_SEC) / (float)RAIN_GAUGE_INTERVAL_DURATION_SEC;
    if (rate > RAIN_RATE_MAX_MM_HR) {
        rate = RAIN_RATE_MAX_MM_HR;
    }
    if (rate > s_peak_interval_mm_hr) {
        s_peak_interval_mm_hr = rate;
    }
}

uint8_t rain_gauge_encode_telemetry_byte(uint16_t interval_tips) {
    if (interval_tips >= RAIN_GAUGE_TELEMETRY_MAX_TIPS) {
        return (uint8_t)RAIN_GAUGE_TELEMETRY_MAX_TIPS;
    }
    return (uint8_t)interval_tips;
}

status_t rain_gauge_get_accumulation(rain_gauge_data_t *p_data) {
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t cur_interval = 0U;
    uint32_t cur_daily    = 0U;
    uint32_t cur_total    = 0U;

    /* Atomic copy of volatile counters */
    __disable_irq();
    cur_interval = s_interval_tips;
    cur_daily    = s_daily_tips;
    cur_total    = s_total_lifetime_tips;
    __enable_irq();

    uint32_t hourly_sum = get_rolling_hourly_tips();

    p_data->interval_tips       = cur_interval;
    p_data->interval_rain_mm    = (float)cur_interval * RAIN_GAUGE_CALIB_MM_PER_TIP;
    p_data->hourly_tips         = hourly_sum;
    p_data->hourly_rain_mm      = (float)hourly_sum * RAIN_GAUGE_CALIB_MM_PER_TIP;
    p_data->daily_tips          = cur_daily;
    p_data->daily_rain_mm       = (float)cur_daily * RAIN_GAUGE_CALIB_MM_PER_TIP;
    p_data->total_lifetime_tips = cur_total;
    p_data->telemetry_byte8     = rain_gauge_encode_telemetry_byte(cur_interval);

    return STATUS_OK;
}

void rain_gauge_reset_daily(void) {
    __disable_irq();
    s_daily_tips = 0U;
    __enable_irq();
}

void rain_gauge_reset_all_accumulators(void) {
    __disable_irq();
    s_interval_tips       = 0U;
    s_daily_tips          = 0U;
    s_total_lifetime_tips = 0U;
    __enable_irq();

    (void)memset(s_hourly_fifo, 0, sizeof(s_hourly_fifo));
    s_hourly_fifo_index = 0U;
}

/* ========================================================================== */
/* Rain Rate Calculation & Classification                                     */
/* ========================================================================== */

float rain_gauge_get_instantaneous_rate(uint32_t current_tick_ms) {
    uint32_t last_tick = 0U;
    uint32_t delta_ms  = 0U;
    bool     has_tip   = false;

    /* Thread-safe snapshot of timing variables */
    __disable_irq();
    last_tick = s_last_pulse_timestamp_ms;
    delta_ms  = s_inter_tip_delta_ms;
    has_tip   = s_has_first_pulse;
    __enable_irq();

    if (!has_tip || (delta_ms == 0U)) {
        return 0.0f;
    }

    uint32_t elapsed_since_last = current_tick_ms - last_tick;

    /* Inactivity timeout check (15 minutes of zero tips) */
    if (elapsed_since_last >= RAIN_RATE_INACTIVITY_TIMEOUT_MS) {
        return 0.0f;
    }

    /* Baseline rate from last inter-tip period (0.20 mm * 3,600,000 / delta_ms) */
    float base_rate = RAIN_RATE_NUMERATOR_MS / (float)delta_ms;

    /* Apply time-decay aging if elapsed time exceeds last delta */
    if (elapsed_since_last > delta_ms) {
        float decayed_rate = RAIN_RATE_NUMERATOR_MS / (float)elapsed_since_last;
        if (decayed_rate < base_rate) {
            base_rate = decayed_rate;
        }
    }

    /* Clamp to physical sensor limit: [0.0, 300.0] mm/hr */
    if (base_rate > RAIN_RATE_MAX_MM_HR) {
        base_rate = RAIN_RATE_MAX_MM_HR;
    } else if (base_rate < 0.0f) {
        base_rate = 0.0f;
    }

    return base_rate;
}

float rain_gauge_compute_interval_rate(uint16_t interval_tips, uint32_t interval_duration_sec) {
    if (interval_duration_sec == 0U) {
        return 0.0f;
    }

    float rate = ((float)interval_tips * RAIN_RATE_NUMERATOR_SEC) / (float)interval_duration_sec;

    if (rate > RAIN_RATE_MAX_MM_HR) {
        rate = RAIN_RATE_MAX_MM_HR;
    } else if (rate < 0.0f) {
        rate = 0.0f;
    }

    return rate;
}

rain_intensity_t rain_gauge_classify_intensity(float rain_rate_mm_hr) {
    if (rain_rate_mm_hr < RAIN_THRESH_DRIZZLE_MIN_MM_HR) {
        return RAIN_INTENSITY_NONE;
    } else if (rain_rate_mm_hr < RAIN_THRESH_LIGHT_MIN_MM_HR) {
        return RAIN_INTENSITY_DRIZZLE;
    } else if (rain_rate_mm_hr < RAIN_THRESH_MODERATE_MIN_MM_HR) {
        return RAIN_INTENSITY_LIGHT;
    } else if (rain_rate_mm_hr < RAIN_THRESH_HEAVY_MIN_MM_HR) {
        return RAIN_INTENSITY_MODERATE;
    } else if (rain_rate_mm_hr < RAIN_THRESH_TORRENTIAL_MIN_MM_HR) {
        return RAIN_INTENSITY_HEAVY;
    } else if (rain_rate_mm_hr < RAIN_THRESH_CLOUDBURST_MIN_MM_HR) {
        return RAIN_INTENSITY_TORRENTIAL;
    } else {
        return RAIN_INTENSITY_CLOUDBURST;
    }
}

const char *rain_gauge_intensity_to_str(rain_intensity_t intensity) {
    switch (intensity) {
        case RAIN_INTENSITY_NONE:       return "None";
        case RAIN_INTENSITY_DRIZZLE:     return "Drizzle";
        case RAIN_INTENSITY_LIGHT:       return "Light";
        case RAIN_INTENSITY_MODERATE:    return "Moderate";
        case RAIN_INTENSITY_HEAVY:       return "Heavy";
        case RAIN_INTENSITY_TORRENTIAL:  return "Torrential";
        case RAIN_INTENSITY_CLOUDBURST:  return "Cloudburst";
        default:                         return "Unknown";
    }
}

bool rain_gauge_should_accelerate_sampling(float rain_rate_mm_hr) {
    return (rain_rate_mm_hr >= RAIN_RATE_ACCELERATION_THRESH_MM_HR);
}

status_t rain_gauge_get_rate_metrics(uint32_t current_tick_ms,
                                     uint32_t interval_duration_sec,
                                     rain_rate_metrics_t *p_metrics) {
    if (p_metrics == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint16_t cur_interval_tips = 0U;
    uint32_t last_tick         = 0U;
    uint32_t delta_ms          = 0U;

    __disable_irq();
    cur_interval_tips = s_interval_tips;
    last_tick         = s_last_pulse_timestamp_ms;
    delta_ms          = s_inter_tip_delta_ms;
    __enable_irq();

    p_metrics->last_tip_tick_ms   = last_tick;
    p_metrics->inter_tip_delta_ms = delta_ms;

    float inst_rate = rain_gauge_get_instantaneous_rate(current_tick_ms);
    float int_rate  = rain_gauge_compute_interval_rate(cur_interval_tips, interval_duration_sec);
    float hour_rate = (float)get_rolling_hourly_tips() * RAIN_GAUGE_CALIB_MM_PER_TIP;

    p_metrics->instantaneous_rate_mm_hr = inst_rate;
    p_metrics->interval_rate_mm_hr      = int_rate;
    p_metrics->hourly_rate_mm_hr        = hour_rate;
    p_metrics->peak_instantaneous_mm_hr = s_peak_instantaneous_mm_hr;
    p_metrics->peak_interval_mm_hr      = s_peak_interval_mm_hr;

    float active_rate = (inst_rate > int_rate) ? inst_rate : int_rate;
    p_metrics->current_intensity        = rain_gauge_classify_intensity(active_rate);

    return STATUS_OK;
}

void rain_gauge_reset_peak_rates(void) {
    s_peak_instantaneous_mm_hr = 0.0f;
    s_peak_interval_mm_hr      = 0.0f;
}

/* ========================================================================== */
/* Diagnostics & Telemetry Accessors                                          */
/* ========================================================================== */

bool rain_gauge_is_rain_active(void) {
    return s_rain_active_flag;
}

uint32_t rain_gauge_get_last_pulse_timestamp(void) {
    return s_last_pulse_timestamp_ms;
}

uint32_t rain_gauge_get_rejected_bounce_count(void) {
    return s_rejected_bounce_count;
}

void rain_gauge_reset_diagnostics(void) {
    s_rejected_bounce_count = 0U;
    s_rain_active_flag      = false;
}
