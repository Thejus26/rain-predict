# Current Feature: S2-T1.3 - Ring Buffer & Moving-Average Filter Unit Test Suites

## Status

In Progress

## Goals

- [x] Create comprehensive ThrowTheSwitch Unity unit test suite in [`tests/unit/test_ring_buffer.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_ring_buffer.c) covering:
  - NULL pointer and invalid parameter boundary validation (`ring_buffer_init`)
  - FIFO ordering with primitive data types (`uint32_t`, `uint8_t`, `uint16_t`)
  - FIFO ordering with structured sensor records (`test_record_t` / 16-byte structs)
  - Full buffer rejection under `RING_BUFFER_DROP_NEW` policy (`STATUS_ERR_BUSY`)
  - Oldest item eviction mechanics under `RING_BUFFER_OVERWRITE_OLD` policy
  - 500–1000 cycle continuous push/pop wrap-around boundary safety
  - Direct indexed non-destructive peeking (`ring_buffer_peek_at`) and out-of-bounds error handling (`STATUS_ERR_INVALID_PARAM`)
  - Housekeeping verification (`ring_buffer_clear`, `ring_buffer_is_empty`, `ring_buffer_is_full`)
- [x] Create comprehensive ThrowTheSwitch Unity unit test suite in [`tests/unit/test_moving_avg.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_moving_avg.c) covering:
  - NULL pointer and zero window size validation (`moving_avg_init`)
  - Sliding window mathematical correctness across window sizes 3, 4, 6, and 12 during warm-up and steady state
  - $O(1)$ constant-time accumulator stability and zero floating-point drift over 10,000 iterations
  - Outlier spike detection and threshold clamping ($100.0^\circ\text{C}$ spike clamped to $25.0^\circ\text{C}$)
  - Priming state indicator transitions (`moving_avg_is_primed`)
  - Negative temperatures and near-zero value precision handling
  - State reset verification (`moving_avg_reset`)
- [x] Register both test executables (`ring_buffer` and `moving_avg`) in [`tests/CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/CMakeLists.txt) linking `firmware_middleware`.
- [x] Execute test suites via CTest and native test binaries with 100% pass rate and 0 compiler warnings under strict flags (`-Wall -Wextra -Wpedantic -Werror`).
- [x] Verify zero memory leaks or uninitialized memory access via ASan/UBSan sanitizers.

## Notes

- **Framework**: ThrowTheSwitch Unity test framework ([`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h)).
- **Target Files**:
  - [`tests/unit/test_ring_buffer.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_ring_buffer.c)
  - [`tests/unit/test_moving_avg.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_moving_avg.c)
  - [`tests/CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/CMakeLists.txt)
- **Memory & Allocation Rules**: Zero dynamic memory allocation (`malloc`/`free` strictly prohibited); all ring buffer storage arrays and filter buffers must be statically allocated test fixtures.
- **Float Comparison**: Single-precision floating-point comparisons must use `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)` with appropriate tolerances ($0.001\text{f}$ for short tests, $0.0001\text{f}$ for drift stability).
- **Target Libraries**: Linked against `firmware_middleware` library target implementing [`firmware/middleware/inc/ring_buffer.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/ring_buffer.h) and [`firmware/middleware/inc/moving_avg_filter.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/moving_avg_filter.h).

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
- 2026-09-10: S1-T1.4 - Configured cloud-native Dev Container (.devcontainer/Dockerfile, devcontainer.json, post-create.sh) and GitHub Actions CI workflow (.github/workflows/ci.yml) for automated linting, host testing, and ARM cross-compilation.
- 2026-09-11: S1-T2.1 - Integrated ThrowTheSwitch Unity test framework (v2.5.x) under tests/unity/ and configured tests/CMakeLists.txt with add_firmware_test() helper function.
- 2026-09-11: S1-T2.2 - Implemented Mock I2C bus driver (tests/mocks/mock_i2c_bus.h/.c), production I2C interface (firmware/drivers/inc/i2c_bus.h), status enum (firmware/core/inc/status.h), and unit tests (tests/unit/test_mock_i2c.c).
- 2026-09-11: S1-T2.3 - Implemented Mock UART bus driver (tests/mocks/mock_uart_bus.h/.c), production UART interface (firmware/drivers/inc/uart_bus.h), RS-485 DE/RE direction guards, SDI-12 ASCII framing, serial fault simulation, and Unity test suite (tests/unit/test_mock_uart.c).
- 2026-09-11: S1-T2.4 - Implemented Mock GPIO & EXTI Pulse Driver (tests/mocks/mock_gpio.h/.c), production GPIO interface (firmware/drivers/inc/gpio_driver.h), power rail gate monitoring, contact bounce/pulse train injection, and Unity test suite (tests/unit/test_mock_gpio.c).
- 2026-09-11: S1-T2.5 - Implemented initial host sanity unit test suite (tests/unit/test_sanity.c) validating Unity assertions, Mock I2C/UART/GPIO loopbacks, EXTI pulse delivery, power rail gating, and single-precision floating-point psychrometric math.
- 2026-09-11: S1-T3.1 - Implemented Python microclimate weather simulator core engine (tools/simulation/simulate_plantation_weather.py) with configurable temporal resolution, WMO hypsometric barometric reduction, CLI parser, and unit test suite (tests/unit/test_simulator.py).
- 2026-09-11: S1-T3.2 - Implemented diurnal physical microclimate curves (tools/simulation/simulate_plantation_weather.py) with clear-sky solar irradiance, asymmetric temperature, Magnus-Tetens psychrometric humidity anti-correlation, 12h semi-diurnal barometric solar tide, sensor noise models, and unit test suite (tests/unit/test_simulator.py).
- 2026-09-11: S1-T3.3 - Implemented convective and orographic storm injection models (tools/simulation/simulate_plantation_weather.py) with pre-monsoon convective storm build-up/rain/dissipation, sustained monsoon profiles, false-alarm cumulus shadow and morning valley fog controls, discrete tipping-bucket accumulator (0.2 mm/tip), and ground-truth lead time tracking.
- 2026-09-12: S1-T3.4 - Implemented multi-format dataset serialization (tools/simulation/simulate_plantation_weather.py) supporting RFC 4180 CSV, JSON telemetry streams, and C99 test vector array headers (tests/unit/simulated_weather_vectors.h), meteorological/tipping-bucket integrity validation, CLI summary reporting, and unit test suite (tests/unit/test_simulator.py). Completed Sprint 1.
- 2026-09-12: S2-T1.1 - Implemented generic static circular FIFO ring buffer (firmware/middleware/inc/ring_buffer.h, ring_buffer.c, firmware/middleware/CMakeLists.txt) supporting arbitrary item sizes, DROP_NEW/OVERWRITE_OLD overflow policies, non-destructive indexed peeking, zero dynamic memory allocation, and Unity test suite (tests/unit/test_ring_buffer.c).
- 2026-09-12: S2-T1.2 - Implemented single-precision floating-point moving average filter (firmware/middleware/inc/moving_avg_filter.h, firmware/middleware/src/moving_avg_filter.c, firmware/middleware/CMakeLists.txt) with O(1) running sum accumulator, statistical outlier spike suppression, configurable sampling window sizes (3, 4, 6, 12 samples), zero-crossing drift protection, and zero dynamic memory allocation.





