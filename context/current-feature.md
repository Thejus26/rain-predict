# Current Feature

## Status

Specification Phase - Sprint 3 Complete (Sprint 4 Upcoming)

## Goals

- Author and maintain rigorous, traceable, embedded C99 task specifications for the Tea Plantation Rain Prediction System firmware running on the STM32WLE5 SoC.
- Define target hardware configurations, clock trees (MSI 48 MHz, LSE 32.768 kHz, HSE 32 MHz), STM32CubeWL HAL module whitelist, and bounded non-blocking I2C/UART bus drivers with lockup recovery.
- Specify switched sensor power rails ($20\text{ms}$ stabilization guard), board indicators (LEDs, buzzer, siren auto-cutoff), pre-sleep GPIO analog isolation, ultra-low power Stop 2 sleep mode ($< 3.0\,\mu\text{A}$), Independent Watchdog ($8.0\text{s}$ timeout), and factory-calibrated battery ADC telemetry.
- Ensure all specifications include complete Doxygen API blueprints, state/sequence diagrams, register calculations, defensive guards, and 10-point verification test matrices.

## Notes

- **MCU Hardware Target**: STMicroelectronics STM32WLE5CC (ARM Cortex-M4 @ 48 MHz, 64 KB SRAM, 256 KB Flash).
- **Embedded C Standards**: Strict C99, `<stdint.h>`, `<stdbool.h>`, zero dynamic memory allocation (`malloc`/`free` prohibited), 4-space indentation, deterministic bounded non-blocking timeouts.
- **Power Architecture**: High-side P-MOSFET sensor power gating (`PA4` / `VSENS_SW`), $20\text{ms}$ stabilization delay, gated battery divider (`PB1` / `PB0`), pre-sleep analog isolation to suppress parasitic ESD clamping leakage.
- **Sleep & Watchdog Management**: Stop 2 mode with RTC periodic wakeup timer ($2\text{--}15\text{ min}$), full 64 KB SRAM retention ($< 3.0\,\mu\text{A}$ standby), and Independent Watchdog (IWDG, $8.0\text{s}$ timeout via 32 kHz LSI) with debug Stop 2 counter freeze.
- **Battery & Solar Telemetry**: Factory $V_{\text{REFINT\_CAL}}$ (`0x1FFF75AA`) analog supply calibration, 8-segment $\text{LiFePO}_4$ piecewise State of Charge mapping, and solar harvesting status classification.

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
