/**
 * @file    main.h
 * @brief   Master application header and system entry point prototypes.
 */

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "status.h"
#include "board_config.h"
#include "system_clock.h"
#include "watchdog.h"
#include "bsp_indicators.h"
#include "app_state_machine.h"

int main(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
