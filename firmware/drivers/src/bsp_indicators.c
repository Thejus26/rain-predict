/**
 * @file    bsp_indicators.c
 * @brief   Implementation of Board Indicator and Alarm Actuator Driver.
 * @details Target driver for STM32WLE5 Status LEDs (PB8/PB9), Buzzer (PB2), and Siren Relay (PB4)
 *          with non-blocking pattern generator and host simulation backend.
 */

#include "bsp_indicators.h"
#include <string.h>

typedef struct {
    bsp_indicator_pattern_t pattern;
    uint32_t                pattern_timer_ms;
    uint32_t                relay_timer_ms;
    bool                    relay_active;
    bool                    buzzer_active;
    bool                    led_states[BSP_LED_MAX];
} indicator_state_t;

static indicator_state_t s_state;

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

status_t bsp_indicators_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {0};

    /* Configure PB8 (LED_OK), PB9 (LED_WARN), PB2 (BUZZER), PB4 (RELAY) */
    HAL_GPIO_WritePin(GPIOB, PIN_LED_OK_PIN | PIN_LED_WARN_PIN | PIN_BUZZER_PIN | PIN_RELAY_PIN, GPIO_PIN_RESET);
    gpio_init.Pin   = PIN_LED_OK_PIN | PIN_LED_WARN_PIN | PIN_BUZZER_PIN | PIN_RELAY_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    (void)memset(&s_state, 0, sizeof(s_state));
    s_state.pattern = BSP_INDICATOR_PATTERN_OFF;

    return STATUS_OK;
}

void bsp_led_set(bsp_led_t led, bool state) {
    if (led >= BSP_LED_MAX) {
        return;
    }
    s_state.led_states[led] = state;
    GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;

    if (led == BSP_LED_GREEN) {
        HAL_GPIO_WritePin(PIN_LED_OK_PORT, PIN_LED_OK_PIN, pin_state);
    } else if (led == BSP_LED_RED) {
        HAL_GPIO_WritePin(PIN_LED_WARN_PORT, PIN_LED_WARN_PIN, pin_state);
    }
}

void bsp_led_toggle(bsp_led_t led) {
    if (led < BSP_LED_MAX) {
        bsp_led_set(led, !s_state.led_states[led]);
    }
}

bool bsp_led_get(bsp_led_t led) {
    if (led >= BSP_LED_MAX) {
        return false;
    }
    return s_state.led_states[led];
}

void bsp_buzzer_set(bool state) {
    s_state.buzzer_active = state;
    HAL_GPIO_WritePin(PIN_BUZZER_PORT, PIN_BUZZER_PIN, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool bsp_buzzer_get(void) {
    return s_state.buzzer_active;
}

void bsp_relay_set(bool state) {
    s_state.relay_active = state;
    if (!state) {
        s_state.relay_timer_ms = 0U;
    }
    HAL_GPIO_WritePin(PIN_RELAY_PORT, PIN_RELAY_PIN, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool bsp_relay_get(void) {
    return s_state.relay_active;
}

#else

/* ============================================================================
 * Host Simulation Backend
 * ============================================================================ */

void bsp_indicators_test_reset(void) {
    (void)memset(&s_state, 0, sizeof(s_state));
    s_state.pattern = BSP_INDICATOR_PATTERN_OFF;
}

uint32_t bsp_indicators_test_get_relay_timer_ms(void) {
    return s_state.relay_timer_ms;
}

uint32_t bsp_indicators_test_get_pattern_timer_ms(void) {
    return s_state.pattern_timer_ms;
}

status_t bsp_indicators_init(void) {
    bsp_indicators_test_reset();
    return STATUS_OK;
}

void bsp_led_set(bsp_led_t led, bool state) {
    if (led < BSP_LED_MAX) {
        s_state.led_states[led] = state;
    }
}

void bsp_led_toggle(bsp_led_t led) {
    if (led < BSP_LED_MAX) {
        s_state.led_states[led] = !s_state.led_states[led];
    }
}

bool bsp_led_get(bsp_led_t led) {
    if (led >= BSP_LED_MAX) {
        return false;
    }
    return s_state.led_states[led];
}

void bsp_buzzer_set(bool state) {
    s_state.buzzer_active = state;
}

bool bsp_buzzer_get(void) {
    return s_state.buzzer_active;
}

void bsp_relay_set(bool state) {
    s_state.relay_active = state;
    if (!state) {
        s_state.relay_timer_ms = 0U;
    }
}

bool bsp_relay_get(void) {
    return s_state.relay_active;
}

#endif /* HAVE_STM32WLXX_HAL */

/* ============================================================================
 * Common High-Level Pattern & Timed Relay Logic
 * ============================================================================ */

status_t bsp_relay_trigger_timed(uint32_t duration_ms) {
    if (duration_ms == 0U) {
        bsp_relay_set(false);
        return STATUS_OK;
    }

    /* Clamp pulse duration to maximum allowable hardware protection limit */
    uint32_t pulse_len = (duration_ms > BSP_RELAY_MAX_PULSE_DURATION_MS) ?
                          BSP_RELAY_MAX_PULSE_DURATION_MS : duration_ms;

    s_state.relay_timer_ms = pulse_len;
    bsp_relay_set(true);
    return STATUS_OK;
}

status_t bsp_indicator_set_pattern(bsp_indicator_pattern_t pattern) {
    if (pattern > BSP_INDICATOR_PATTERN_STORM_ALERT) {
        return STATUS_ERR_INVALID_PARAM;
    }

    s_state.pattern          = pattern;
    s_state.pattern_timer_ms = 0U;

    if (pattern == BSP_INDICATOR_PATTERN_OFF) {
        (void)bsp_indicators_all_off();
    }
    return STATUS_OK;
}

bsp_indicator_pattern_t bsp_indicator_get_pattern(void) {
    return s_state.pattern;
}

void bsp_indicators_process(uint32_t delta_ms) {
    /* 1. Handle timed relay auto-cutoff countdown */
    if (s_state.relay_active) {
        if (delta_ms >= s_state.relay_timer_ms) {
            bsp_relay_set(false);
        } else {
            s_state.relay_timer_ms -= delta_ms;
        }
    }

    /* 2. Process non-blocking indicator patterns */
    s_state.pattern_timer_ms += delta_ms;

    switch (s_state.pattern) {
        case BSP_INDICATOR_PATTERN_HEARTBEAT: {
            /* 50ms Green pulse every 5000ms */
            uint32_t cycle = s_state.pattern_timer_ms % 5000U;
            bsp_led_set(BSP_LED_GREEN, (cycle < 50U));
            bsp_led_set(BSP_LED_RED, false);
            bsp_buzzer_set(false);
            break;
        }

        case BSP_INDICATOR_PATTERN_WATCH: {
            /* 500ms Amber blink (Green + Red simultaneous) */
            uint32_t cycle = s_state.pattern_timer_ms % 1000U;
            bool on = (cycle < 500U);
            bsp_led_set(BSP_LED_GREEN, on);
            bsp_led_set(BSP_LED_RED, on);
            /* Chirp for 50ms at start of each 10s cycle */
            uint32_t sound_cycle = s_state.pattern_timer_ms % 10000U;
            bsp_buzzer_set(sound_cycle < 50U);
            break;
        }

        case BSP_INDICATOR_PATTERN_WARNING: {
            /* 200ms Red blink */
            uint32_t cycle = s_state.pattern_timer_ms % 400U;
            bsp_led_set(BSP_LED_GREEN, false);
            bsp_led_set(BSP_LED_RED, (cycle < 200U));
            /* Double chirp: 0-60ms ON, 60-120ms OFF, 120-180ms ON every 2s */
            uint32_t sound_cycle = s_state.pattern_timer_ms % 2000U;
            bool chirp = (sound_cycle < 60U) || (sound_cycle >= 120U && sound_cycle < 180U);
            bsp_buzzer_set(chirp);
            break;
        }

        case BSP_INDICATOR_PATTERN_STORM_ALERT: {
            /* 100ms Red rapid strobe */
            uint32_t cycle = s_state.pattern_timer_ms % 200U;
            bsp_led_set(BSP_LED_GREEN, false);
            bsp_led_set(BSP_LED_RED, (cycle < 100U));
            bsp_buzzer_set(cycle < 100U);
            break;
        }

        case BSP_INDICATOR_PATTERN_OFF:
        default:
            bsp_led_set(BSP_LED_GREEN, false);
            bsp_led_set(BSP_LED_RED, false);
            bsp_buzzer_set(false);
            break;
    }
}

status_t bsp_indicators_all_off(void) {
    bsp_led_set(BSP_LED_GREEN, false);
    bsp_led_set(BSP_LED_RED, false);
    bsp_buzzer_set(false);
    bsp_relay_set(false);
    s_state.pattern          = BSP_INDICATOR_PATTERN_OFF;
    s_state.pattern_timer_ms = 0U;
    return STATUS_OK;
}
