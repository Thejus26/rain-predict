# Current Feature: S6-T4.1 - Full System State Machine End-to-End Integration Tests

## Status

In Progress

## Goals

- **Comprehensive 4-Layer Stack Cross-Verification**: Develop host-executable integration test suite in `tests/integration/test_state_machine.c` verifying Core & HAL (Layer 1), BSP & Drivers (Layer 2), Middleware & Services (Layer 3), and Application (Layer 4) executing cohesively across all 8 states under ThrowTheSwitch Unity framework.
- **Hardware & Peripheral Mock Harness**: Construct an integrated mock harness stubbing switched sensor rails (`PA4`), BME280 forced-burst acquisition, OPT3001 single-shot lux conversion, rain gauge EXTI pulse accumulation, battery ADC reading, Flash NVM circular storage, LoRaWAN Class A packet serialization, and IWDG watchdog checkpoints.
- **Three End-to-End Operational Weather Missions**:
  - *Mission 1: Quiescent Fair-Weather Cycle ($CPI < 30\%$)* — Validate deterministic sequence `STATE_WAKE` $\rightarrow \dots \rightarrow$ `STATE_SLEEP`, 8 safe watchdog checkpoints, 12-byte telemetry frame packing, Flash circular buffer logging, Green heartbeat pulse ($50\text{ms}$ ON / $950\text{ms}$ OFF), and active-time compensated Stop 2 sleep entry ($899\text{s}$).
  - *Mission 2: Rapid Convective Storm Development & Siren Actuation ($CPI \ge 80\%$)* — Validate severe barometric drop override, $CPI \ge 80\%$ imminent alert classification, Red $10\text{Hz}$ rapid strobe, $10\text{s}$ estate siren pulse actuation, $5\text{min}$ schedule acceleration, and $30\text{min}$ siren cooldown lockout.
  - *Mission 3: Active Rain Downpour & Multi-Horizon Accumulation* — Validate EXTI tipping bucket pulse processing, Red double-flash beacon, $2\text{min}$ cadence promotion, and $30\text{min}$ calm hold-down recovery before returning to nominal $15\text{min}$ interval.
- **16-Point Integration Test Matrix (`IT-SM-01` through `IT-SM-16`)**:
  - `IT-SM-01` (`test_integration_cold_boot_initialization`): Verify cold boot subsystem initialization and initial state `STATE_WAKE`.
  - `IT-SM-02` (`test_integration_single_quiescent_cycle`): Verify nominal 8-state sequence, 8 watchdog kicks, and `cycle_count == 1`.
  - `IT-SM-03` (`test_integration_power_rail_stabilization`): Verify `PA4` load switch enable and $20\text{ms}$ RC stabilization guard delay prior to I2C traffic.
  - `IT-SM-04` (`test_integration_telemetry_packet_generation`): Verify 12-byte big-endian LoRaWAN frame matching sensor measurements.
  - `IT-SM-05` (`test_integration_flash_nvm_logging`): Verify 12-byte packet commit into Flash circular ring buffer with valid count and `0xAA55` magic.
  - `IT-SM-06` (`test_integration_active_time_compensation`): Verify active CPU duration measured and subtracted from RTC sleep timer ($900\text{s} - 950\text{ms} \implies 899\text{s}$).
  - `IT-SM-07` (`test_integration_pre_sleep_hardware_shutdown`): Verify sensor rails powered off, indicators forced off, and Stop 2 sleep entered.
  - `IT-SM-08` (`test_integration_storm_watch_acceleration`): Verify pressure drop precursor ($CPI = 45\%$) promotes cadence to $300\text{s}$ and Amber watch blink.
  - `IT-SM-09` (`test_integration_imminent_storm_siren_actuation`): Verify severe storm ($CPI = 91\%$) triggers Red $10\text{Hz}$ strobe, $10\text{s}$ siren relay pulse, and arms $1800\text{s}$ cooldown.
  - `IT-SM-10` (`test_integration_siren_cooldown_suppression`): Verify subsequent storm cycle keeps Red strobe but suppresses siren relay during active cooldown.
  - `IT-SM-11` (`test_integration_active_rain_rapid_cadence`): Verify tipping bucket pulses promote cadence to $120\text{s}$ and Red double-flash beacon.
  - `IT-SM-12` (`test_integration_calm_hold_down_recovery`): Verify system enforces $30\text{min}$ consecutive calm hold-down before restoring nominal $15\text{min}$.
  - `IT-SM-13` (`test_integration_downlink_schedule_override`): Verify FPort 10 remote downlink packet overrides autonomous schedule until cleared.
  - `IT-SM-14` (`test_integration_exti_rain_wake_handling`): Verify rain pulse EXTI wake during Stop 2 sleep increments accumulator and resumes sleep.
  - `IT-SM-15` (`test_integration_continuous_24hour_mission`): Verify 96 consecutive 15-minute cycles with zero memory leaks and 96 committed Flash logs.
  - `IT-SM-16` (`test_integration_processing_budget_compliance`): Verify active CPU window per cycle is strictly $< 1.2\text{s}$ ($< 1200\text{ms}$).
- **Build System Integration**: Add `test_state_machine` executable target to `tests/CMakeLists.txt` and register with CTest test runner.
- **Code Quality & Verification**: Ensure 100% test pass rate with zero warnings under host GCC/Clang builds conforming to `context/coding-standards.md`.

## Notes

### Discovered Module Dependencies (via Knowledge Graph & Architecture Analysis)
- **Application Layer**:
  - `firmware/app/inc/app_state_machine.h` & `app_state_machine.c`: 8-state sequence coordinator, `app_context_t`, `app_state_machine_init()`, `app_state_machine_step()`, `app_state_machine_run_cycle()`, `app_state_machine_get_context()`.
  - `firmware/app/inc/measurement_scheduler.h` & `measurement_scheduler.c`: Adaptive cadence state machine ($15\text{m} / 5\text{m} / 2\text{m}$), $30\text{m}$ anti-chatter calm hold-down, active-time compensation, downlink overrides.
  - `firmware/app/inc/alert_manager.h` & `alert_manager.c`: LED waveform generator (Green pulse, Amber watch, Red warning, Red strobe, Red double-flash), buzzer acoustic cadences, $10\text{s}$ siren relay pulse with $1800\text{s}$ cooldown.
  - `firmware/app/inc/app_fault_handler.h` & `app_fault_handler.c`: Subsystem fault status mask, sensor fallback values, I2C bus recovery.
  - `firmware/app/inc/rain_algo.h` & `rain_algo.c`: CPI composite scoring and threshold classification (`RAIN_ALERT_UNLIKELY` to `RAIN_ALERT_IMMINENT`).
- **Algorithms & Middleware Layer**:
  - `firmware/algorithm/inc/zambretti.h` & `zambretti.c`: 26-rule barometric forecaster.
  - `firmware/algorithm/inc/trend_detector.h` & `trend_detector.c`: Multi-variable gradient trend evaluation.
  - `firmware/algorithm/inc/dew_point.h` & `dew_point.c`: Magnus-Tetens dew point and dew point depression calculation.
  - `firmware/algorithm/inc/moving_avg_filter.h` & `moving_avg_filter.c`: Noise reduction filtering.
  - `firmware/middleware/inc/telemetry_codec.h` & `telemetry_codec.c`: 12-byte periodic packet bit-packing and decoding.
  - `firmware/middleware/inc/power_mgr.h`: System tick, battery voltage ADC, Stop 2 deep sleep entry, RTC wake timers.
  - `firmware/middleware/inc/flash_storage.h`: Circular ring buffer Flash NVM record storage.
  - `firmware/middleware/inc/lorawan_service.h`: Class A Sub-GHz transmission dispatch.
- **Core & Drivers Layer**:
  - `firmware/core/inc/status.h`: Standardized status codes (`STATUS_OK`, `STATUS_ERR_*`).
  - `firmware/core/inc/watchdog.h`: 8 safe state execution checkpoints, IWDG timeout supervision ($8.0\text{s}$).
  - `firmware/drivers/inc/bsp_power_rails.h`: Switched sensor power rail (`PA4` load switch).
  - `firmware/drivers/inc/bsp_indicators.h`: Status LEDs, buzzer, and siren relay driver.
  - `firmware/drivers/inc/bme280_driver.h`: Temperature, humidity, barometric pressure acquisition.
  - `firmware/drivers/inc/opt3001_driver.h`: Ambient light lux conversion.
  - `firmware/drivers/inc/rain_gauge_driver.h`: EXTI pulse counter accumulation.

### Hardware & Power Constraints
- **Switched Sensor Rail (`PA4`)**: Must observe a mandatory $20\text{ms}$ RC stabilization delay after energizing `PA4` before issuing I2C transactions to BME280/OPT3001.
- **Stop 2 Deep Sleep Floor**: Target quiescent sleep current $< 3.0\,\mu\text{A}$; requires switching all unused GPIOs to analog low-leakage state prior to sleep.
- **Watchdog Supervision (IWDG)**: Hardware watchdog configured with $8.0\text{s}$ timeout; refreshed at exactly 8 distinct state boundary checkpoints with single-state execution $< 200\text{ms}$ guard.
- **Active Processing Window**: Total active CPU time per cycle must remain strictly below $1.2\text{ seconds}$ ($< 1200\text{ ms}$) across all states to meet battery life budget.
- **Estate Siren Pulse & Cooldown**: Siren relay is hardware-energized for at most $10\text{s}$ ($10,000\text{ms}$), followed by a mandatory $1800\text{s}$ ($30\text{min}$) anti-chatter cooldown period to avoid nuisance false-alarm triggering.

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
