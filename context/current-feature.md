# Current Feature: S6-T2.1 Status LED Flash Patterns & Visual Alert Engine

## Status

In Progress

## Goals

- Implement Status LED Flash Patterns & Visual Alert Engine (`alert_manager.h` and `alert_manager.c`).
- Provide non-blocking pattern animation timing via asynchronous tick processor (`alert_manager_process_step(delta_ms)`).
- Implement complete 8-state optical flash pattern matrix:
  - `ALERT_LED_PATTERN_OFF`: Dark / All OFF (Stop 2 sleep or Critical Battery Tier 3).
  - `ALERT_LED_PATTERN_HEALTHY_PULSE`: Green pulse 50ms ON / 950ms OFF (1.0 Hz, 5% duty cycle, CPI < 30%).
  - `ALERT_LED_PATTERN_WATCH_AMBER`: Amber blink 250ms ON / 250ms OFF (2.0 Hz, 50% duty cycle, 30% <= CPI < 60%).
  - `ALERT_LED_PATTERN_WARNING_RED`: Red warning blink 200ms ON / 200ms OFF (2.5 Hz, 50% duty cycle, 60% <= CPI < 80%).
  - `ALERT_LED_PATTERN_IMMINENT_STROBE`: Red rapid strobe 50ms ON / 50ms OFF (10.0 Hz, 50% duty cycle, CPI >= 80% or overrides).
  - `ALERT_LED_PATTERN_ACTIVE_RAIN`: Red double flash 50ms ON / 50ms OFF / 50ms ON / 850ms OFF (1.0 Hz, 10% duty cycle).
  - `ALERT_LED_PATTERN_SYSTEM_FAULT`: Alternating Green/Red beacon (100ms Green, 100ms Red, 100ms Green, 100ms Red, 600ms OFF).
  - `ALERT_LED_PATTERN_CONSERVATION`: Green micro-pulse 10ms ON / 4990ms OFF (0.2 Hz, 0.2% duty cycle).
- Implement strict priority cascade: Pre-Sleep Off -> Critical Battery Tier 3 Suppressed -> System Hardware Fault -> Rain Imminent Strobe -> Active Rain Double Flash -> Rain Likely Warning -> Rain Possible Watch -> Conservation Micro-Pulse -> Healthy Pulse.
- Support direct manual pattern override via `alert_manager_set_led_pattern()` and all-off emergency cutoff via `alert_manager_force_all_off()`.
- Provide runtime status telemetry and diagnostics via `alert_manager_get_status()` and pattern string lookup via `alert_manager_get_pattern_name()`.

## Notes

- Hardware Constraints:
  - Green Status LED connected to `PB8` (`BSP_LED_GREEN`).
  - Red Warning/Alarm LED connected to `PB9` (`BSP_LED_RED`).
  - Active-High push-pull driven via Layer 2 driver `bsp_indicators.h`.
- Power Constraints:
  - Standard LED draw: ~4.0 mA per LED (~8.0 mA total during Amber).
  - Average current in Healthy mode: ~200 uA.
  - Average current in Conservation mode (Battery Tier 2: 2.90V <= Vbat < 3.10V): ~8 uA (25x reduction).
  - Critical Tier 3 (Vbat < 2.90V) or Deep Sleep: 0.0 uA (complete LED suppression).
- Timing & Execution:
  - Non-blocking delta_ms stepping, zero busy-wait loops, MISRA-C compliant, zero dynamic heap allocation.
- Discovered Module Dependencies:
  - `firmware/drivers/inc/bsp_indicators.h`: Low-level LED control (`bsp_led_set`, `bsp_indicators_all_off`).
  - `firmware/app/inc/rain_algo.h`: Rain alert states (`rain_alert_state_t`).
  - `firmware/app/inc/measurement_scheduler.h`: Battery preservation tiers (`battery_throttle_tier_t`).
  - `firmware/core/inc/status.h`: System status definitions.

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
