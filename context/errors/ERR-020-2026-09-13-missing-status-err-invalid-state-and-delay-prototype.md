# Error Report: ERR-020 - Missing STATUS_ERR_INVALID_STATE Enum & board_test_get_last_delay_ms Prototype

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-020` |
| **Date & Time** | `2026-09-13 16:11:39 +0530` |
| **Commit SHA** | [`c3bc892`](https://github.com/Thejus26/rain-predict/commit/c3bc8920751509c2ec090ea013ee8a9348372875) |
| **Sprint / Task** | Sprint 3 (S3-T4.1 Pre-Sleep GPIO Conditioning) |
| **Severity** | High (Header Declaration Omission / Compilation Failure) |
| **Impacted Files** | [`firmware/core/inc/status.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/status.h)<br>[`firmware/core/inc/board_config.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/board_config.h) |

---

## 1. Description & Symptoms

When building unit tests for pre-sleep GPIO conditioning and power rail stabilization verification, the compiler reported undefined symbols:
1. `error: 'STATUS_ERR_INVALID_STATE' undeclared` in power manager state validation checks.
2. `error: implicit declaration of function 'board_test_get_last_delay_ms' [-Werror=implicit-function-declaration]` in power rail test assertions.

---

## 2. Root Cause Analysis

1. `status.h` contained `STATUS_ERR_INVALID_PARAM` and `STATUS_ERR_NOT_INITIALIZED`, but was missing `STATUS_ERR_INVALID_STATE` for state machine validation errors.
2. `board_config.c` implemented `board_test_get_last_delay_ms()` for host unit testing, but its prototype was omitted from the host simulation section of `board_config.h`.

---

## 3. Resolution & Code Changes

1. Added `STATUS_ERR_INVALID_STATE` to the `status_t` enum in `firmware/core/inc/status.h`.
2. Declared `uint32_t board_test_get_last_delay_ms(void);` in the test API section of `firmware/core/inc/board_config.h`:

```diff
--- a/firmware/core/inc/status.h
+++ b/firmware/core/inc/status.h
@@ -27,6 +27,7 @@ typedef enum {
     STATUS_ERR_INVALID_PARAM,
     STATUS_ERR_NULL_PTR,
     STATUS_ERR_NOT_INITIALIZED,
+    STATUS_ERR_INVALID_STATE,
     STATUS_ERR_OVERFLOW,
 
--- a/firmware/core/inc/board_config.h
+++ b/firmware/core/inc/board_config.h
@@ -347,6 +347,12 @@ board_rf_mode_t board_test_get_rf_mode(void);
  */
 uint32_t board_test_get_delay_call_count(void);
 
+/**
+ * @brief Gets last delay duration passed in milliseconds.
+ * @return uint32_t duration in ms.
+ */
+uint32_t board_test_get_last_delay_ms(void);
```

---

## 4. Verification & Prevention Guidelines

- Ensure every function implemented in `.c` files has a matching prototype in the appropriate `.h` header file.
- Standardize system-wide error return codes in `firmware/core/inc/status.h`.
