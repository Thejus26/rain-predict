# Current Feature: S1-T1.4 - Cloud-Native Dev Container & GitHub Codespaces Configuration

## Status

In Progress

## Goals

- Create `.devcontainer/Dockerfile` based on `mcr.microsoft.com/devcontainers/base:ubuntu-22.04` including host compilation tools (`build-essential`, `make`, `cmake`, `ninja-build`), ARM cross-compiler (`gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `gdb-multiarch`), code hygiene tools (`clang-format`, `cppcheck`), and Python scientific stack (`numpy`, `pandas`, `matplotlib`, `scipy`).
- Create `.devcontainer/devcontainer.json` defining VS Code remote container settings (`editor.formatOnSave`, `C_Cpp.default.cStandard: c99`, `cmake.configureOnOpen`), required extensions (`cpptools`, `cmake-tools`, `cortex-debug`, `python`, `clang-format`), remote user `vscode`, and `postCreateCommand`.
- Create `.devcontainer/post-create.sh` initializing build environment diagnostics and checking toolchain versions.
- Create `.github/workflows/ci.yml` CI/CD workflow mirroring Dev Container tools to automate code formatting validation, static analysis (`cppcheck`), host unit testing (`ctest`), simulation regressions, and ARM Cortex-M4 firmware cross-compilation with artifact upload.

## Notes

- Target Files:
  - `.devcontainer/Dockerfile`
  - `.devcontainer/devcontainer.json`
  - `.devcontainer/post-create.sh`
  - `.github/workflows/ci.yml`
- Container optimizations: `apt-get clean`, `rm -rf /var/lib/apt/lists/*`, and `--no-install-recommends` to keep image footprint lean.
- Dependencies: `S1-T1.1` (Root CMake), `S1-T1.2` (Root Makefile), `S1-T1.3` (.clang-format).
- Downstream Impact: Provides baseline zero-install environment for all desktop unit tests, simulation runs, and STM32WLE5 firmware builds across Sprints 1–7.

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
- 2026-09-09: Created and linked all 18 Sprint 4 task specifications (S4-T1.1 through S4-T5.3) covering Bosch BME280 calibration readout, forced-mode burst sampling, hardware FPU compensation, and saturation recovery; TI OPT3001 single-shot conversion, exponential lux/irradiance math, and day/night cloud attenuation scoring; Tipping-Bucket Rain Gauge 50ms EXTI debounce, atomic pulse counters, multi-horizon FIFO registers, rain rate derivative math, and IMD/WMO intensity classification; RS-485 Modbus RTU FC03 frame generator, polynomial 0xA001 bitwise/LUT CRC-16, and PA1 transceiver direction guard timing; SDI-12 1200-baud break/mark timing, command formatting, and multi-parameter ASCII response token parsing; with comprehensive ThrowTheSwitch Unity test suites across all drivers.
- 2026-09-09: Completed all 12 task specifications for Sprint 5 (Telemetry Protocol, LoRaWAN & Flash Storage) covering binary telemetry codec, ChirpStack/TTN decoders, on-chip Flash ring buffer, LoRaWAN Class A stack, regional ADR, and transmission priority queue.
- 2026-09-09: Completed all 10 task specifications for Sprint 6 (Application Orchestration, Scheduler & Local Alerts) covering measurement scheduler, battery throttling, visual/audible alert manager, 8-state application state machine, fault tolerance handlers, watchdog checkpoints, and integration test suites.
- 2026-09-09: Created and linked all 8 task specifications for Sprint 7 (System Integration, Synthetic Validation & Field SOPs) covering 30-day multi-scenario synthetic climate simulation (S7-T1.1), meteorological contingency table metrics verification (S7-T1.2), Stop 2 deep sleep & active cycle energy profiling (S7-T2.1), 24-hour daily energy budget verification (S7-T2.2), 14-day zero-sunlight battery survivability simulation (S7-T2.3), on-site barometric altitude offset calibration SOP (S7-T3.1), tipping-bucket rain gauge dynamic water calibration SOP (S7-T3.2), and estate agronomic operational response guidelines & field safety protocols (S7-T3.3).
- 2026-09-09: Completed the entire 7-Sprint Engineering Specification Roadmap for the Tea Plantation Rain Prediction System (80+ tasks across 93 spec files).
- 2026-09-10: S1-T1.1 - Implemented Root CMake build system (CMakeLists.txt, cmake/CompilerFlags.cmake, cmake/toolchain-arm-none-eabi.cmake) with dual-target host/embedded support, strict C99 diagnostics, ASan/UBSan, coverage profiling, and Cortex-M4 toolchain.
- 2026-09-10: S1-T1.2 - Implemented root Makefile with high-level developer orchestration targets (all, test, sim, firmware, coverage, asan, format, check-format, lint, clean, help) and cross-platform OS detection.
- 2026-09-10: S1-T1.3 - Configured root .clang-format enforcing C99 4-space indent, 100-col width, 1TBS/K&R braces, right-aligned pointers, and 8-tier include sorting.
