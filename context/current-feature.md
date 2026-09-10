# Current Feature: S1-T1.1 - Root CMake Build System Configuration

## Status

In Progress

## Goals

- Create top-level [`CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/CMakeLists.txt) supporting dual-target execution:
  - **Host Mode**: x86_64 / ARM64 native builds for Unity unit testing, mock sensor HAL, and algorithmic simulations (C99, strict warnings, ASan/UBSan, code coverage).
  - **Embedded Firmware Mode**: STM32WLE5CC ARM Cortex-M4 bare-metal cross-compilation (`.elf`, `.hex`, `.bin`, `.map`).
- Create [`cmake/CompilerFlags.cmake`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/cmake/CompilerFlags.cmake) with strict diagnostic flags:
  - `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings`
  - Zero-warning tolerance enforcement via `-Werror` (`ENABLE_WARNINGS_AS_ERRORS`).
  - Sanitizer support (`-fsanitize=address,undefined`) and code coverage flags (`--coverage`).
- Create [`cmake/toolchain-arm-none-eabi.cmake`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/cmake/toolchain-arm-none-eabi.cmake) for bare-metal cross-compilation:
  - Cortex-M4 architecture & FPU flags: `-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`.
  - Linker optimizations: `-Wl,--gc-sections --specs=nano.specs --specs=nosys.specs`.
  - Static library try-compile configuration (`CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY`).
- Structure 4-layer architectural targets (`firmware_core`, `firmware_drivers`, `firmware_middleware`, `firmware_app`) and executable `rain_predict.elf`.
- Integrate host test builds under `tests/` when `BUILD_TESTING=ON` and `CMAKE_CROSSCOMPILING=FALSE`.
- Add custom build targets/commands for `.hex`/`.bin` generation and memory footprint sizing via `arm-none-eabi-size`.
- Validate against test cases TC-S1-T1.1-01 through TC-S1-T1.1-06.

## Notes

- **Target MCU**: STMicroelectronics STM32WLE5CC (ARM Cortex-M4 @ 48 MHz, Single-Precision FPU, 256 KB Flash, 64 KB SRAM).
- **Standards Compliance**: Strict ISO C99 (`CMAKE_C_STANDARD 99`, `CMAKE_C_STANDARD_REQUIRED ON`, `CMAKE_C_EXTENSIONS OFF`).
- **Dependencies**: Foundational root build infrastructure; no upstream dependencies.
- **Downstream Tasks**: `S1-T1.2` (Makefile wrapper), `S1-T2.1` (Unity host test harness), `S2-T1` through `S7-T3` (all firmware modules).
- **Linker Script**: [`firmware/core/src/STM32WLE5XX_FLASH.ld`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/STM32WLE5XX_FLASH.ld).
- **Spec Reference**: [`context/specs/s1-t1.1-root-cmake.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s1-t1.1-root-cmake.md).

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
