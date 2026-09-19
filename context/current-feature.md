# Current Feature: S6-T1.2 — Battery Preservation Throttling & Low-Power Interval Extension

## Status

In Progress

## Goals

- Add `battery_throttle_tier_t` enum (`BATTERY_TIER_NORMAL`, `BATTERY_TIER_CONSERVATION`, `BATTERY_TIER_CRITICAL`) to `measurement_scheduler.h`
- Add `SCHEDULER_MODE_CONSERVATION` (30-min) and `SCHEDULER_MODE_CRITICAL` (60-min) entries to `scheduler_mode_t`
- Add battery voltage threshold macros: `SCHEDULER_VBAT_CONSERVE_ENTER_V` (3.10 V), `SCHEDULER_VBAT_CONSERVE_RECOVER_V` (3.20 V), `SCHEDULER_VBAT_CRITICAL_ENTER_V` (2.90 V), `SCHEDULER_VBAT_CRITICAL_RECOVER_V` (3.00 V)
- Add conservation and critical interval constants: `SCHEDULER_INTERVAL_CONSERVE_SEC` (1800 s) and `SCHEDULER_INTERVAL_CRITICAL_SEC` (3600 s)
- Extend `scheduler_config_t` with `conserve_interval_sec`, `critical_interval_sec`, and 4 Vbat threshold floats
- Extend `scheduler_decision_t` with `battery_tier`, `actuation_allowed`, `aux_buses_allowed`, and `max_tx_power_dbm` fields
- Extend `scheduler_status_t` with `battery_tier`, `last_vbat_volts`, `actuation_allowed`, `aux_buses_allowed`, `max_tx_power_dbm`, and `total_wakeups_throttled`
- Implement `measurement_scheduler_set_battery_voltage(float vbat_volts)` with 3-tier hysteresis state machine and 2-consecutive-reading recovery filter
- Implement `measurement_scheduler_is_actuation_allowed()` — buzzer/siren only in Tier 1
- Implement `measurement_scheduler_is_aux_sensor_allowed()` — Modbus/SDI-12 off in Tier 3
- Implement `measurement_scheduler_get_max_tx_power()` — +22 dBm in Tiers 1 & 2, +14 dBm in Tier 3
- Modify `measurement_scheduler_evaluate()` to enforce battery tier interval clamping (Critical overrides all to 60 min; Conservation baseline 30 min with rain capped at 5 min)
- Modify `measurement_scheduler_init()` and `measurement_scheduler_reset()` to initialize new battery state variables
- Modify `measurement_scheduler_get_status()` to populate new diagnostic fields
- Validate voltage input bounds [1.0 V, 5.0 V] in `set_battery_voltage()`, reject with `STATUS_ERROR_INVALID_PARAM`
- Pass all 10 verification test cases from spec (TC-BAT-01 through TC-BAT-10)

## Notes

### Target Files
- `firmware/app/inc/measurement_scheduler.h` — extend enums, structs, add new function prototypes
- `firmware/app/src/measurement_scheduler.c` — implement battery tier state machine and peripheral gating

### Dependencies (Upstream)
- **S6-T1.1** (Measurement Scheduler Core) — already implemented; this task extends it
- **S3-T4.4** (`bsp_adc.h` / `bsp_adc.c`) — Battery ADC driver provides `bsp_adc_read_battery_voltage()` as Vbat input source
- **S1-T1.1** (`status.h`) — `STATUS_OK`, `STATUS_ERROR_INVALID_PARAM`, `STATUS_ERROR_NULL_POINTER`, `STATUS_ERROR_NOT_INITIALIZED`, `STATUS_ERROR_OUT_OF_BOUNDS`

### Downstream Consumers
- **S6-T2.1** (Alert Manager) — queries `measurement_scheduler_is_actuation_allowed()` to gate siren/buzzer
- **S6-T3.1** (App State Machine) — queries battery tier to adjust lifecycle transitions
- **S7-T2.3** (14-Day Zero-Sunlight Test) — validates Tier 3 survival autonomy

### Hardware Constraints
- LiFePO4 flat discharge plateau: 3.30 V → 3.20 V (80% → 20% SoC)
- Discharge knee begins at 3.10 V (~15% SoC), steep drop below 2.90 V (~5% SoC)
- STM32WLE5 brownout reset threshold (V_BOR0) = 2.55 V
- LoRa HP PA (+22 dBm) peak current: 110 mA; LP PA (+14 dBm): 45 mA
- Piezo buzzer: 35 mA; Siren relay coil: 60 mA; RS-485 transceiver: 40 mA
- 100 mV hysteresis gaps prevent oscillation under fluctuating solar conditions

### Existing Module State (from S6-T1.1)
- Header includes `status.h` (aliased from `status_codes.h` in spec) and `rain_algo.h`
- `scheduler_mode_t` currently has: NOMINAL, STORM_WATCH, ACTIVE_RAIN, OVERRIDE
- `scheduler_decision_t` currently has: active_mode, target_interval_sec, computed_sleep_sec, mode_changed, hold_down_remaining_sec
- `scheduler_config_t` currently has: nominal/storm/rain intervals, hold_down, CPI thresholds, pressure_drop
- Static state: s_current_mode, s_hold_down_timer_sec, s_rain_calm_timer_sec, s_override_interval_sec, s_is_override_active, s_last_sleep_duration_sec, s_total_wakeups_nominal/storm/rain, s_is_initialized

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
