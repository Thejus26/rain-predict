# Current Feature

## Status

Specification Phase - Sprint 2 Complete (Sprint 3 Upcoming)

## Goals

- Author and maintain rigorous, traceable, embedded C99 task specifications for the Tea Plantation Rain Prediction System firmware running on the STM32WLE5 SoC.
- Ensure all meteorological calculations (Magnus-Tetens, Zambretti heuristic forecaster, multi-variable gradient detector, composite precipitation index) adhere to single-precision hardware FPU discipline, zero dynamic memory allocation, and $< 0.1^\circ\text{C}$ / $< 0.1\text{ hPa}$ meteorological tolerances.
- Provide comprehensive ThrowTheSwitch Unity unit test suites with simulated synthetic storm vectors for 100% host verification coverage.

## Notes

- **MCU Hardware Target**: STMicroelectronics STM32WLE5CC (ARM Cortex-M4 @ 48 MHz with Single-Precision Hardware FPU, 64 KB SRAM, 256 KB Flash).
- **Embedded C Standards**: C99/C11, `<stdint.h>`, `<stdbool.h>`, zero `malloc`/`free`, deterministic bounded execution.
- **Memory Footprint**: Middleware ring buffer (18 samples @ 10-min cadence = 3h window), static stack footprints $< 48$ bytes per calculation.
- **Algorithm Performance**: Master nowcasting evaluation (`rain_algo_evaluate`) latency $< 500$ CPU cycles ($\approx 10.4\,\mu\text{s}$ @ 48 MHz).

## History

- 2026-09-09: Implemented Phase 1 documentation deliverables (System & Hardware Architecture).
- 2026-09-09: Implemented Phase 2 documentation deliverables (Sensors & Prediction Math).
- 2026-09-09: Implemented Phase 3 documentation deliverables (Firmware & Telemetry Protocols).
- 2026-09-09: Implemented Phase 4 documentation deliverables (Power, Hardening & Mechanical).
- 2026-09-09: Implemented Phase 5 documentation deliverables (Field Validation & Calibration).
- 2026-09-09: Completed full Technical Documentation Roadmap across all 5 architecture, hardware, sensor, and algorithm phases.
- 2026-09-09: Created and linked all Sprint 1 task specifications (S1-T1.1 through S1-T3.4) covering CMake, Makefile, .clang-format, Unity test harness, Mock I2C/UART/GPIO drivers, and Python microclimate simulator.
- 2026-09-09: Created and linked all 17 Sprint 2 task specifications (S2-T1.1 through S2-T5.3) covering static ring buffer, moving average filters, Magnus-Tetens vapor pressure & dew point depression, hypsometric sea-level barometric reduction, 26-state Zambretti forecaster with monsoon/wind weighting, multi-variable gradient trend detection, 5-variable composite precipitation scoring ($CPI$), and 4-tier alert classification with simulated convective storm verification.
