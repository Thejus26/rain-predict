# Error Report: ERR-015 - Missing Header Guard for stm32wlxx_hal.h in Host/Embedded Dual Build

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-015` |
| **Date & Time** | `2026-09-13 11:59:10 +0530` |
| **Commit SHA** | [`07bea0e`](https://github.com/Thejus26/rain-predict/commit/07bea0ee361c966bfeeb6865e7e234c12ed92f5a) |
| **Sprint / Task** | Sprint 3 (S3-T1.1 Board Configuration) |
| **Severity** | High (Build Failure during Host vs Target Compilation) |
| **Impacted Files** | [`firmware/core/inc/board_config.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h)<br>[`firmware/core/src/board_config.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/board_config.c) |

---

## 1. Description & Symptoms

When compiling board configuration modules in environments where `STM32WLE5xx` macro was partially defined or when running host simulation tests on non-ARM toolchains, the preprocessor attempted to include `stm32wlxx_hal.h`, resulting in fatal header inclusion errors:
```
fatal error: stm32wlxx_hal.h: No such file or directory
 #include "stm32wlxx_hal.h"
          ^~~~~~~~~~~~~~~~~
```

---

## 2. Root Cause Analysis

The conditional inclusion guard relied solely on `#if defined(STM32WLE5xx) || defined(USE_HAL_DRIVER)`. In dual-target builds (native host x86_64 testing vs ARM Cortex-M cross-compilation), build flags or vendor headers could define symbols without providing physical access to the vendor HAL headers in the include search path.

---

## 3. Resolution & Code Changes

Utilized the C99/C11 `__has_include` preprocessor feature to conditionally define `HAVE_STM32WLXX_HAL` only when `stm32wlxx_hal.h` is physically present in the compiler's include directory list:

```diff
-#if defined(STM32WLE5xx) || defined(USE_HAL_DRIVER)
+#if defined(__has_include)
+#if __has_include("stm32wlxx_hal.h")
+#define HAVE_STM32WLXX_HAL 1
+#endif
+#endif
+
+#if defined(HAVE_STM32WLXX_HAL)
 #include "stm32wlxx_hal.h"
 #else
 /* Standard port/pin types for host unit testing & simulation */
```

---

## 4. Verification & Prevention Guidelines

- In hybrid embedded/host repositories, guard hardware vendor SDK includes with `__has_include` checks.
- Provide clean host fallback type definitions (`GPIO_TypeDef`, `GPIO_PinState`) when hardware HAL headers are unavailable.
