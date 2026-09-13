# Error Report: ERR-017 - Duplicate GPIO Type Definitions and Enum Re-declaration Conflict

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-017` |
| **Date & Time** | `2026-09-13 12:57:32 +0530` |
| **Commit SHA** | [`0a1c245`](https://github.com/Thejus26/rain-predict/commit/0a1c2453a644b5d10cf3a364497e9dc16e23ab44) |
| **Sprint / Task** | Sprint 3 (S3-T2.1 I2C Bus Driver & S3-T1.1 Board Config Integration) |
| **Severity** | High (Header Definition Conflict / Build Failure) |
| **Impacted Files** | [`firmware/drivers/inc/gpio_driver.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/gpio_driver.h) |

---

## 1. Description & Symptoms

When building driver units that included both `board_config.h` and `gpio_driver.h`, compilation failed with multiple conflicting type and enum redefinition errors:
```
firmware/drivers/inc/gpio_driver.h:25:3: error: redeclaration of enumerator 'GPIO_PORT_A'
firmware/core/inc/board_config.h:45:3: note: previous definition of 'GPIO_PORT_A' was here
firmware/drivers/inc/gpio_driver.h:35:3: error: conflicting types for 'gpio_pin_state_t'
```

---

## 2. Root Cause Analysis

In Sprint 1, `gpio_driver.h` defined placeholder enums for `gpio_port_t`, `gpio_pin_t`, and `gpio_pin_state_t` to support mock GPIO testing. In Sprint 3, `board_config.h` was introduced as the canonical STM32WLE5 hardware pin mapping authority, defining the same enum symbols. Including both headers in a single translation unit caused symbol collisions.

---

## 3. Resolution & Code Changes

Refactored `gpio_driver.h` to include `board_config.h` and removed the redundant duplicate enum definitions:

```diff
-#include "status.h"
+#include "board_config.h"
-
-typedef enum {
-    GPIO_PORT_A = 0,
-    GPIO_PORT_B,
-    GPIO_PORT_C
-} gpio_port_t;
...
```

---

## 4. Verification & Prevention Guidelines

- Maintain a single source of truth for hardware architecture types under `firmware/core/inc/board_config.h`.
- Avoid re-declaring hardware register/pin enums across driver headers; include the common configuration header instead.
