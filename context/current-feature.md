# Current Feature

## Status

Not Started

## Goals

<!-- Goals will be populated when a feature is loaded -->

## Notes

<!-- Notes will be populated when a feature is loaded -->

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
