# Error Report: ERR-021 - Unclosed Function Brace in power_mgr.c Causing Nested Function Diagnostics

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-021` |
| **Date & Time** | `2026-09-13 17:21:10 +0530` |
| **Commit SHA** | [`51562da`](https://github.com/Thejus26/rain-predict/commit/51562da19c61f0d1f11ee890a5e7608cec424d03) |
| **Sprint / Task** | Sprint 3 (S3-T4.4 Battery ADC Voltage Telemetry & Solar Harvesting Telemetry) |
| **Severity** | High (Build Breakage / Host & Embedded Compilation Failure) |
| **Impacted Files** | [`firmware/middleware/src/power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c) |

---

## 1. Description & Symptoms

During CI / host and cross-compilation builds with GCC (`-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Werror`), compilation of [`firmware/middleware/src/power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c) failed with multiple compiler errors:

```text
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/power_mgr.c: In function ‘power_mgr_verify_leakage_state’:
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/power_mgr.c:641:1: error: ISO C forbids nested functions [-Werror=pedantic]
  641 | uint8_t power_mgr_battery_calc_soc(uint16_t vbat_mv) {
      | ^~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/power_mgr.c:641:9: error: declaration of ‘power_mgr_battery_calc_soc’ shadows a global declaration [-Werror=shadow]
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/power_mgr.c:670:1: error: ISO C forbids nested functions [-Werror=pedantic]
  670 | status_t power_mgr_battery_update(uint16_t ambient_lux) {
...
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/power_mgr.c:756:1: error: expected declaration or statement at end of input
/home/runner/work/rain-predict/rain-predict/firmware/middleware/src/power_mgr.c:758: error: control reaches end of non-void function [-Werror=return-type]
```

---

## 2. Root Cause Analysis

In [`firmware/middleware/src/power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c), the host simulation implementation of [`power_mgr_verify_leakage_state()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c#L530) located inside the `#else` block was missing its closing statement and closing brace (`return STATUS_OK;\n}`) immediately preceding `#endif /* HAVE_STM32WLXX_HAL */`.

As a result, the parser considered [`power_mgr_verify_leakage_state()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c#L530) unclosed when entering the battery telemetry functions appended at the bottom of the file. Under strict C99 diagnostics:
1. Every subsequent function definition (`power_mgr_battery_calc_soc`, `power_mgr_battery_update`, etc.) was treated as a nested function inside `power_mgr_verify_leakage_state`, triggering `-Werror=pedantic` ("ISO C forbids nested functions").
2. The nested function symbols shadowed their header prototypes in [`power_mgr.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/power_mgr.h), triggering `-Werror=shadow`.
3. The enclosing function `power_mgr_verify_leakage_state` never reached a return statement, triggering `-Werror=return-type`.

---

## 3. Resolution & Code Changes

Added `return STATUS_OK;\n}` before `#endif /* HAVE_STM32WLXX_HAL */` to properly terminate [`power_mgr_verify_leakage_state()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c#L530) in the host simulation backend of [`firmware/middleware/src/power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c):

```diff
--- a/firmware/middleware/src/power_mgr.c
+++ b/firmware/middleware/src/power_mgr.c
@@ -632,6 +632,9 @@ status_t power_mgr_verify_leakage_state(void) {
         }
     }
 
+    return STATUS_OK;
+}
+
 #endif /* HAVE_STM32WLXX_HAL */
```

---

## 4. Verification & Prevention Guidelines

1. **Pre-Merge Compiler Check**: Ensure build targets for both host (`build-host`) and embedded target (`build-arm`) compile cleanly with zero warnings under `-Wall -Wextra -Wpedantic -Wshadow -Werror`.
2. **Preprocessor Scoping**: Double-check that all functions declared inside conditional preprocessor blocks (`#if`, `#else`) are fully closed before the `#endif` directive.
