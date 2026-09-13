# Error Report: ERR-009 - Unresolved Math Library Symbols (libm) on Unix/GCC

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-009` |
| **Date & Time** | `2026-09-11 15:48:48 +0530` |
| **Commit SHA** | [`41a0e93`](https://github.com/Thejus26/rain-predict/commit/41a0e932debe68e4b08ca76eaa416e25367e0f80) |
| **Sprint / Task** | Sprint 1 (S1-T1.1 / S1-T2.5 Test Build System) |
| **Severity** | High (Linker Error on Linux/GCC CI) |
| **Impacted Files** | [`tests/CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/CMakeLists.txt) |

---

## 1. Description & Symptoms

When running test builds in GitHub Actions CI (Ubuntu Linux with GCC/Clang), linking `test_sanity` failed with undefined reference errors:
```
/usr/bin/ld: CMakeFiles/test_sanity.dir/unit/test_sanity.c.o: in function 'test_sanity_floating_point_math':
test_sanity.c:(.text+0x1a4): undefined reference to 'expf'
collect2: error: ld returned 1 exit status
```

---

## 2. Root Cause Analysis

On Unix platforms (Linux/BSD), standard C math library functions like `expf()`, `logf()`, `powf()`, and `sqrtf()` reside in `libm` and are not automatically linked by `gcc`/`clang` unless `-lm` is passed to the linker. On Windows (MSVC/MinGW), math functions are part of the standard C runtime library.

---

## 3. Resolution & Code Changes

Added conditional linking of the `m` math library for non-MSVC compilers in the `add_firmware_test()` CMake helper:

```diff
     target_link_libraries(${EXEC_NAME} PRIVATE
         unity
         $<$<TARGET_EXISTS:test_mocks>:test_mocks>
+        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
         ${TEST_LIBRARIES}
     )
```

---

## 4. Verification & Prevention Guidelines

- Ensure any host test targets utilizing `<math.h>` link `-lm` conditionally on non-Windows platforms.
- Verify test runners on Linux CI before merging host algorithmic modules.
