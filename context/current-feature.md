# Current Feature: S6-T4.2 - System Fault Injection & Resilience Integration Tests

## Status

In Progress

## Goals

- Develop host-executable integration test suite in [`tests/integration/test_fault_injection.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_fault_injection.c) using the ThrowTheSwitch Unity framework.
- Implement comprehensive 16-point fault injection test matrix across 4 major failure categories:
  - **Category A (Sensor & I2C Bus Faults)**:
    - `FI-SM-01`: BME280 disconnect and stale pressure holding for 3 cycles before neutral baseline fallback.
    - `FI-SM-02`: Autonomous I2C bus lockup recovery via 9 SCL clock pulses and power rail cycling.
    - `FI-SM-03`: OPT3001 dead sensor solar heuristic substitution (RTC daytime 25,000 Lux / night 0 Lux) preventing false storm overrides.
    - `FI-SM-04`: Self-healing 3-consecutive clean read automatic fault flag clearing.
  - **Category B (Battery Depletion & Brownout Throttling)**:
    - `FI-SM-05`: Battery Tier 2 Conservation entry at 3.05V (< 3.10V) with 30-min cadence extension and acoustic muting.
    - `FI-SM-06`: Battery Tier 3 Critical entry at 2.85V (< 2.90V) with 60-min cadence extension, LED shutdown, and RF power capping (+14 dBm).
    - `FI-SM-07`: Battery hysteresis recovery restoring Tier 1 Normal cadence when voltage recovers to 3.22V (> 3.20V).
    - `FI-SM-08`: Siren relay blast suppression during severe storm event ($CPI \ge 80\%$) under low battery to conserve uplink capacity.
  - **Category C (LoRa Radio & Gateway Blackouts)**:
    - `FI-SM-09`: LoRa radio TX timeout (150 ms) non-blocking recovery with packet fallback buffered in Flash storage.
    - `FI-SM-10`: 72-hour LoRa gateway blackout logging 288 consecutive periodic records safely to Flash circular buffer.
    - `FI-SM-11`: Gateway reconnect historical backlog playback draining via FPort 3 confirmed batches.
    - `FI-SM-12`: High-priority storm alert packet preemption on FPort 2 over background backlog playback.
  - **Category D (Flash, Mechanical & Compound Outages)**:
    - `FI-SM-13`: Flash write failure bypass ensuring telemetry continues over LoRa uplink without crashing.
    - `FI-SM-14`: Rain gauge contact chatter clamping to 40 tips/cycle (500 mm/hr physical ceiling).
    - `FI-SM-15`: Watchdog timeout reboot recovery asserting Byte 11 bit 6 (`0x40`) unexpected reboot telemetry flag.
    - `FI-SM-16`: Total compound multi-fault survival (I2C lockup + OPT3001 failure + LoRa timeout + Flash error + low battery) executing in < 1.2s and safely entering Stop 2 sleep (< 3.0 µA).
- Register `test_fault_injection` executable in [`tests/CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/CMakeLists.txt) and verify zero compilation warnings.

## Notes

- **Hardware Constraints & Fallback Limits**:
  - Switched P-MOSFET sensor rail (`PA4`) with $20\text{ms}$ RC stabilization delay.
  - Autonomous 2-tier I2C bus recovery: 9 SCL clock pulses followed by switched sensor rail toggle.
  - BME280 stale pressure holding limit: strictly 3 cycles before neutral baseline fallback ($S_P = 50, S_{RH} = 50$).
  - OPT3001 solar fallback heuristics: 25,000 Lux (06:00-18:00) and 0 Lux (nighttime), clamping $S_{sol} = 0$.
  - Rain gauge contact chatter debounce: clamped to 40 pulses/cycle ($8.0\text{mm}$ / $500\text{mm/hr}$).
  - Low-power Stop 2 deep sleep entry ($< 3.0\,\mu\text{A}$) with all unused pins set to analog mode.
  - Battery preservation thresholds: Tier 2 Conservation ($< 3.10\text{V}$, $1800\text{s}$ interval), Tier 3 Critical ($< 2.90\text{V}$, $3600\text{s}$ interval, LEDs disabled, LoRa RF capped to $+14\text{dBm}$), $100\text{mV}$ hysteresis recovery ($3.20\text{V}$).
  - Watchdog supervision: $8.0\text{s}$ window, reset cause latch via `RCC_CSR_IWDGRSTF` / `watchdog_was_reset_by_watchdog()` mapped to Byte 11 bit 6 (`0x40`).
- **Discovered Module Dependencies**:
  - Application Layer: `app_state_machine.h` / `app_state_machine.c`, `app_fault_handler.h` / `app_fault_handler.c`, `measurement_scheduler.h` / `measurement_scheduler.c`, `alert_manager.h` / `alert_manager.c`, `rain_algo.h` / `rain_algo.c`.
  - Algorithm Layer: `zambretti.h` / `zambretti.c`, `trend_detector.h` / `trend_detector.c`, `dew_point.h` / `dew_point.c`, `moving_avg_filter.h` / `moving_avg_filter.c`.
  - Middleware & Drivers: `telemetry_codec.h` / `telemetry_codec.c`, `status_codes.h` / `status.h`, `power_mgr.h`, `bsp_power_rails.h`, `bsp_indicators.h`, `bsp_adc.h`, `bme280.h`, `opt3001.h`, `rain_gauge.h`, `flash_storage.h`, `lorawan_service.h`, `watchdog.h`.
  - Test Target: `tests/integration/test_fault_injection.c`, `tests/CMakeLists.txt`.
- **Execution & Timing Budget**:
  - Total active CPU execution per cycle $< 1.2\text{ seconds}$ ($< 1200\text{ ms}$) across all nominal and catastrophic fault states.
  - Sub-GHz LoRa radio TX timeout bounded to $150\text{ ms}$.
- **Graphify & Build Policy**:
  - CMake builds and tests run in GitHub Actions CI (`.github/workflows/ci.yml`). Do not execute local CMake builds.

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
- 2026-09-20: S6-T3.2 — Implemented Graceful Degradation & Fault Tolerance Paths (Centralized app_fault_handler engine with 10 subsystem fault bitmasks and diagnostic status tracking; 3-cycle auto-clearing self-healing mechanism; autonomous 2-tier I2C bus recovery via 9-clock SCL cycling and switched P-MOSFET sensor rail power toggle; BME280 fallback holding last valid barometric pressure for up to 3 cycles before neutral baseline fallback; OPT3001 solar fallback with daytime 25,000 Lux and nighttime 0 Lux heuristics; rain gauge contact chatter clamping to 40 tips/interval; state machine integration across sampling, transmission, and alerts with bounded execution (< 150 ms) and guaranteed Stop 2 deep sleep entry (< 3.0 uA); Unity unit test suite with 7 test cases covering recovery, fallbacks, and self-healing).
- 2026-09-20: S6-T3.3 — Implemented Watchdog Kick Points at State Execution Checkpoints & Boot Reset Diagnostics (Integrated 8 dedicated safe watchdog refresh checkpoints into top-level state machine sequence STATE_WAKE through STATE_SLEEP; enforced strict anti-masking invariants verifying state bounds, single-state execution duration < 200 ms, and zero ISR refreshes; integrated boot reset cause diagnostics via watchdog_was_reset_by_watchdog() and mapped unexpected reboot flag to Byte 11 bit 6 in periodic LoRaWAN telemetry; maintained runtime diagnostic counters; verified DBGMCU Stop 2 deep sleep counter freeze; created comprehensive Unity test suite test_app_state_machine.c validating all 10 verification criteria UT_WDG_01 through CP_WDG_10).
- 2026-09-20: S6-T4.1 — Implemented Full System State Machine End-to-End Integration Tests (Created test_state_machine.c under Unity framework verifying the complete 4-layer embedded firmware stack operating cohesively across all 8 states; constructed comprehensive mock harness for switched PA4 rails, BME280, OPT3001, rain gauge EXTI pulses, battery ADC, Flash NVM circular storage, LoRaWAN Class A service, and IWDG watchdog checkpoints; implemented and verified all 16 integration tests IT-SM-01 through IT-SM-16 covering nominal cycles, 20ms RC rail stabilization, 12-byte LoRaWAN serialization, Flash ring buffer logging, active-time compensated Stop 2 sleep, convective storm detection with Red 10 Hz strobe and 10s siren blast, 30-min siren anti-chatter cooldown suppression, 2-min active rain cadence acceleration, 30-min calm hold-down recovery, remote downlink schedule overrides, EXTI wake handling, 24-hour continuous mission stability, and CPU processing budget compliance < 1.2s; added test_state_machine target to tests/CMakeLists.txt).
