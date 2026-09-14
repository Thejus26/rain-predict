# Error Report: ERR-031 - Modbus Driver CI Build Failures: Host Unused Variable and Embedded ARM HAL Identifier Errors

## Metadata

| Field | Details |
| :--- | :--- |
| **Error ID** | `ERR-031` |
| **Date & Time** | 2026-09-14 14:08:00 IST |
| **Commit SHA** | [`762a63f`](https://github.com/Thejus26/rain-predict/commit/762a63ff4ca56b7e73643ac50a4740a361a3dbcb) |
| **Sprint / Task** | `Sprint 4: Sensor Drivers & Remote Field Bus Interfaces` / `S4-T4.3` |
| **Severity** | High (CI Blocking Build Failure) |
| **Impacted Files** | [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c)<br>[`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c) |

---

## 1. Description & Symptoms

Continuous integration (CI) failed during both the native host test build and the embedded ARM cross-compilation toolchain on GitHub Actions following commit [`8f6e4fe`](https://github.com/Thejus26/rain-predict/commit/8f6e4fe754812c278bbed5ec609eca98c9e4fb0b):

### Symptom A: Host GCC Build Failure (`test_modbus_rtu.c`)
```text
[48/77] Building C object tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o
FAILED: [code=1] tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o 
/usr/bin/cc -DUNITY_INCLUDE_CONFIG_H ... -Wall -Wextra -Werror ...
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c: In function ‘test_modbus_query_null_and_boundary_guards’:
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:764:26: error: unused variable ‘reading’ [-Werror=unused-variable]
  764 |     modbus_thp_reading_t reading;
      |                          ^~~~~~~
cc1: all warnings being treated as errors
```

### Symptom B: Embedded ARM Toolchain Build Failure (`modbus_rtu.c`)
```text
[7/23] Building C object firmware/drivers/CMakeFiles/firmware_drivers.dir/src/modbus_rtu.c.obj
FAILED: [code=1] firmware/drivers/CMakeFiles/firmware_drivers.dir/src/modbus_rtu.c.obj 
/usr/bin/arm-none-eabi-gcc -DSTM32WLE5xx -DUSE_HAL_DRIVER ... -std=c99 -Wall -Wextra -Werror ...
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c:12:8: error: unknown type name 'UART_HandleTypeDef'
   12 | extern UART_HandleTypeDef huart1;
      |        ^~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c: In function 'delay_us':
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c:33:9: error: implicit declaration of function '__NOP' [-Werror=implicit-function-declaration]
   33 |         __NOP();
      |         ^~~~~
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c: In function 'modbus_set_direction_tx':
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c:408:5: error: implicit declaration of function 'HAL_GPIO_WritePin' [-Werror=implicit-function-declaration]
  408 |     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
      |     ^~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c: In function 'modbus_query_slave_raw':
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c:486:13: error: implicit declaration of function '__HAL_UART_GET_FLAG' [-Werror=implicit-function-declaration]
  486 |     while (!__HAL_UART_GET_FLAG(RS485_UART_HANDLE, UART_FLAG_TC) && (tc_timeout-- > 0U)) {
      |             ^~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/drivers/src/modbus_rtu.c:486:52: error: 'UART_FLAG_TC' undeclared (first use in this function)
  486 |     while (!__HAL_UART_GET_FLAG(RS485_UART_HANDLE, UART_FLAG_TC) && (tc_timeout-- > 0U)) {
      |                                                    ^~~~~~~~~~~~
```

---

## 2. Root Cause Analysis

1. **Unused Local Variable in Unit Test**:
   - In [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c) line 764 (`test_modbus_query_null_and_boundary_guards`), `modbus_thp_reading_t reading;` was allocated on the stack but never passed into any assertions because the test verified `modbus_query_slave_thp(1U, NULL, 150U)` with a literal `NULL` pointer and used `reg_data` for all boundary tests. Under `-Wunused-variable -Werror`, the compiler aborted.

2. **Direct Hardware Invocations Bypassing Driver Layer & Missing CMSIS/HAL Headers**:
   - In [`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c), the driver attempted to directly access `HAL_GPIO_WritePin`, `huart1`, `UART_HandleTypeDef`, and `__NOP()` under `#ifdef STM32WLE5xx` without properly including STM32 HAL/CMSIS headers.
   - More importantly, direct register/HAL access violated project architecture rules ([`coding-standards.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/coding-standards.md) Section 3): protocol drivers must communicate through the [`uart_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/uart_bus.h) hardware abstraction layer (`uart_bus_set_direction()`, `uart_bus_transmit()`, `uart_bus_receive()`, `uart_bus_flush()`), which already encapsulates pin management, guard delays, and TC flag synchronization across both target hardware and host simulation.

---

## 3. Resolution & Code Changes

1. **Fixed `tests/unit/test_modbus_rtu.c`**:
   - Removed the unused `modbus_thp_reading_t reading;` stack variable from `test_modbus_query_null_and_boundary_guards()`, ensuring strict `-Werror=unused-variable` compliance under host GCC.

2. **Refactored `firmware/drivers/src/modbus_rtu.c`**:
   - Removed raw HAL declarations (`UART_HandleTypeDef`, `huart1`, `RS485_UART_HANDLE`, `s_mock_pa1_state`, `HAL_GPIO_WritePin`, `__NOP`, `__HAL_UART_GET_FLAG`, `UART_FLAG_TC`).
   - Refactored `modbus_set_direction_tx()` and `modbus_set_direction_rx()` to delegate directly to `uart_bus_set_direction(UART_PORT_RS485, UART_DIR_TX)` and `uart_bus_set_direction(UART_PORT_RS485, UART_DIR_RX)`.
   - Made `delay_us()` portable using a calibrated volatile decrement loop.
   - Streamlined `modbus_query_slave_raw()` by removing redundant raw HAL TC register polling (handled by `uart_bus_transmit()`).

### Code Diff
```diff
diff --git a/firmware/drivers/src/modbus_rtu.c b/firmware/drivers/src/modbus_rtu.c
index fef54c4..cd64d22 100644
--- a/firmware/drivers/src/modbus_rtu.c
+++ b/firmware/drivers/src/modbus_rtu.c
@@ -7,34 +7,19 @@
 #include "uart_bus.h"
 #include <string.h>
 
-#ifdef STM32WLE5xx
-#include "stm32wlxx_hal.h"
-extern UART_HandleTypeDef huart1;
-#define RS485_UART_HANDLE (&huart1)
-#else
-/* Host unit test simulation state */
-static uint8_t s_mock_pa1_state = 0U;
-#define RS485_UART_HANDLE NULL
-#endif
-
 #define MODBUS_CRC16_INIT_VAL   0xFFFFU
 #define MODBUS_CRC16_POLYNOMIAL 0xA001U
 
 /* ========================================================================== */
-/* Precise Microsecond Delay Utility (@ 48 MHz)                               */
+/* Precise Microsecond Delay Utility                                          */
 /* ========================================================================== */
 
 static void delay_us(uint32_t us)
 {
-#ifdef STM32WLE5xx
-    /* 48 cycles per microsecond at 48 MHz MSI clock */
-    uint32_t count = us * 12U; /* ~4 CPU cycles per loop iteration */
-    while (count-- > 0U) {
-        __NOP();
-    }
-#else
-    (void)us;
-#endif
+    volatile uint32_t count = us * 12U;
+    while (count > 0U) {
+        count--;
+    }
 }
 
 /* ========================================================================== */
@@ -404,22 +389,12 @@ const char *modbus_exception_to_str(modbus_exception_t exception_code)
 
 void modbus_set_direction_tx(void)
 {
-#ifdef STM32WLE5xx
-    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
-#else
-    s_mock_pa1_state = 1U;
-    uart_bus_test_set_direction(UART_PORT_RS485, UART_DIR_TX);
-#endif
+    (void)uart_bus_set_direction(UART_PORT_RS485, UART_DIR_TX);
 }
 
 void modbus_set_direction_rx(void)
 {
-#ifdef STM32WLE5xx
-    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
-#else
-    s_mock_pa1_state = 0U;
-    uart_bus_test_set_direction(UART_PORT_RS485, UART_DIR_RX);
-#endif
+    (void)uart_bus_set_direction(UART_PORT_RS485, UART_DIR_RX);
 }
 
 /* ========================================================================== */
@@ -480,21 +455,13 @@ status_t modbus_query_slave_raw(uint8_t slave_addr,
         return status;
     }
 
-#ifdef STM32WLE5xx
-    /* Step 6: Wait for Hardware Transmission Complete (TC) Flag */
-    uint32_t tc_timeout = 10000U;
-    while (!__HAL_UART_GET_FLAG(RS485_UART_HANDLE, UART_FLAG_TC) && (tc_timeout-- > 0U)) {
-        __NOP();
-    }
-#endif
-
-    /* Step 7: Post-Transmission Guard Delay (35 µs) */
+    /* Step 6: Post-Transmission Guard Delay (35 µs) */
     delay_us(MODBUS_GUARD_TIME_POST_US);
 
-    /* Step 8: De-assert DE=LOW (Receiver Mode Active) */
+    /* Step 7: De-assert DE=LOW (Receiver Mode Active) */
     modbus_set_direction_rx();
 
-    /* Step 9: Await Slave Response with Bounded Software Timeout */
+    /* Step 8: Await Slave Response with Bounded Software Timeout */
     status = uart_bus_receive(UART_PORT_RS485,
                               rx_buf,
                               sizeof(rx_buf),
@@ -504,7 +471,7 @@ status_t modbus_query_slave_raw(uint8_t slave_addr,
         return status;
     }
 
-    /* Step 10: Parse Response Frame, Verify CRC, and Unpack 16-Bit Words */
+    /* Step 9: Parse Response Frame, Verify CRC, and Unpack 16-Bit Words */
     modbus_exception_t exception = MODBUS_EX_NONE;
     status = modbus_parse_read_holding_registers_resp(slave_addr,
                                                       reg_count,
diff --git a/tests/unit/test_modbus_rtu.c b/tests/unit/test_modbus_rtu.c
index b246eec..aa535c5 100644
--- a/tests/unit/test_modbus_rtu.c
+++ b/tests/unit/test_modbus_rtu.c
@@ -761,7 +761,6 @@ static void test_modbus_guard_timing_and_constants(void) {
  */
 static void test_modbus_query_null_and_boundary_guards(void) {
     uint16_t reg_data[8];
-    modbus_thp_reading_t reading;
 
     /* NULL pointers */
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER,
```

---

## 4. Verification & Prevention Guidelines

- Ensure every stack variable declared in unit test functions is actively exercised in an assertion.
- Enforce strict layering: protocol drivers must rely exclusively on underlying bus drivers (`uart_bus.h`, `i2c_bus.h`) rather than invoking raw HAL macros directly.
- Verify cross-compilation builds with both host GCC and `arm-none-eabi-gcc`.

