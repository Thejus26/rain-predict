# Current Feature: S2-T2.2 - Dew Point Temperature & Relative Humidity Depression

## Status

In Progress

## Goals

- Implement `dew_point_calc_tdew(float temp_c, float rh_pct, float *p_dew_c)` in Layer 3 middleware utilizing intermediate parameter $\gamma(T, RH) = \ln(RH/100) + (17.67 \cdot T)/(T + 243.5)$ and analytical inversion $T_{dew} = (243.5 \cdot \gamma) / (17.67 - \gamma)$.
- Implement `dew_point_calc_depression(float temp_c, float rh_pct, float *p_dpd_c)` calculating $\Delta T_{dep} = \max(0.0\text{f}, T - T_{dew})$.
- Implement `dew_point_calc_from_vapor_pressure(float e_hpa, float *p_dew_c)` computing dew point temperature directly from known actual partial vapor pressure $e$.
- Implement `dew_point_calc_psychrometric_state(float temp_c, float rh_pct, psychrometric_state_t *p_state)` aggregating $e_s$, $e$, $\text{VPD}$, $\text{AH}$, $T_{dew}$, and $\Delta T_{dep}$ in a single validated invocation.
- Enforce strict single-precision FPU execution (`logf`, `fabsf`), zero dynamic memory allocation, stack frame $< 48$ bytes, and execution latency $< 280$ CPU cycles @ 48 MHz.
- Implement singularity and boundary protections: denominator guard ($|17.67 - \gamma| < 10^{-4}\text{f}$), non-positive logarithm clamping ($RH \ge 0.1\%$, $e \ge 0.1\text{ hPa}$), and physical invariants ($T_{dew} \le T$, $\Delta T_{dep} \ge 0.0^\circ\text{C}$).
- Include robust input parameter validation (NULL pointer checks returning `STATUS_ERR_NULL_PTR`, `isnan()` checks returning `STATUS_ERR_INVALID_ARG`, and temperature/humidity domain clamping).
- Verify numerical accuracy against NOAA/Smithsonian psychrometric reference test vectors (TC-DP-01 through TC-DP-11) within tolerance $\pm 0.05^\circ\text{C}$.

## Notes

- **Target Files**:
  - [`firmware/middleware/inc/dew_point.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/dew_point.h)
  - [`firmware/middleware/src/dew_point.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/dew_point.c)
- **Specification Document**: [`context/specs/s2-t2.2-dew-point-depression.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s2-t2.2-dew-point-depression.md)
- **Mathematical Constants**:
  - Magnus coefficients: $a = 6.112\text{f}\text{ hPa}$, $b = 17.67\text{f}$, $c = 243.5\text{f}$
  - Absolute humidity constant: $C = 216.7\text{f}\text{ g}\cdot\text{K}/\text{J}$
  - Kelvin offset: $273.15\text{f}$
  - Denominator epsilon: $10^{-4}\text{f}$
- **Domain Constraints**:
  - $T \in [-40.0^\circ\text{C}, +85.0^\circ\text{C}]$
  - $RH \in [0.1\%, 100.0\%]$
  - Actual Vapor Pressure $e \ge 0.1\text{ hPa}$
- **Agronomic & Meteorological Significance**:
  - Lifting Condensation Level (LCL) / Cloud Base: $H_{cloud\_base} \approx 125 \times (T - T_{dew})\text{ [m]}$
  - Canopy saturation & blister blight trigger: $\Delta T_{dep} \le 0.5^\circ\text{C}$
  - Convective squall precursor: Rapid collapse of $\Delta T_{dep}$ over 30–60 min

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






