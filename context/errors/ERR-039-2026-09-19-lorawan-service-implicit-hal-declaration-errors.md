# Error Report: ERR-039 - Target Toolchain Implicit HAL Function Declaration Failures in LoRaWAN Service

## Metadata

| Field | Details |
| :--- | :--- |
| **Error ID** | `ERR-039` |
| **Date & Time** | 2026-09-19 15:47:00 IST |
| **Commit SHA** | [`a752ee2`](https://github.com/Thejus26/rain-predict/commit/a752ee20be2f934f0c82d7078bc5d7d78b1e4328) |
| **Sprint / Task** | `Sprint 5: Telemetry Protocol, LoRaWAN & Flash Storage` / `S5-T4.1` |
| **Severity** | High (CI Blocking Target MCU Cross-Compilation Failure) |
| **Impacted Files** | [`firmware/middleware/src/lorawan_service.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/lorawan_service.c)<br>[`firmware/middleware/inc/lorawan_service.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/lorawan_service.h)<br>[`firmware/core/inc/board_config.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h) |

---

## 1. Description & Symptoms

Continuous integration failed during target ARM Cortex-M4 cross-compilation on GitHub Actions (`firmware` workflow):

```text
FAILED: [code=1] firmware/middleware/CMakeFiles/firmware_middleware.dir/src/lorawan_service.c.obj 
/usr/bin/arm-none-eabi-gcc -DSTM32WLE5xx -DUSE_HAL_DRIVER -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -O3 -DNDEBUG -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror -MD -MT firmware/middleware/CMakeFiles/firmware_middleware.dir/src/lorawan_service.c.obj -MF firmware/middleware/CMakeFiles/firmware_middleware.dir/src/lorawan_service.c.obj.d -o firmware/middleware/CMakeFiles/firmware_middleware.dir/src/lorawan_service.c.obj -c /home/runner/work/rain-predict/rain-predict/firmware/middleware/src/lorawan_service.c
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/lorawan_service.c: In function 'lorawan_derive_factory_deveui':
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/lorawan_service.c:61:21: error: implicit declaration of function 'HAL_GetUIDw0' [-Werror=implicit-function-declaration]
   61 |     uint32_t uid0 = HAL_GetUIDw0();
      |                     ^~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/lorawan_service.c:62:21: error: implicit declaration of function 'HAL_GetUIDw1' [-Werror=implicit-function-declaration]
   62 |     uint32_t uid1 = HAL_GetUIDw1();
      |                     ^~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/lorawan_service.c: In function 'lorawan_set_rf_switch':
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/lorawan_service.c:88:13: error: implicit declaration of function 'HAL_GPIO_WritePin' [-Werror=implicit-function-declaration]
   88 |             HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_SET);   /* FE_CTRL1 = HIGH */
      |             ^~~~~~~~~~~~~~~~~
cc1: all warnings being treated as errors
```

---

## 2. Root Cause Analysis

1. **Unconditional Target Macro Gating (`STM32WLE5xx`) vs. HAL Availability (`HAVE_STM32WLXX_HAL`)**:
   - In [`firmware/middleware/src/lorawan_service.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/lorawan_service.c), hardware driver calls were guarded with `#if defined(STM32WLE5xx) || defined(TARGET_MCU)`.
   - The CMake ARM cross-compilation toolchain defines `-DSTM32WLE5xx` for all target firmware compilation units.
   - However, the vendor STM32CubeWL HAL headers are not bundled into the repository; in [`firmware/core/inc/board_config.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h), `HAVE_STM32WLXX_HAL` is conditionally defined only when `__has_include("stm32wlxx_hal.h")` evaluates to true.
   - Because `lorawan_service.c` branched on `STM32WLE5xx` instead of `HAVE_STM32WLXX_HAL`, the compiler attempted to compile the vendor HAL code paths without the corresponding header prototypes.

2. **Direct Invocation of `HAL_GetUIDw0()` and `HAL_GetUIDw1()`**:
   - `HAL_GetUIDw0()` and `HAL_GetUIDw1()` were called directly in `lorawan_derive_factory_deveui()`.
   - Without `stm32wlxx_hal.h` included, these functions have no forward declaration. Under `-Werror=implicit-function-declaration`, GCC immediately terminates compilation.
   - On the STM32WLE5, the factory 96-bit Unique Device ID is accessible at base address `0x1FFF7590U` (UID64 registers `UID_BASE`), which can be safely read via volatile memory-mapped pointers or guarded by `HAVE_STM32WLXX_HAL`.

3. **Bypassing Board Support Package (BSP) Layering for RF Switch Control**:
   - In [`lorawan_set_rf_switch()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/lorawan_service.c#L69), `lorawan_service.c` invoked raw `HAL_GPIO_WritePin()` calls for `PC3`, `PC4`, and `PC5`.
   - The Board Support Package already provides [`board_rf_switch_set(board_rf_mode_t mode)`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h#L270) in [`firmware/core/inc/board_config.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h), which maps the RF truth table and encapsulates the `HAVE_STM32WLXX_HAL` / simulation fallback boundary.
   - Bypassing the BSP violated Layer 3 $\rightarrow$ Layer 1 architectural abstraction and directly caused the undeclared identifier failure for `HAL_GPIO_WritePin`.

---

## 3. Resolution & Code Changes

### Resolution Steps
1. **Encapsulated RF Switch Control via BSP Layer**:
   - Refactored [`lorawan_set_rf_switch()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/lorawan_service.c#L69) to map [`lorawan_rf_mode_t`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/lorawan_service.h#L65) to [`board_rf_mode_t`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h#L186) and delegate hardware pin writes to [`board_rf_switch_set()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h#L270), eliminating all direct `HAL_GPIO_WritePin` calls from the middleware layer.
2. **Standardized HAL Guarding on `HAVE_STM32WLXX_HAL`**:
   - Replaced all `#if defined(STM32WLE5xx) || defined(TARGET_MCU)` and `#if !defined(STM32WLE5xx) && !defined(TARGET_MCU)` checks throughout [`firmware/middleware/src/lorawan_service.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/lorawan_service.c) with `#if defined(HAVE_STM32WLXX_HAL)` and `#if !defined(HAVE_STM32WLXX_HAL)`.
3. **Memory-Mapped UID64 Access**:
   - Updated `lorawan_derive_factory_deveui()` to read hardware 96-bit Unique Device ID registers at fixed memory-mapped address `0x1FFF7590U` when `HAVE_STM32WLXX_HAL` is defined, with seamless fallback to deterministic mock DevEUI for unit testing and builds without vendor HAL.

### Code Diff

```diff
--- a/firmware/middleware/src/lorawan_service.c
+++ b/firmware/middleware/src/lorawan_service.c
@@ -8,7 +8,7 @@
 #include "board_config.h"
 #include <string.h>
 
-#if defined(STM32WLE5xx) || defined(TARGET_MCU)
+#if defined(HAVE_STM32WLXX_HAL)
 #include "stm32wlxx_hal.h"
 #endif
 
@@ -39,8 +39,8 @@ static bool                  s_tx_confirmed = false;
 static uint8_t               s_tx_retry_count = 0U;
 static bool                  s_last_ack_received = false;
 
-/* Mock simulation variables for host test harness */
-#if !defined(STM32WLE5xx) && !defined(TARGET_MCU)
+/* Mock simulation variables for host test harness & builds without vendor HAL */
+#if !defined(HAVE_STM32WLXX_HAL)
 static bool s_mock_join_accept_pending = false;
 static bool s_mock_ack_pending = false;
 #endif
@@ -57,9 +57,11 @@ static void lorawan_derive_factory_deveui(uint8_t *eui) {
         return;
     }
 
-#if defined(STM32WLE5xx) || defined(TARGET_MCU)
-    uint32_t uid0 = HAL_GetUIDw0();
-    uint32_t uid1 = HAL_GetUIDw1();
+#if defined(HAVE_STM32WLXX_HAL)
+    /* Read hardware 96-bit Unique Device ID registers (UID64 / UID_BASE @ 0x1FFF7590) */
+    const volatile uint32_t *p_uid = (const volatile uint32_t *)0x1FFF7590U;
+    uint32_t uid0 = p_uid[0];
+    uint32_t uid1 = p_uid[1];
     eui[0] = (uint8_t)(uid0 >> 24);
     eui[1] = (uint8_t)(uid0 >> 16);
     eui[2] = (uint8_t)(uid0 >> 8);
@@ -82,32 +84,24 @@ static void lorawan_derive_factory_deveui(uint8_t *eui) {
 status_t lorawan_set_rf_switch(lorawan_rf_mode_t mode) {
     s_rf_mode = mode;
 
-#if defined(STM32WLE5xx) || defined(TARGET_MCU)
+    board_rf_mode_t bsp_mode;
     switch (mode) {
         case LORAWAN_RF_MODE_RX:
-            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_SET);
+            bsp_mode = RF_SWITCH_RX;
             break;
         case LORAWAN_RF_MODE_TX_HP:
-            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
+            bsp_mode = RF_SWITCH_TX_HP;
             break;
         case LORAWAN_RF_MODE_TX_LP:
-            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
+            bsp_mode = RF_SWITCH_TX_LP;
             break;
         case LORAWAN_RF_MODE_SHUTDOWN:
         default:
-            HAL_GPIO_WritePin(PIN_FE_CTRL1_PORT, PIN_FE_CTRL1_PIN, GPIO_PIN_RESET);
+            bsp_mode = RF_SWITCH_SHUTDOWN;
             break;
     }
-#endif
 
+    board_rf_switch_set(bsp_mode);
     return STATUS_OK;
 }
```

---

## 4. Verification & Prevention Guidelines

1. **Strict Architectural Layering**:
   - Middleware services (Layer 3) must never call vendor HAL APIs (`HAL_*`) directly when a corresponding Board Support Package (BSP) function exists in Layer 1/2.
2. **Defensive Vendor Header Guards**:
   - All vendor HAL header inclusions and functions must be guarded by `HAVE_STM32WLXX_HAL` (derived from `__has_include("stm32wlxx_hal.h")`), ensuring builds without vendor libraries cleanly fall back to portable register or simulation abstractions.
3. **Continuous Integration Check**:
   - Both host CMake build and target cross-compilation (`-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake`) must be verified for zero warnings under `-Wall -Wextra -Wpedantic -Werror`.
