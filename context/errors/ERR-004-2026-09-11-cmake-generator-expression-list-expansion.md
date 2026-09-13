# Error Report: ERR-004 - CMake Generator Expression List Expansion Failure

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-004` |
| **Date & Time** | `2026-09-11 11:51:26 +0530` |
| **Commit SHA** | [`30ef630`](https://github.com/Thejus26/rain-predict/commit/30ef630988e7fa0b30b8594d723dd02c98e49494) |
| **Sprint / Task** | Sprint 1 (S1-T1.1 Root CMake Build System) |
| **Severity** | High (Build System Failure) |
| **Impacted Files** | [`cmake/CompilerFlags.cmake`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/cmake/CompilerFlags.cmake) |

---

## 1. Description & Symptoms

When running `cmake -B build` on host machines, CMake generated a single combined compiler option containing semicolon delimiters (e.g. `"-Wall;-Wextra;-Wpedantic;-Werror"`) instead of separate arguments, causing the C compiler (GCC/Clang) to fail compilation with:
```
gcc: error: -Wall;-Wextra;-Wpedantic;-Werror: No such file or directory
```

---

## 2. Root Cause Analysis

In CMake, `add_compile_options($<$<COMPILE_LANGUAGE:C>:${STRICT_C_FLAGS}>)` evaluates the list `${STRICT_C_FLAGS}` inside a generator expression. When a CMake list variable is evaluated within a single generator expression, it is not automatically expanded into separate list arguments for `add_compile_options`. Instead, it is passed as a single string with semicolons.

---

## 3. Resolution & Code Changes

Iterated through `STRICT_C_FLAGS` with a `foreach` loop, wrapping each individual compiler flag in its own `COMPILE_LANGUAGE:C` generator expression:

```diff
-add_compile_options(
-    $<$<COMPILE_LANGUAGE:C>:${STRICT_C_FLAGS}>
-)
+foreach(FLAG IN LISTS STRICT_C_FLAGS)
+    add_compile_options(
+        $<$<COMPILE_LANGUAGE:C>:${FLAG}>
+    )
+endforeach()
```

---

## 4. Verification & Prevention Guidelines

- When using CMake generator expressions to apply a list of flags to `add_compile_options` or `target_compile_options`, always iterate through the list using `foreach` or use the `$<JOIN:...>` expression.
- Test CMake configuration across native GCC, Clang, and MSVC toolchains.
