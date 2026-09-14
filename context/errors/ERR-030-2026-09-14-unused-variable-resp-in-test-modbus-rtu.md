# Error Report: ERR-030 - Unused Variable 'resp' in test_modbus_rtu.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-030` |
| **Date & Time** | 2026-09-14 13:47:33 +05:30 |
| **Commit SHA** | [`4f4e624`](https://github.com/Thejus26/rain-predict/commit/4f4e62470276aab6f1420dacd530d5c422c431c8) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Werror, unused-variable) |
| **Sprint & Task** | Sprint 4 (`S4-T4.1` & `S4-T4.4`) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=unused-variable`) |
| **Impacted Files** | [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c) |

---

## 1. Description & Symptoms

During CI host test compilation (`ninja` with `-Wall -Wextra -Werror`), compilation of `test_modbus_rtu.c` failed with an unused variable diagnostic:

```text
[46/77] Building C object tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o
FAILED: [code=1] tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o 
/usr/bin/cc -DUNITY_INCLUDE_CONFIG_H -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/tests/mocks -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o -MF tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o.d -o tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o -c /home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c: In function ‘test_modbus_parse_resp_byte_count_mismatches’:
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:287:13: error: unused variable ‘resp’ [-Werror=unused-variable]
  287 |     uint8_t resp[11];
      |             ^~~~
cc1: all warnings being treated as errors
[47/77] Building C object tests/CMakeFiles/test_bme280.dir/unit/test_bme280.c.o
[48/77] Linking C static library tests/libunity.a
[49/77] Linking C static library firmware/core/libfirmware_core.a
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c), when resolving `ERR-029`, Case B was updated to use a dedicated array `uint8_t resp_short[10]`, and Case C was added using `uint8_t resp_long[12]`.

The previously allocated stack array `uint8_t resp[11];` declared at the beginning of `test_modbus_parse_resp_byte_count_mismatches()` was no longer referenced anywhere within the function body.

Because the build flags strictly enforce `-Wall -Wextra -Werror`, the GCC compiler emitted `-Werror=unused-variable` and terminated compilation.

---

## 3. Resolution & Code Changes

### Resolution Steps
1. Removed the dead stack array declaration `uint8_t resp[11];` from `test_modbus_parse_resp_byte_count_mismatches()` in [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c).

### Code Diff
```diff
diff --git a/tests/unit/test_modbus_rtu.c b/tests/unit/test_modbus_rtu.c
--- a/tests/unit/test_modbus_rtu.c
+++ b/tests/unit/test_modbus_rtu.c
@@ -284,7 +284,6 @@ static void test_modbus_parse_resp_byte_count_mismatches(void) {
-    uint8_t resp[11];
     uint16_t reg_data[8];
     modbus_exception_t ex = MODBUS_EX_NONE;
```

---

## 4. Verification & Prevention Guidelines

1. **Dead Code & Unused Declaration Audits**:
   - When refactoring unit test function bodies to introduce specific sub-case buffers, ensure obsolete outer variable declarations are cleanly removed.
2. **Strict Compiler Flag Discipline**:
   - Maintain zero warnings under `-Wall -Wextra -Wpedantic -Werror`.
