/**
 * @file    main.c
 * @brief   System reset entry point and primary application dispatcher loop.
 * @details Initializes hardware, arms watchdog, and dispatches the 8-state cyclic engine.
 */

#include "main.h"

int main(void) {
    /* 1. Initialize Core Board GPIOs & System Clocks */
    (void)board_gpio_init();
    (void)system_clock_init();

    /* 2. Initialize Watchdog with 8-second hardware timeout */
    (void)watchdog_init(WATCHDOG_TIMEOUT_MS_DEFAULT);

    /* 3. Initialize Application State Machine Subsystems */
    if (app_state_machine_init() != STATUS_OK) {
        /* Hardware fault fallback: Safe LED blink loop */
        while (1) {
            bsp_led_toggle(BSP_LED_RED);
            watchdog_refresh();
#if defined(HAVE_STM32WLXX_HAL)
            HAL_Delay(500U);
#endif
        }
    }

    /* 4. Primary Autonomous Cyclic Dispatcher Loop */
    while (1) {
        /* Execute one full 8-state cycle (WAKE -> POWER_ON -> SAMPLE ->
         * FILTER -> PREDICT -> TRANSMIT -> ALERT -> SLEEP) */
        (void)app_state_machine_step();
    }

    return 0;
}
