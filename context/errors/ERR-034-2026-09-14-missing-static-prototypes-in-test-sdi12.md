# Error Report: ERR-034 - Missing static Function Prototypes in test_sdi12.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-034` |
| **Date & Time** | 2026-09-14 14:58:02 +05:30 |
| **Commit SHA** | [`23e4ee5`](https://github.com/Thejus26/rain-predict/commit/23e4ee5c66e9b672ff216d3d909ef8a3ce7b276c) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Wmissing-prototypes, -Werror) |
| **Sprint & Task** | Sprint 4 (`S4-T5.1` SDI-12 Bus Driver) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=missing-prototypes`) |
| **Impacted Files** | [`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c) |

---

## 1. Description & Symptoms

During CI host test compilation (`ninja` with `-Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror`), compilation of `test_sdi12.c` failed with multiple missing prototype diagnostics:

```text
[47/80] Building C object tests/CMakeFiles/test_rain_gauge.dir/unit/test_rain_gauge.c.o
[48/80] Building C object tests/CMakeFiles/test_sdi12.dir/unit/test_sdi12.c.o
FAILED: [code=1] tests/CMakeFiles/test_sdi12.dir/unit/test_sdi12.c.o 
/usr/bin/cc -DUNITY_INCLUDE_CONFIG_H -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/tests/mocks -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_sdi12.dir/unit/test_sdi12.c.o -MF tests/CMakeFiles/test_sdi12.dir/unit/test_sdi12.c.o.d -o tests/CMakeFiles/test_sdi12.dir/unit/test_sdi12.c.o -c /home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:39:6: error: no previous prototype for ‘test_sdi12_init_defaults’ [-Werror=missing-prototypes]
   39 | void test_sdi12_init_defaults(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:50:6: error: no previous prototype for ‘test_sdi12_direction_control’ [-Werror=missing-prototypes]
   50 | void test_sdi12_direction_control(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:60:6: error: no previous prototype for ‘test_sdi12_send_break_and_mark_execution’ [-Werror=missing-prototypes]
   60 | void test_sdi12_send_break_and_mark_execution(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:75:6: error: no previous prototype for ‘test_sdi12_transmit_command_valid’ [-Werror=missing-prototypes]
   75 | void test_sdi12_transmit_command_valid(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:90:6: error: no previous prototype for ‘test_sdi12_transmit_command_missing_exclamation’ [-Werror=missing-prototypes]
   90 | void test_sdi12_transmit_command_missing_exclamation(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:97:6: error: no previous prototype for ‘test_sdi12_transmit_command_empty_and_overflow’ [-Werror=missing-prototypes]
   97 | void test_sdi12_transmit_command_empty_and_overflow(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:109:6: error: no previous prototype for ‘test_sdi12_wake_and_transmit_valid’ [-Werror=missing-prototypes]
  109 | void test_sdi12_wake_and_transmit_valid(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:128:6: error: no previous prototype for ‘test_sdi12_wake_and_transmit_null_and_invalid’ [-Werror=missing-prototypes]
  128 | void test_sdi12_wake_and_transmit_null_and_invalid(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:133:6: error: no previous prototype for ‘test_sdi12_receive_response_valid’ [-Werror=missing-prototypes]
  133 | void test_sdi12_receive_response_valid(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:146:6: error: no previous prototype for ‘test_sdi12_receive_response_missing_crlf’ [-Werror=missing-prototypes]
  146 | void test_sdi12_receive_response_missing_crlf(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:156:6: error: no previous prototype for ‘test_sdi12_receive_response_timeout’ [-Werror=missing-prototypes]
  156 | void test_sdi12_receive_response_timeout(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:165:6: error: no previous prototype for ‘test_sdi12_receive_response_null_and_overflow’ [-Werror=missing-prototypes]
  165 | void test_sdi12_receive_response_null_and_overflow(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:177:6: error: no previous prototype for ‘test_sdi12_deinit’ [-Werror=missing-prototypes]
  177 | void test_sdi12_deinit(void) {
      |      ^~~~~~~~~~~~~~~~~
cc1: all warnings being treated as errors
[49/80] Building C object tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o
[50/80] Linking C static library tests/libunity.a
[51/80] Linking C static library firmware/core/libfirmware_core.a
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c), all unit test runner functions were declared with global linkage (`void test_sdi12_...`) instead of internal linkage (`static void test_sdi12_...`).

Because the project build configuration enforces `-std=c99 -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror`, GCC requires every non-static function with external linkage to have a prior prototype declaration. In the absence of a header or forward declaration, `-Wmissing-prototypes` triggered a compilation failure under `-Werror`.

---

## 3. Resolution & Code Changes

### Resolution Steps
1. Prepended `static` storage-class specifier to all 13 unit test functions in [`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c).
2. Retained global linkage for Unity lifecycle hooks `setUp()` and `tearDown()` as well as `main()`.

### Code Diff
```diff
diff --git a/tests/unit/test_sdi12.c b/tests/unit/test_sdi12.c
index 58ada55..3771f46 100644
--- a/tests/unit/test_sdi12.c
+++ b/tests/unit/test_sdi12.c
@@ -36,7 +36,7 @@ void tearDown(void) {
  * Physical Layer & Direction Control Tests (Task S4-T5.1)
  * ============================================================================ */
 
-void test_sdi12_init_defaults(void) {
+static void test_sdi12_init_defaults(void) {
     /* Verify direction pin PC2 is initialized to LOW (RX Mode) */
     TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
     TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
@@ -47,7 +47,7 @@ void test_sdi12_init_defaults(void) {
     TEST_ASSERT_EQUAL_UINT8(1U, uart_bus_test_get_stop_bits(UART_PORT_SDI12));
 }
 
-void test_sdi12_direction_control(void) {
+static void test_sdi12_direction_control(void) {
     sdi12_set_direction(SDI12_DIR_TX);
     TEST_ASSERT_EQUAL_INT(1, gpio_read_pin(GPIO_PORT_C, 2));
     TEST_ASSERT_EQUAL_INT(SDI12_DIR_TX, sdi12_test_get_direction());
@@ -57,7 +57,7 @@ void test_sdi12_direction_control(void) {
     TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
 }
 
-void test_sdi12_send_break_and_mark_execution(void) {
+static void test_sdi12_send_break_and_mark_execution(void) {
     sdi12_send_break_and_mark();
 
     /* Verify break/mark leaves direction pin in TX mode before command TX */
@@ -72,7 +72,7 @@ void test_sdi12_send_break_and_mark_execution(void) {
     sdi12_set_direction(SDI12_DIR_RX);
 }
 
-void test_sdi12_transmit_command_valid(void) {
+static void test_sdi12_transmit_command_valid(void) {
     status_t status = sdi12_transmit_command("0M!");
     TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
 
@@ -87,14 +87,14 @@ void test_sdi12_transmit_command_valid(void) {
     TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
 }
 
-void test_sdi12_transmit_command_missing_exclamation(void) {
+static void test_sdi12_transmit_command_missing_exclamation(void) {
     /* Missing trailing '!' */
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command("0M"));
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command("0D0"));
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_transmit_command(NULL));
 }
 
-void test_sdi12_transmit_command_empty_and_overflow(void) {
+static void test_sdi12_transmit_command_empty_and_overflow(void) {
     /* Empty command */
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command(""));
 
@@ -106,7 +106,7 @@ void test_sdi12_transmit_command_empty_and_overflow(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_transmit_command(long_cmd));
 }
 
-void test_sdi12_wake_and_transmit_valid(void) {
+static void test_sdi12_wake_and_transmit_valid(void) {
     status_t status = sdi12_wake_and_transmit("0D0!");
     TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
 
@@ -125,12 +125,12 @@ void test_sdi12_wake_and_transmit_valid(void) {
     TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
 }
 
-void test_sdi12_wake_and_transmit_null_and_invalid(void) {
+static void test_sdi12_wake_and_transmit_null_and_invalid(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_wake_and_transmit(NULL));
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_wake_and_transmit("0D0"));
 }
 
-void test_sdi12_receive_response_valid(void) {
+static void test_sdi12_receive_response_valid(void) {
     const char *mock_resp = "00023\r\n";
     (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)mock_resp, (uint16_t)strlen(mock_resp));
 
@@ -143,7 +143,7 @@ void test_sdi12_receive_response_valid(void) {
     TEST_ASSERT_EQUAL_STRING("00023\r\n", rx_buf);
 }
 
-void test_sdi12_receive_response_missing_crlf(void) {
+static void test_sdi12_receive_response_missing_crlf(void) {
     const char *bad_resp = "00023";
     (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)bad_resp, (uint16_t)strlen(bad_resp));
 
@@ -153,7 +153,7 @@ void test_sdi12_receive_response_missing_crlf(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME, status);
 }
 
-void test_sdi12_receive_response_timeout(void) {
+static void test_sdi12_receive_response_timeout(void) {
     mock_uart_inject_fault(UART_PORT_SDI12, MOCK_UART_FAULT_TIMEOUT);
 
     char rx_buf[16] = {0};
@@ -162,7 +162,7 @@ void test_sdi12_receive_response_timeout(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_TIMEOUT, status);
 }
 
-void test_sdi12_receive_response_null_and_overflow(void) {
+static void test_sdi12_receive_response_null_and_overflow(void) {
     char rx_buf[16];
     uint16_t rx_len;
 
@@ -174,7 +174,7 @@ void test_sdi12_receive_response_null_and_overflow(void) {
                           sdi12_receive_response(rx_buf, 2U, &rx_len, 1000U));
 }
 
-void test_sdi12_deinit(void) {
+static void test_sdi12_deinit(void) {
     sdi12_set_direction(SDI12_DIR_TX);
     TEST_ASSERT_EQUAL_INT(STATUS_OK, sdi12_deinit());
     TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
```

---

## 4. Verification & Prevention Guidelines

1. **Internal Linkage for Test Functions**:
   - Always declare unit test case helper and execution functions as `static void test_<name>(void)` in Unity test files.
   - `setUp(void)` and `tearDown(void)` are declared in Unity headers, while test case functions are only referenced inside `main()` within the same translation unit.
2. **Compiler Warning Enforcement**:
   - Keep `-Wmissing-prototypes` and `-Wstrict-prototypes` enabled under `-Werror` to catch unintended global symbol leaks early.
