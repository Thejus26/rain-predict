# Error Report: ERR-005 - Unity Test Framework Compilation Warnings & Missing stdio.h

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-005` |
| **Date & Time** | `2026-09-11 11:53:22 +0530` |
| **Commit SHA** | [`46f2e54`](https://github.com/Thejus26/rain-predict/commit/46f2e540b4a9ea9ac63e81b3f2d7ae006f918849) |
| **Sprint / Task** | Sprint 1 (S1-T2.1 Unity Test Harness Setup) |
| **Severity** | High (Build Failure under Strict Compiler Flags) |
| **Impacted Files** | [`tests/unity/unity.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.c) |

---

## 1. Description & Symptoms

When building the test framework with `-Wall -Wextra -Wpedantic -Werror`, the build broke with several compiler diagnostics:
1. `implicit declaration of function 'snprintf'` in `UnityPrintFloat()`.
2. `unused variable 'UnityStrColorYellow'` and `unused variable 'UnityStrColorReset'` when color output was not enabled.
3. `ignoring return value of 'snprintf' declared with attribute 'warn_unused_result'`.

---

## 2. Root Cause Analysis

1. `unity.c` used `snprintf` in floating point formatting without explicitly including `<stdio.h>`.
2. Strict C99 flags (`-Wunused-const-variable` and `-Werror`) triggered errors on ANSI escape code constants defined in Unity when `UNITY_OUTPUT_COLOR` was undefined.
3. `snprintf` return value was unread, triggering compiler warnings on GCC/Clang.

---

## 3. Resolution & Code Changes

1. Included `#include <stdio.h>` in `unity.c`.
2. Cast `snprintf` return value to `(void)`.
3. Guarded ANSI color constant usage with `#ifdef UNITY_OUTPUT_COLOR`:

```diff
+#include <stdio.h>
...
 void UnityPrintFloat(const double number)
 {
     char buffer[64];
-    snprintf(buffer, sizeof(buffer), "%.6f", number);
+    (void)snprintf(buffer, sizeof(buffer), "%.6f", number);
     UnityPrint(buffer);
 }
...
 void UnityIgnore(const char* msg, const UNITY_UINT_TYPE line)
 {
     UnityTestResultsBegin(Unity.TestFile, line);
+#ifdef UNITY_OUTPUT_COLOR
+    UnityPrint(UnityStrColorYellow);
+#endif
     UnityPrint(UnityStrIgnore);
+#ifdef UNITY_OUTPUT_COLOR
+    UnityPrint(UnityStrColorReset);
+#endif
```

---

## 4. Verification & Prevention Guidelines

- Third-party test framework sources integrated into the repository must compile cleanly under `-Wall -Wextra -Wpedantic -Werror`.
- Always verify third-party vendor code against strict project compiler configurations before committing.
