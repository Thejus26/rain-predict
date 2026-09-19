# Firmware Coding Standards & Guidelines

## 1. Scope & Purpose
This document establishes the official coding standards, architectural guidelines, and engineering practices for the **Tea Plantation Rain Prediction System** firmware. 

Targeting the **STMicroelectronics STM32WLE5** SoC (ARM Cortex-M4 with integrated Sub-GHz LoRa radio) and associated sensors (Bosch BME280/BME680, TI OPT3001, rain gauge), this guide ensures that all code is **reliable, deterministic, energy-efficient, and maintainable** in unattended remote field deployments.

---

## 2. Language Standard & Compiler Toolchain

- **Target Language**: **C99 / C11** for embedded target firmware; **Python 3.10+** for desktop test harnesses, tooling, and data simulation.
- **Compiler**: `arm-none-eabi-gcc` (GNU Arm Embedded Toolchain) or Keil / IAR equivalent.
- **Compiler Flags & Diagnostics**:
  - Code must compile with zero warnings under strict diagnostic flags:
    ```bash
    -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith \
    -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror
    ```
- **Standard Types**: Strictly use `<stdint.h>` and `<stdbool.h>` fixed-width integer types (`uint8_t`, `int16_t`, `uint32_t`, `bool`, etc.). Avoid bare C types like `int`, `long`, `short`, or `unsigned` except for standard loop indices where width is irrelevant.

---

## 3. Architecture & Layering

The firmware adheres to a modular layered architecture with strict unidirectional dependencies:

```
+-------------------------------------------------------------+
|             Application Layer (App / State Machine)         |
|  - Main controller logic & measurement scheduler            |
|  - Rain prediction algorithm (Zambretti / Trend heuristics) |
+------------------------------+------------------------------+
                               |
+------------------------------v------------------------------+
|             Middleware & Services Layer                     |
|  - LoRaWAN Stack / Telemetry Payload Formatter              |
|  - Power Management (Low-Power Sleep / RTC Wakeup)          |
|  - Ring Buffer & Moving-Average Data Filters                |
+------------------------------+------------------------------+
                               |
+------------------------------v------------------------------+
|             Driver / Board Support Package (BSP)            |
|  - Sensor Drivers (BME280, OPT3001, Rain Gauge GPIO)        |
|  - Flash / EEPROM Storage                                   |
+------------------------------+------------------------------+
                               |
+------------------------------v------------------------------+
|             Hardware Abstraction Layer (HAL / LL)           |
|  - STM32CubeWL HAL / LL (I2C, SPI, UART, RTC, SUBGHZ, IWDG)  |
+-------------------------------------------------------------+
```

### Modular Rules
1. **No Circular Dependencies**: Higher layers may include lower layers, but lower layers (e.g., sensor drivers) must never depend on application state.
2. **Hardware Abstraction**: Sensor and algorithm modules must not invoke bare register writes directly; use HAL/LL or board-level wrapper functions.

---

## 4. Naming Conventions

Consistency across files, variables, and functions improves readability and prevents symbol collisions.

| Category | Convention | Example |
| :--- | :--- | :--- |
| **Files** | `snake_case.c` / `snake_case.h` | `bme280_driver.c`, `rain_algo.h` |
| **Functions** | `module_action_target()` (`snake_case`) | `bme280_read_pressure()`, `rain_algo_predict()` |
| **Variables** | `snake_case` | `current_pressure_pa`, `sample_interval_sec` |
| **Global / Static Variables** | `s_` prefix for static, `g_` prefix for extern | `static uint32_t s_last_sample_tick;` |
| **Constants & Macros** | `SCREAMING_SNAKE_CASE` | `LORA_APP_PORT`, `PRESSURE_DROP_THRESHOLD_HPA` |
| **Typedefs / Enums / Structs**| `module_name_t` (`snake_case_t`) | `rain_forecast_state_t`, `sensor_data_t` |
| **Enum Members** | `PREFIX_NAME` | `RAIN_STATE_UNLIKELY`, `RAIN_STATE_IMMINENT` |

---

## 5. Memory Management & Safety

1. **Zero Dynamic Allocation**:
   - `malloc()`, `calloc()`, `realloc()`, and `free()` are **strictly forbidden** in runtime firmware.
   - All buffers, state objects, queues, and packet structures must be statically allocated at compile time.
2. **Deterministic Stack Usage**:
   - Avoid deep recursive function calls.
   - Do not allocate large buffers/arrays on the stack; use static buffers with explicit lifetime bounds.
3. **Pointer Safety & Boundary Checking**:
   - Check all input pointers against `NULL` before dereferencing:
     ```c
     if (p_sensor_data == NULL) {
         return STATUS_ERR_NULL_PTR;
     }
     ```
   - Array index lookups and buffer writes must always verify buffer capacity against `sizeof` or length variables.
4. **Volatile Keyword Usage**:
   - Any variable modified within an Interrupt Service Routine (ISR) and read in the main loop must be marked `volatile`.

---

## 6. Error Handling & Reliability

1. **Standard Status Codes**:
   - Every driver and application function performing I/O, communication, or state updates must return an explicit status enum:
     ```c
     typedef enum {
         STATUS_OK = 0,
         STATUS_ERR_BUSY,
         STATUS_ERR_TIMEOUT,
         STATUS_ERR_I2C,
         STATUS_ERR_UART_BUS,
         STATUS_ERR_CRC_MISMATCH,
         STATUS_ERR_SENSOR_NO_RESPONSE,
         STATUS_ERR_INVALID_PARAM,
         STATUS_ERR_NULL_PTR
     } status_t;
     ```
2. **Never Ignore Return Values**:
   - Return values from HAL calls, I2C/SPI transfers, and sensor conversions must be checked and handled gracefully.
3. **Remote Bus & Decoupled Sensor Communication Rules**:
   - **Mandatory Frame Validation**: All packets from remote sensor probes (e.g., Modbus RTU / SDI-12 / UART) must be verified with CRC-16 or checksum checks before parsing.
   - **Guaranteed Bounded Timeouts**: Every remote query must have a hardware timer or software tick timeout. A detached or damaged sensor cable must never block the MCU state machine or delay watchdog servicing.
   - **Switched Power Rail Sequencing**: When energizing remote sensor rails via high-side switches, enforce a defined stabilization delay before initiating bus transactions, and power down the rail immediately after data acquisition.
4. **Watchdog Management (IWDG)**:
   - The Independent Watchdog (IWDG) must be enabled in production firmware.
   - The watchdog must only be refreshed at well-defined points in the main state loop—never blindly within ISRs or deep loops.
5. **Defensive Assertions**:
   - Use compile-time assertions (`static_assert`) for structure sizes and configuration validity.
   - Use debug-only runtime asserts for invariant checks, which safely degrade to a logged error in release builds.

---

## 7. Low-Power & Timing Best Practices

Because the node is solar/battery powered in remote tea estates, power efficiency is paramount:

1. **No Blocking Busy-Waits**:
   - `HAL_Delay()` and active `while(1)` polling loops are prohibited in operational state flows.
   - Use timer interrupts, DMA completion flags, or event queues.
2. **Duty Cycle Optimization**:
   - Sensor read -> Process / Algorithm -> LoRaWAN Uplink -> Enter Low-Power Stop/Standby Mode.
   - Power down or switch sensor ICs (BME280, OPT3001) to forced/sleep mode between samples.
3. **Pin & Peripheral Hygiene**:
   - Unused GPIO pins must be configured to Analog/No-Pull mode to prevent leakage currents.
   - Peripherals (I2C, SPI, ADC) must have clocks disabled prior to entering deep sleep.
4. **Atomic Operations in Critical Sections**:
   - Guard shared state between ISRs and main context using `__disable_irq()` / `__enable_irq()` (or RTOS critical section primitives) for the shortest possible duration.

---

## 8. Algorithm & Math Guidelines

1. **Floating-Point vs Fixed-Point**:
   - The STM32WLE5 Cortex-M4 core includes a Single-Precision Floating-Point Unit (FPU).
   - Single-precision `float` (32-bit) with hardware acceleration (`-mfloat-abi=hard -mfpu=fpv4-sp-d16`) is permitted for meteorological formulas (e.g., dew-point Magnus formula, Zambretti pressure conversion).
   - Avoid double-precision `double` (64-bit) as it incurs software emulation overhead.
2. **Numerical Stability**:
   - Guard against division by zero and invalid logarithmic/exponential inputs (`log()`, `sqrt()`).
   - Clamp calculations to physically realistic ranges (e.g., RH between 0.0% and 100.0%, Barometric Pressure between 500 hPa and 1100 hPa).
3. **Analytical Ground Truth for Test Vectors**:
   - Reference test vectors in unit test suites must be calculated using the identical canonical formulas and constants defined in the system specifications (e.g., Magnus-Tetens vs Goff-Gratch, WMO hypsometric barometric equations). Never hardcode approximate empirical numbers from external calculators without validating the underlying formula constants.

---

## 9. Code Documentation & Formatting

1. **Doxygen Comments**:
   - All public header functions must have complete Doxygen header comments:
     ```c
     /**
      * @brief  Evaluates localized rainfall probability from pressure and humidity trends.
      * @param[in]  p_history  Pointer to the moving-average environmental history buffer.
      * @param[out] p_forecast Pointer to output forecast structure.
      * @return status_t       STATUS_OK on success, error code otherwise.
      */
     status_t rain_algo_predict(const env_history_t *p_history, rain_forecast_t *p_forecast);
     ```
2. **Code Style Rules**:
   - **Indentation**: 4 spaces (no tabs).
   - **Line Length**: Maximum 100 characters.
   - **Brace Style**: 1TBS / K&R style (opening brace on same line for control structures, new line for functions).
   - **Files**: Every `.h` file must have include guards matching the file name:
     ```c
     #ifndef SENSOR_BME280_H
     #define SENSOR_BME280_H

     #ifdef __cplusplus
     extern "C" {
     #endif

     /* Declarations */

     #ifdef __cplusplus
     }
     #endif

     #endif /* SENSOR_BME280_H */
     ```

---

## 10. Verification, Testing & Tooling

### 10.1 Static Analysis & Automated Formatting
1. **Static Analysis**:
   - Code must pass `cppcheck --enable=all` and `clang-tidy` checks before pull requests are merged.
2. **Automated Formatting**:
   - A `.clang-format` configuration file enforces uniform code formatting (4-space indent, 100-column limit, 1TBS/K&R braces, right-aligned pointers) across all commits.

### 10.2 Git Commit Conventions
1. **Conventional Commits**:
   - Use Conventional Commits format across all commits:
     - `feat(sensor): add bme280 continuous sampling driver`
     - `fix(algo): correct pressure delta sign in falling trend check`
     - `docs(standards): update low-power pin configuration guidelines`

### 10.3 Unit Test Linkage & Scope (Rule on `-Wmissing-prototypes`)
1. **Mandatory `static` Specifier for Test Functions**:
   - Under `-Wmissing-prototypes -Werror`, any function with external linkage that lacks a previous prototype declaration triggers a fatal compilation error.
   - All unit test case functions in `tests/unit/*.c` must be declared with internal linkage using `static void`:
     ```c
     static void test_bme280_forced_mode_conversion(void) {
         /* Test assertions */
     }
     ```
2. **Harness Hook Exceptions**:
   - Only ThrowTheSwitch Unity lifecycle hooks (`void setUp(void)`, `void tearDown(void)` prototyped in `unity.h`) and the test runner entry point (`int main(void)`) retain external linkage.
3. **Internal Helper Functions & Mock Callbacks**:
   - All local helper functions, mock callbacks, and test setup utilities defined in test files must also be declared `static`.

### 10.4 Whitelisted Unity Assertion Macros & Tolerance Guidelines
1. **Prohibition of Non-Existent Integer Tolerance Macros**:
   - Standard ThrowTheSwitch Unity v2.5.x does **not** provide integer tolerance macros (e.g., `TEST_ASSERT_INT_WITHIN`, `TEST_ASSERT_INT16_WITHIN`, `TEST_ASSERT_UINT16_WITHIN`, `TEST_ASSERT_UINT32_WITHIN`). Never use them.
2. **Integer Tolerance Bounds Checking**:
   - To verify integer quantities within an expected tolerance window, use explicit compound boolean assertions:
     ```c
     TEST_ASSERT_TRUE((actual >= (expected - delta)) && (actual <= (expected + delta)));
     ```
3. **Floating-Point Tolerances**:
   - Use `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)` for single-precision floats and `TEST_ASSERT_DOUBLE_WITHIN` for double precision.
4. **Prohibition of `TEST_PASS()`**:
   - Never call `TEST_PASS()`, which is not a standard Unity macro. In Unity, passing tests execute without triggering failing assertions. If an explicit true assert is required, use `TEST_ASSERT_TRUE(true)`.
5. **Array & Buffer Assertions**:
   - For byte arrays, frame buffers, and telemetry payload verification, use `TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, length)`.

### 10.5 Dual-Target Compilation & Hardware Header Preprocessor Gating
1. **Target Header Inclusion Guards**:
   - STM32CubeWL HAL headers (`stm32wlxx_hal.h`), CMSIS device definitions, and hardware peripheral headers must never be included unconditionally in files compiled by host test runners.
   - Guard hardware includes with target preprocessor macros:
     ```c
     #if defined(STM32WLE5xx) || defined(EMBEDDED_TARGET)
     #include "stm32wlxx_hal.h"
     #endif
     ```
2. **Hardware Intrinsics & Factory Calibration Registers**:
   - Direct MCU registers, NVIC interrupt controllers, and factory calibration UID words (e.g., `HAL_GetUIDw0()`, `HAL_GetUIDw1()`) must provide compile-time host mock shims under `tests/mocks/` or be abstracted through BSP accessors.

### 10.6 Mock Driver Behavioral Invariants & Simulation Isolation
1. **Read Idempotency & Side-Effect Freedom**:
   - Mock register reads, address search routines, and diagnostic query APIs must be strictly idempotent and side-effect free.
   - Inspecting an unconfigured I2C/SPI address or querying device status must never alter device state, allocate phantom virtual devices, or mutate control flags (e.g., clearing OPT3001 CRF bit) unless explicitly requested by a simulated write transaction.
2. **Discrete Protocol Frame Boundaries**:
   - Simulated serial bus drivers (USART/RS-485 Modbus RTU, LPUART/SDI-12) must deliver only the exact byte length requested by the transaction.
   - Do not drain or purge entire multi-stage response queues during pre-transmission flushes or retry cycles, which leads to artificial query timeouts in multi-message transactions.

### 10.7 Unused Variables & Strict Diagnostic Hygiene
1. **Zero Unused Variables or Parameters**:
   - Under `-Wall -Wextra -Werror`, any unused local variable or unused parameter causes a fatal build failure.
   - In test files and driver implementations, every declared variable must be asserted, consumed, or explicitly suppressed using `(void)variable;`.
2. **Const Pointer Discipline**:
   - When passing read-only memory buffers to parsers, codecs, and ring buffer push routines, maintain `const` qualification throughout the call chain. Never cast away `const` unless strictly mandated by third-party C interfaces.

