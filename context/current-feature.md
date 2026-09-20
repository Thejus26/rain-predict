# Current Feature: S6-T3.2 - Graceful Degradation & Fault Tolerance Paths

## Status

In Progress

## Goals

- Implement `firmware/app/inc/app_fault_handler.h` header with fault bitmasks (`FAULT_MASK_BME280_COMM`, `FAULT_MASK_OPT3001_COMM`, `FAULT_MASK_I2C_BUS_LOCKUP`, `FAULT_MASK_RAIN_GAUGE_CHATTER`, `FAULT_MASK_MODBUS_COMM`, `FAULT_MASK_SDI12_COMM`, `FAULT_MASK_FLASH_WRITE`, `FAULT_MASK_LORA_TX_TIMEOUT`, `FAULT_MASK_BATTERY_LOW`, `FAULT_MASK_BATTERY_CRITICAL`), configuration constants (`FAULT_MAX_CONSECUTIVE_HEAL`, `FAULT_MAX_PRESSURE_STALE_CYCLES`), and `fault_handler_status_t` diagnostic structure.
- Define public API prototypes in `app_fault_handler.h`: `app_fault_handler_init()`, `app_fault_handler_report()`, `app_fault_handler_recover_i2c_bus()`, `app_fault_handler_get_bme280_fallback()`, `app_fault_handler_get_opt3001_fallback()`, `app_fault_handler_is_system_fault_active()`, and `app_fault_handler_get_status()`.
- Implement `firmware/app/src/app_fault_handler.c` with internal static status context, fault reporting & bitmask tracking, lifetime failure counters, and 3-cycle auto-clearing self-healing mechanism.
- Implement autonomous 2-tier I2C bus recovery in `app_fault_handler_recover_i2c_bus()`: primary 9-clock bus cycling (`i2c_bus_recover()`), followed by sensor rail power cycle fallback (`bsp_power_rail_enable(BSP_POWER_RAIL_SENSORS, false/true)` with `bsp_power_rail_stabilize()`).
- Implement sensor fallback strategies in `app_fault_handler.c`:
  - BME280: Hold last valid barometric pressure, temperature, and humidity for up to 3 cycles; fallback to neutral baseline ($20.0^\circ\text{C}$, $70.0\%$, $950.0\text{ hPa}$) if persistent outage.
  - OPT3001: Daytime RTC estimate ($25,000\text{ Lux}$ between 06:00 and 18:00, $0\text{ Lux}$ at night) using `power_mgr_get_rtc_hour()`.
- Integrate graceful degradation hooks into `firmware/app/src/app_state_machine.c`:
  - In `app_exec_sample()`: wrap BME280 and OPT3001 reads with fault reporting and fallback value injection; trigger I2C bus recovery on BME280 read failure; propagate `app_fault_handler_is_system_fault_active()` to `s_app_ctx.sensor_fault`.
  - In `app_exec_transmit()`: assert telemetry byte 11 bit 5 (`TELEMETRY_STATUS_SYS_ERROR_MASK`) when sensor fault is active; handle LoRa TX timeout or Flash push failures gracefully without stalling state transitions.
  - In `app_exec_alert()`: display `ALERT_LED_PATTERN_SYSTEM_FAULT` when system fault is active.
- Verify zero-stall mandate: ensure all sensor reads, peripheral transactions, and state transitions complete within bounded timeouts ($< 150\text{ ms}$) and maintain guaranteed entry into low-power Stop 2 deep sleep ($< 3.0\,\mu\text{A}$).
- Implement unit tests in `tests/unit/test_app_fault_handler.c` validating fault bitmask latching, self-healing auto-clearing, I2C recovery sequence, and fallback calculations.

## Notes

- **Specification**: [`context/specs/s6-t3.2-graceful-degradation.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s6-t3.2-graceful-degradation.md)
- **Target Files**:
  - [`firmware/app/inc/app_fault_handler.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/app_fault_handler.h)
  - [`firmware/app/src/app_fault_handler.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_fault_handler.c)
  - [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c)
- **Discovered Module Dependencies (via Knowledge Graph)**:
  - [`firmware/core/inc/status.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/status.h) (`status_t`, `STATUS_OK`, `STATUS_ERR_*`)
  - [`firmware/drivers/inc/i2c_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/i2c_bus.h) (`i2c_bus_recover()`)
  - [`firmware/drivers/inc/bsp_power_rails.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/bsp_power_rails.h) (`bsp_power_rail_enable()`, `bsp_power_rail_stabilize()`, `BSP_POWER_RAIL_SENSORS`)
  - [`firmware/drivers/inc/bme280.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/bme280.h) (`bme280_read_forced_burst()`, `bme280_data_t`)
  - [`firmware/drivers/inc/opt3001.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/opt3001.h) (`opt3001_read_lux_single_shot()`)
  - [`firmware/middleware/inc/power_mgr.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/power_mgr.h) (`power_mgr_get_rtc_hour()`)
  - [`firmware/middleware/inc/alert_manager.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/alert_manager.h) (`ALERT_LED_PATTERN_SYSTEM_FAULT`, `ALERT_ACOUSTIC_FAULT_TONE`)
  - [`firmware/middleware/inc/telemetry_protocol.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/telemetry_protocol.h) (`TELEMETRY_STATUS_SYS_ERROR_MASK`)
- **Hardware & Timing Constraints**:
  - Target MCU: STM32WLE5 SoC (ARM Cortex-M4 @ 48 MHz).
  - Switched Sensor Rail (`PA4` / `BSP_POWER_RAIL_SENSORS`): High-side P-MOSFET requires $20\text{ ms}$ RC stabilization delay after energizing.
  - I2C Bus Recovery: Primary 9-clock toggle sequence. If stuck, toggle sensor rail (`PA4`) with stabilization guard.
  - Stale Pressure Hold: Up to 3 cycles (`FAULT_MAX_PRESSURE_STALE_CYCLES = 3`) before substituting neutral defaults ($20.0^\circ\text{C}, 70.0\%, 950.0\text{ hPa}$).
  - Solar Fallback: Neutral $25,000\text{ Lux}$ during daylight (06:00 to 18:00), $0\text{ Lux}$ at night. Clamps solar drop rate to prevent false optical storm alarms.
  - Rain Gauge EXTI Clamp: Max 40 tips/sec ($8.0\text{ mm/hr}$) to prevent runaway CPI score calculation.
  - Self-Healing Auto-Clear: 3 consecutive successful cycles (`FAULT_MAX_CONSECUTIVE_HEAL = 3`) automatically clear active fault bits.
  - Telemetry Error Flag: Master error mapped to LoRaWAN packet Byte 11 bit 5 (0x20) and optical fault beacon.
  - Stop 2 Deep Sleep: Guaranteed entry with current drain $< 3.0\,\mu\text{A}$ even during complete peripheral failure.

## History

- 2026-09-09: Implemented Phase 1 documentation deliverables (System & Hardware Architecture).
- 2026-09-09: Implemented Phase 2 documentation deliverables (Sensors & Prediction Math).
- 2026-09-09: Implemented Phase 3 documentation deliverables (Firmware & Telemetry Protocols).
- 2026-09-09: Implemented Phase 4 documentation deliverables (Power, Hardening & Mechanical).
- 2026-09-09: Implemented Phase 5 documentation deliverables (Field Validation & Calibration).
- 2026-09-09: Completed full Technical Documentation Roadmap across all 5 architecture, hardware, sensor, and algorithm phases.
- 2026-09-09: Created and linked all Sprint 1 task specifications (S1-T1.1 through S1-T3.4) covering CMake, Makefile, .clang-format, Unity test harness, Mock I2C/UART/GPIO drivers, and Python microclimate simulator.
- 2026-09-09: Created and linked all 17 Sprint 2 task specifications (S2-T1.1 through S2-T5.3) covering static ring buffer, moving average filters, Magnus-Tetens vapor pressure & dew point depression, hypsometric sea-level barometric reduction, 26-state Zambretti forecaster with monsoon/wind weighting, multi-variable gradient trend detection, 5-variable composite precipitation scoring ($CPI$), and 4-tier alert classification with simulated convective storm verification.
- 2026-09-09: Created and linked all 11 Sprint 3 task specifications (S3-T1.1 through S3-T4.4) covering STM32WLE5 48-pin GPIO allocations, multi-source clock tree configuration, HAL module whitelist, bounded I2C1 master driver with 9-clock bus recovery, dual-port UART/SDI-12 driver with circular buffers, switched P-MOSFET sensor rails with $20\text{ms}$ RC stabilization, board indicators and siren relays, pre-sleep GPIO analog isolation, Stop 2 deep sleep manager with RTC wakeup, $8.0\text{s}$ Independent Watchdog (IWDG) supervision, and factory-calibrated battery ADC telemetry with $\text{LiFePO}_4$ State of Charge modeling.
- 2026-09-09: Created and linked all 18 Sprint 4 task specifications (S4-T1.1 through S4-T5.3).
- 2026-09-09: Completed all 12 task specifications for Sprint 5 (Telemetry Protocol, LoRaWAN & Flash Storage).
- 2026-09-09: Completed all 10 task specifications for Sprint 6 (Application Orchestration, Scheduler & Local Alerts).
- 2026-09-09: Created and linked all 8 task specifications for Sprint 7 (System Integration, Synthetic Validation & Field SOPs).
- 2026-09-09: Completed the entire 7-Sprint Engineering Specification Roadmap (80+ tasks across 93 spec files).
- 2026-09-10: S1-T1.1 through S1-T1.4 — Root build system (CMake, Makefile, .clang-format, Dev Container & CI).
- 2026-09-11: S1-T2.1 through S1-T2.5 — Unity test framework, Mock drivers, and host sanity tests.
- 2026-09-11: S1-T3.1 through S1-T3.4 — Python microclimate weather simulator. Completed Sprint 1.
- 2026-09-12: S2-T1.1 through S2-T5.3 — Meteorological algorithms & nowcasting engine. Completed Sprint 2.
- 2026-09-13: S3-T1.1 through S3-T4.4 — Core MCU architecture, BSP & power management. Completed Sprint 3.
- 2026-09-14: S4-T1.1 through S4-T5.3 — Sensor drivers & remote field bus interfaces. Completed Sprint 4.
- 2026-09-14: S5-T1.1 through S5-T2.3 — LoRaWAN binary telemetry codec & JavaScript gateway decoders.
- 2026-09-16: S5-T3.1 through S5-T3.4 — On-chip Flash circular ring-buffer logging & playback.
- 2026-09-19: S5-T4.1 through S5-T4.4 — LoRaWAN Class A stack, regional ADR, and TX priority queue. Completed Sprint 5.
- 2026-09-19: S6-T1.1 — Implemented Adaptive Measurement Scheduler core (15-min/5-min/2-min autonomous multi-rate state machine with 30-minute anti-chatter hysteresis, active-time RTC sleep compensation, and remote downlink override).
- 2026-09-19: S6-T1.2 — Implemented Battery Preservation Throttling (3-tier battery preservation engine with 30-min Conservation and 60-min Critical interval extension, 100 mV anti-oscillation hysteresis with 2-consecutive reading recovery filter, peripheral actuation gating muting buzzer/siren, Modbus/SDI-12 bus isolation, and LoRa RF power capping to +14 dBm in Critical tier).
- 2026-09-19: S6-T2.1 — Implemented Status LED Flash Patterns & Visual Alert Engine (8-state optical matrix including 5% Green healthy pulse, 50% Amber watch blink, 50% Red warning blink, 10 Hz Red rapid strobe, Red double-flash for active rain, and alternating Green/Red fault beacon; non-blocking millisecond tick animation; strict priority cascade with battery preservation throttling down to 8 uA in Tier 2 and 0 uA shutdown in Tier 3/sleep).
- 2026-09-19: S6-T2.2 — Implemented Audible Buzzer Burst & Estate Siren Relay Trigger (5-state acoustic cadence including single watch chirp, double warning chirp, 200ms storm burst, and periodic fault tone; 10s auto-cutoff relay pulse; 30-minute anti-chatter cooldown hysteresis; nighttime quiet hours muting; battery preservation interlocks muting buzzers/siren in Conservation and Critical tiers).
- 2026-09-19: S6-T2.3 — Implemented Alert Manager Unit Test Suite (24 Unity test cases across Categories A..D validating initialization defaults, LED waveforms, buzzer chirps, 10s auto-cutoff siren pulses, 30-min anti-chatter cooldown, night quiet hours, battery preservation throttling, and sensor fault overrides).
- 2026-09-20: S6-T3.1 — Implemented 8-State Application State Machine & Lifecycle Flow (STATE_WAKE -> STATE_POWER_ON -> STATE_SAMPLE -> STATE_FILTER -> STATE_PREDICT -> STATE_TRANSMIT -> STATE_ALERT -> STATE_SLEEP deterministic cycle, master app_context_t, non-blocking single-step runner, 20ms switched sensor rail RC stabilization guard, BME280/OPT3001/rain gauge/battery ADC sampling, psychrometric dew point math, Zambretti & CPI forecasting, 12-byte periodic telemetry bit-packing, Flash ring buffer logging, LoRaWAN Class A transmission, local alert manager dispatch, active execution time sleep compensation, GPIO low-leakage Stop 2 deep sleep, and main dispatcher loop in main.c).
