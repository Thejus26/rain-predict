# Current Feature: S2-T2.3 - Hypsometric Barometric Sea-Level Reduction

## Status

In Progress

## Goals

- Implement `dew_point_calc_sea_level_pressure(float station_p_hpa, float temp_c, float altitude_m, float *p_p0_hpa)` utilizing the hypsometric reduction formula $P_0 = P \times \left(1 - \frac{0.0065 \cdot h}{T + 0.0065 \cdot h + 273.15}\right)^{-5.257}$.
- Implement `dew_point_calc_station_pressure_from_p0(float p0_hpa, float temp_c, float altitude_m, float *p_station_p)` for bidirectional barometric verification and sensor calibration.
- Implement `dew_point_calc_pressure_altitude(float station_p_hpa, float p0_hpa, float temp_c, float *p_altitude_m)` computing estimated pressure altitude from local and sea-level barometric differential.
- Include sea-level station direct bypass optimization ($h \le 0.0\text{ m} \implies P_0 = P$).
- Enforce strict single-precision FPU execution (`powf`, `fabsf`), zero dynamic memory allocation, stack frame $< 40$ bytes, and execution latency $< 320$ CPU cycles @ 48 MHz.
- Implement numerical singularity, domain, and range protections: altitude clamping ($[-100.0\text{ m}, +5000.0\text{ m}]$), station pressure validation ($[300.0\text{ hPa}, 1100.0\text{ hPa}]$ returning `STATUS_ERR_OUT_OF_RANGE`), denominator safety bound ($T_{sea\_kelvin} \ge 200.0\text{ K}$), and `NULL`/`NaN` input traps.
- Verify numerical accuracy against ICAO/WMO barometric reduction reference vectors (`TC-HYP-01` through `TC-HYP-10`) within tolerance $\pm 0.05\text{ hPa}$.

## Notes

- **Target Files**:
  - [`firmware/middleware/inc/dew_point.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/dew_point.h)
  - [`firmware/middleware/src/dew_point.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/dew_point.c)
- **Specification Document**: [`context/specs/s2-t2.3-barometric-reduction.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s2-t2.3-barometric-reduction.md)
- **Mathematical Constants**:
  - Tropospheric lapse rate: $\Gamma = 0.0065\text{f}\text{ K/m}$
  - Hypsometric exponent: $\kappa = -5.257\text{f}$ (and inverse $\kappa_{inv} = 0.190222\text{f}$)
  - Kelvin offset: $273.15\text{f}$
- **Domain Constraints**:
  - Station Pressure: $P \in [300.0\text{ hPa}, 1100.0\text{ hPa}]$
  - Station Elevation: $h \in [-100.0\text{ m}, +5000.0\text{ m}]$
  - Temperature: $T \in [-40.0^\circ\text{C}, +85.0^\circ\text{C}]$
- **Downstream Consumers**:
  - Zambretti heuristic forecasting engine (`S2-T3`)
  - Multi-variable gradient trend detector (`S2-T4`)
  - LoRaWAN binary telemetry uplink (`S5-T1`)

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
- 2026-09-12: S2-T1.3 - Implemented comprehensive ThrowTheSwitch Unity test suites (tests/unit/test_ring_buffer.c, tests/unit/test_moving_avg.c, tests/CMakeLists.txt) covering generic FIFO ring buffer ordering, drop/overwrite overflow policies, pointer wrap-around, non-destructive peeking, multi-window moving average math (sizes 3, 4, 6, 12), O(1) stability over 10,000 samples, and outlier spike clamping.
- 2026-09-12: S2-T2.1 - Implemented saturation vapor pressure (es), actual partial vapor pressure (e), absolute humidity (AH), and vapor pressure deficit (VPD) formulas (firmware/middleware/inc/dew_point.h, firmware/middleware/src/dew_point.c) with single-precision floating point, defensive input bounds clamping, NULL/NaN safety guards, and ThrowTheSwitch Unity test suite (tests/unit/test_dew_point.c).
- 2026-09-12: S2-T2.2 - Implemented Magnus-Tetens dew point inversion (Tdew), dew point depression (DPD), direct vapor pressure inversion, and complete psychrometric state calculations (firmware/middleware/inc/dew_point.h, firmware/middleware/src/dew_point.c) with single-precision FPU math, singularity/non-positive logarithm guards, physical invariant clamping, and ThrowTheSwitch Unity unit test suite (tests/unit/test_dew_point.c).






