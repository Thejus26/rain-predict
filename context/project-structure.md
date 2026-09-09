# Project Structure & Repository Layout

## 1. Overview & Architectural Principles

This document defines the official directory structure, modular layering, file naming conventions, and file placement rules for the **Tea Plantation Rain Prediction System** repository.

The repository follows an **Edge-First Embedded Architecture** targeting the **STMicroelectronics STM32WLE5** SoC (ARM Cortex-M4 with integrated Sub-GHz LoRa radio). It enforces strict unidirectional layer dependencies, zero dynamic memory allocation, and isolated host-testable algorithmic units.

```
+-------------------------------------------------------------------------+
|                        Application Layer (App)                          |
|   State machine, measurement scheduler, rain prediction algorithm, alert|
+------------------------------------+------------------------------------+
                                     |
+------------------------------------v------------------------------------+
|                      Middleware & Services Layer                        |
|   LoRaWAN stack, power management, moving-average filters, flash storage|
+------------------------------------+------------------------------------+
                                     |
+------------------------------------v------------------------------------+
|                     Driver / BSP Layer (Drivers)                        |
|   BME280/OPT3001 drivers, rain gauge pulse, RS-485/Modbus, power rails  |
+------------------------------------+------------------------------------+
                                     |
+------------------------------------v------------------------------------+
|                     Core & Hardware Abstraction (HAL)                   |
|   STM32CubeWL HAL/LL, system clocks, startup, vector tables, IWDG       |
+-------------------------------------------------------------------------+
```

---

## 2. Directory Tree Structure

```
rain-predict/
│
├── .agents/                                # AI agent workspace configurations & skills
│   └── skills/                             # Custom slash commands & engineering skills
│       ├── cleanup/                        # Codebase hygiene & linting skill
│       ├── feature/                        # Feature lifecycle management skill
│       ├── list-components/                # Architectural component lister skill
│       ├── research/                       # Hardware/sensor/algorithm research skill
│       └── README.md                       # Skills documentation
│
├── .git/                                   # Git version control metadata
├── .gitignore                              # Git ignore patterns
├── .clang-format                           # Automated C/C++ code formatting rules
├── CMakeLists.txt                          # Top-level CMake build configuration
├── Makefile                                # Makefile alternative for cross-compilation & test
├── README.md                               # Project summary and quick-start guide
│
├── context/                                # Project context, specs, & AI guidelines
│   ├── ai-interaction.md                   # AI workflow and interaction standards
│   ├── coding-standards.md                 # Embedded C coding standards & guidelines
│   ├── current-feature.md                  # Active feature tracker & sprint history
│   ├── project-overview.md                 # System architecture, hardware & algorithm spec
│   ├── project-structure.md                # (This file) Repository layout & file conventions
│   └── archive/                            # Historical design notes and drafts
│
├── docs/                                   # Project technical documentation
│   ├── architecture/                       # Deep-dive architecture and block diagrams
│   ├── hardware/                           # Schematics, pinouts, BOM, solar power sizing
│   ├── sensors/                            # Sensor datasheets, wiring, radiation shield design
│   └── algorithms/                         # Meteorological theory (Zambretti, dew point formulas)
│
├── firmware/                               # Target embedded C firmware source code
│   │
│   ├── app/                                # Layer 4: Application Layer
│   │   ├── inc/                            # Application header files
│   │   │   ├── app_state_machine.h         # Top-level system state machine definitions
│   │   │   ├── measurement_scheduler.h     # Sampling period & duty-cycle scheduler
│   │   │   ├── rain_algo.h                 # Rain prediction algorithm public API
│   │   │   ├── zambretti.h                 # Zambretti barometric heuristic engine
│   │   │   ├── trend_detector.h            # Pressure, humidity, & solar trend classifier
│   │   │   └── alert_manager.h             # Local alert & indicator trigger interface
│   │   └── src/                            # Application source files
│   │       ├── app_state_machine.c         # State machine logic (Wake -> Read -> Predict -> LoRa -> Sleep)
│   │       ├── measurement_scheduler.c     # RTC-driven measurement tick handling
│   │       ├── rain_algo.c                 # Primary rainfall nowcasting coordinator
│   │       ├── zambretti.c                 # Zambretti algorithm implementation
│   │       ├── trend_detector.c            # Moving trend calculation & classification
│   │       └── alert_manager.c             # Buzzer/LED/Relay alert actuation
│   │
│   ├── middleware/                         # Layer 3: Middleware & Services Layer
│   │   ├── inc/                            # Middleware header files
│   │   │   ├── lorawan_service.h           # LoRaWAN uplink/downlink telemetry service
│   │   │   ├── telemetry_codec.h           # Compact binary packet serializer/deserializer
│   │   │   ├── power_mgr.h                 # Sleep mode entry, peripheral power sequencing
│   │   │   ├── ring_buffer.h               # Generic static circular buffer
│   │   │   ├── moving_avg_filter.h         # Moving average and noise filter algorithms
│   │   │   ├── dew_point.h                 # Magnus formula dew-point computation
│   │   │   └── flash_storage.h             # Non-volatile ring-buffer logging on MCU flash
│   │   └── src/                            # Middleware source files
│   │       ├── lorawan_service.c           # LoRaWAN state machine & event handlers
│   │       ├── telemetry_codec.c           # Bit-packed sensor & prediction payload encoder
│   │       ├── power_mgr.c                 # Stop/Standby mode controller & pin conditioning
│   │       ├── ring_buffer.c               # Ring buffer implementation
│   │       ├── moving_avg_filter.c         # Multi-sample moving average filter
│   │       ├── dew_point.c                 # Dew point & vapor pressure calculations
│   │       └── flash_storage.c             # On-chip Flash read/write/wear-leveling routines
│   │
│   ├── drivers/                            # Layer 2: Driver & Board Support Package (BSP) Layer
│   │   ├── inc/                            # Driver header files
│   │   │   ├── bme280_driver.h             # Bosch BME280 (Temp, Humidity, Pressure) driver
│   │   │   ├── opt3001_driver.h            # TI OPT3001 ambient light / solar irradiance driver
│   │   │   ├── rain_gauge_driver.h         # Tipping-bucket pulse counter driver (GPIO IRQ)
│   │   │   ├── modbus_rtu.h                # RS-485 Modbus RTU master framing & CRC-16
│   │   │   ├── sdi12_driver.h              # SDI-12 1200-baud half-duplex bus driver
│   │   │   ├── i2c_bus.h                   # I2C bus wrapper with bounded timeouts
│   │   │   ├── uart_bus.h                  # UART DMA/IRQ wrapper with timeouts
│   │   │   ├── bsp_power_rails.h           # High-side switched sensor power rail control
│   │   │   └── bsp_indicators.h            # Board LEDs, buzzer, and relay outputs
│   │   └── src/                            # Driver source files
│   │       ├── bme280_driver.c             # BME280 register map, compensation formulas
│   │       ├── opt3001_driver.c            # OPT3001 lux conversion & register config
│   │       ├── rain_gauge_driver.c         # Debounced interrupt pulse accumulator
│   │       ├── modbus_rtu.c                # Modbus RTU packet builder & CRC verification
│   │       ├── sdi12_driver.c              # SDI-12 bit-timing & command parser
│   │       ├── i2c_bus.c                   # Non-blocking I2C transfer abstractions
│   │       ├── uart_bus.c                  # Non-blocking UART transfer abstractions
│   │       ├── bsp_power_rails.c           # Load switch GPIO sequencing & delay guards
│   │       └── bsp_indicators.c            # Status LED & alarm indicator toggling
│   │
│   ├── core/                               # Layer 1: Hardware Abstraction & Startup Layer
│   │   ├── inc/                            # Core headers & board configuration
│   │   │   ├── main.h                      # Global hardware definitions & pin mappings
│   │   │   ├── stm32wlxx_hal_conf.h        # STM32CubeWL HAL module configuration
│   │   │   ├── stm32wlxx_it.h              # Interrupt handler prototypes
│   │   │   ├── system_stm32wlxx.h          # System clock frequency declarations
│   │   │   └── board_config.h              # Target board GPIO, clock, and pin mapping
│   │   └── src/                            # Core source files & vector tables
│   │       ├── main.c                      # MCU entry point: clock, HAL, state loop init
│   │       ├── stm32wlxx_hal_msp.c         # MCU Support Package (pin/peripheral init)
│   │       ├── stm32wlxx_it.c              # Interrupt Service Routines (RTC, Radio, UART)
│   │       ├── system_stm32wlxx.c          # CMSIS system initialization & clock tree setup
│   │       ├── startup_stm32wle5xx.s       # Assembly vector table & reset handler
│   │       └── STM32WLE5XX_FLASH.ld        # Linker script (Flash/RAM memory layout)
│   │
│   └── lib/                                # External vendor libraries & SDKs
│       ├── cmsis/                          # ARM CMSIS Cortex-M4 Core headers
│       ├── stm32wlxx_hal/                  # STMicroelectronics STM32WLxx HAL/LL drivers
│       └── subghz_phy/                     # ST Sub-GHz PHY / LoRaWAN middleware stack
│
├── tests/                                  # Host-based & target verification test suite
│   ├── CMakeLists.txt                      # Host test build configuration
│   ├── unity/                              # Unity unit testing framework (vendor)
│   ├── mocks/                              # Mock drivers & HAL stubs for host tests
│   │   ├── mock_i2c_bus.h / .c             # Simulated I2C bus with inject-able responses
│   │   ├── mock_uart_bus.h / .c            # Simulated UART / RS-485 bus
│   │   └── mock_bme280_sensor.h / .c       # Mock BME280 with synthetic pressure curves
│   ├── unit/                               # Pure algorithm & middleware host unit tests
│   │   ├── test_rain_algo.c                # Tests for Zambretti & trend prediction logic
│   │   ├── test_moving_avg.c               # Tests for moving average & noise filters
│   │   ├── test_dew_point.c                # Tests for Magnus dew-point numerical accuracy
│   │   ├── test_telemetry_codec.c          # Tests for payload bit-packing & unpacking
│   │   ├── test_ring_buffer.c              # Tests for circular buffer boundary conditions
│   │   └── test_modbus_crc.c               # Tests for Modbus CRC-16 calculation accuracy
│   └── integration/                        # Multi-module state machine & end-to-end tests
│       └── test_state_machine.c            # Tests for full sample-to-forecast transitions
│
└── tools/                                  # Development, simulation & verification scripts
    ├── simulation/                         # Python scripts for synthetic weather modeling
    │   ├── simulate_plantation_weather.py  # Generates realistic tea estate pressure/RH drops
    │   └── test_forecast_accuracy.py       # Validates prediction algorithm against historical datasets
    ├── decoders/                           # Telemetry payload decoders (for gateways & testing)
    │   ├── payload_decoder.js              # ChirpStack / TTN JavaScript payload formatter
    │   └── payload_decoder.py              # Python decoder for local hub & serial log testing
    └── flashing/                           # Target flashing & serial logging utilities
        ├── flash_firmware.bat / .sh        # OpenOCD / STM32CubeProgrammer flash script
        └── serial_logger.py                # Serial telemetry stream logger with timestamps
```

---

## 3. Layer Breakdown & Responsibilities

### 3.1 Application Layer (`firmware/app/`)
- **Authority**: Highest-level business logic and decision-making.
- **Responsibilities**:
  - Orchestrates the state machine: Wake -> Power Rails On -> Sample Sensors -> Filter Data -> Run Prediction -> Format Telemetry -> Transmit LoRa -> Local Alert -> Power Rails Off -> Deep Sleep.
  - Implements localized rainfall prediction heuristics (Zambretti index combined with rapid 3-hour pressure and humidity gradients).
  - Triggers on-board indicators (LEDs, relays, buzzers) for immediate field worker warning.
- **Dependencies**: May include `middleware/`, `drivers/`, and `core/`. Never included by lower layers.

### 3.2 Middleware & Services Layer (`firmware/middleware/`)
- **Authority**: Protocol engines, mathematical filters, power governance, and data structures.
- **Responsibilities**:
  - Manages LoRaWAN uplink packet queues, duty cycle compliance, and channel hopping.
  - Serializes floating-point weather data into compact bit-packed LoRaWAN frames.
  - Manages low-power MCU sleep states (Stop 2 / Standby) and RTC wake timers.
  - Maintains static ring buffers and moving-average windows for environmental trends.
  - Calculates meteorological derivations (dew point, vapor pressure, sea-level pressure).
- **Dependencies**: May include `drivers/` and `core/`. Does not depend on `app/`.

### 3.3 Driver / Board Support Package Layer (`firmware/drivers/`)
- **Authority**: Hardware peripheral communication and external sensor IC abstractions.
- **Responsibilities**:
  - Interfaces with BME280 (temperature, humidity, barometric pressure) via I2C or SPI.
  - Interfaces with OPT3001 (ambient lux) to detect storm cloud attenuation.
  - Manages pulse-counting interrupt handlers for tipping-bucket rain gauges.
  - Implements RS-485 Modbus RTU / SDI-12 drivers with bounded timeouts and CRC verification for remote mast probes.
  - Controls high-side load switches for sensor power rails to eliminate standby leakage.
- **Dependencies**: Depends only on `core/` (HAL/LL). Hardware-agnostic drivers can be unit-tested using stubs.

### 3.4 Core & Hardware Abstraction Layer (`firmware/core/`)
- **Authority**: Silicon-level hardware configuration and CMSIS startup.
- **Responsibilities**:
  - Configures system clocks (MSI / HSE / PLL), GPIO muxing, and DMA controllers.
  - Houses the vector table, reset handlers, and linker memory layout.
  - Configures the Independent Watchdog (IWDG) and low-power hardware timers (LPTIM/RTC).
- **Dependencies**: CMSIS and STM32CubeWL HAL.

---

## 4. File Placement & Naming Conventions

### 4.1 Naming Rules
- **Files**: All filenames must use strict `snake_case` (e.g., `rain_algo.c`, `bme280_driver.h`).
- **Headers & Source Separation**:
  - Header files (`.h`) reside in `inc/` subdirectories for public module APIs.
  - Source files (`.c`) reside in `src/` subdirectories.
  - Private helper functions must remain static within `.c` files and not exposed in headers.
- **Include Guards**: Every header file must use standard macro include guards formatted as `MODULE_NAME_H`:
  ```c
  #ifndef BME280_DRIVER_H
  #define BME280_DRIVER_H

  #ifdef __cplusplus
  extern "C" {
  #endif

  /* Function declarations and types */

  #ifdef __cplusplus
  }
  #endif

  #endif /* BME280_DRIVER_H */
  ```

### 4.2 Module Creation Checklist
When creating a new firmware module or driver:
1. Create header `firmware/<layer>/inc/<module_name>.h` with Doxygen comments and `status_t` return types.
2. Create source `firmware/<layer>/src/<module_name>.c` implementing functions with defensive assertions and zero dynamic allocations.
3. If the module contains pure mathematical or algorithmic logic, create a corresponding unit test in `tests/unit/test_<module_name>.c`.
4. Update `CMakeLists.txt` or `Makefile` to include the new source and header paths.

---

## 5. Testing & Tooling Structure

### 5.1 Host Unit Tests (`tests/unit/`)
- Built using **Unity** on host machines (x86_64 / PC) using GCC / Clang.
- Runs without requiring physical STM32 hardware or emulators.
- Validates:
  - Mathematical correctness of Zambretti and dew point formulas.
  - Bit-packing integrity of telemetry encoders.
  - Circular buffer overflow and underflow handling.
  - Modbus CRC-16 calculation against test vectors.

### 5.2 Simulation & Analysis Tools (`tools/simulation/`)
- Python-based simulation tools generate synthetic weather microclimate scenarios (clear sky, sudden squall, steady monsoon rain, high-altitude temperature inversions).
- Validates the prediction algorithm against simulated and historical tea estate records.

---

## 6. Summary of Architectural Constraints

| Constraint | Rule | Reason |
| :--- | :--- | :--- |
| **Memory Allocation** | **Zero dynamic allocation** (`malloc`/`free` prohibited) | Prevents memory fragmentation and leaks in unattended field operation. |
| **Layer Coupling** | Unidirectional downward calls only | Maintains modularity and enables isolated unit testing. |
| **Remote Communication** | Bounded timeouts + CRC verification on all external buses | Damaged or detached sensor cables must never hang the MCU or trigger watchdog resets. |
| **Power Management** | Mandatory sensor power rail gating & peripheral shutdown | Ensures long-term solar/battery autonomy in remote tea estates. |
| **Math Precision** | Single-precision `float` with Cortex-M4 FPU | Prevents 64-bit software emulation overhead while maintaining meteorological precision. |
