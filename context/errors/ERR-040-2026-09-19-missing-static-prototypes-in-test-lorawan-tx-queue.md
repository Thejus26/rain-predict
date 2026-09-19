# Error Report: ERR-040 - Missing static Function Prototypes in test_lorawan_tx_queue.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-040` |
| **Date & Time** | 2026-09-19 16:45:00 +05:30 |
| **Commit SHA** | [`a1e1447`](https://github.com/Thejus26/rain-predict/commit/a1e144787ab78af7911ad30f3a700f071c4b2dc0) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Wmissing-prototypes, -Werror) |
| **Sprint & Task** | Sprint 5 (`S5-T4.3` LoRaWAN Priority TX Queue Manager Unit Test Suite) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=missing-prototypes`) |
| **Impacted Files** | [`tests/unit/test_lorawan_tx_queue.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_lorawan_tx_queue.c)<br>[`firmware/middleware/inc/lorawan_tx_queue.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/lorawan_tx_queue.h)<br>[`firmware/middleware/src/lorawan_tx_queue.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/lorawan_tx_queue.c) |

---

## 1. Description & Symptoms

During CI host test compilation (`ninja` with `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror`), compilation of `test_lorawan_tx_queue.c` failed with 12 `-Wmissing-prototypes` errors:

```text
[60/96] Building C object tests/CMakeFiles/test_lorawan_tx_queue.dir/unit/test_lorawan_tx_queue.c.o
FAILED: [code=1] tests/CMakeFiles/test_lorawan_tx_queue.dir/unit/test_lorawan_tx_queue.c.o 
/usr/bin/cc -DUNITY_INCLUDE_CONFIG_H -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/tests/mocks -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_lorawan_tx_queue.dir/unit/test_lorawan_tx_queue.c.o -MF tests/CMakeFiles/test_lorawan_tx_queue.dir/unit/test_lorawan_tx_queue.c.o.d -o tests/CMakeFiles/test_lorawan_tx_queue.dir/unit/test_lorawan_tx_queue.c.o -c /home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:81:6: error: no previous prototype for ‘test_tc_que_01_empty_queue_process_tick’ [-Werror=missing-prototypes]
   81 | void test_tc_que_01_empty_queue_process_tick(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:100:6: error: no previous prototype for ‘test_tc_que_02_single_periodic_telemetry_enqueue’ [-Werror=missing-prototypes]
  100 | void test_tc_que_02_single_periodic_telemetry_enqueue(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:125:6: error: no previous prototype for ‘test_tc_que_03_urgent_alert_preemption’ [-Werror=missing-prototypes]
  125 | void test_tc_que_03_urgent_alert_preemption(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:173:6: error: no previous prototype for ‘test_tc_que_04_periodic_deduplication_rule’ [-Werror=missing-prototypes]
  173 | void test_tc_que_04_periodic_deduplication_rule(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:195:6: error: no previous prototype for ‘test_tc_que_05_duty_cycle_blocking_enforcement’ [-Werror=missing-prototypes]
  195 | void test_tc_que_05_duty_cycle_blocking_enforcement(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:234:6: error: no previous prototype for ‘test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass’ [-Werror=missing-prototypes]
  234 | void test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:258:6: error: no previous prototype for ‘test_tc_que_07_confirmed_ack_completion_callback’ [-Werror=missing-prototypes]
  258 | void test_tc_que_07_confirmed_ack_completion_callback(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:283:6: error: no previous prototype for ‘test_tc_que_08_confirmed_nack_retry_counter’ [-Werror=missing-prototypes]
  283 | void test_tc_que_08_confirmed_nack_retry_counter(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:315:6: error: no previous prototype for ‘test_tc_que_09_max_retries_dropped_and_callback’ [-Werror=missing-prototypes]
  315 | void test_tc_que_09_max_retries_dropped_and_callback(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:350:6: error: no previous prototype for ‘test_tc_que_10_capacity_overflow_and_null_guards’ [-Werror=missing-prototypes]
  350 | void test_tc_que_10_capacity_overflow_and_null_guards(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:397:6: error: no previous prototype for ‘test_tc_que_11_full_priority_tier_hierarchy’ [-Werror=missing-prototypes]
  397 | void test_tc_que_11_full_priority_tier_hierarchy(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_lorawan_tx_queue.c:437:6: error: no previous prototype for ‘test_tc_que_12_playback_preemption_on_alert’ [-Werror=missing-prototypes]
  437 | void test_tc_que_12_playback_preemption_on_alert(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cc1: all warnings being treated as errors
[61/96] Linking C static library tests/libunity.a
[62/96] Linking C static library firmware/core/libfirmware_core.a
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_lorawan_tx_queue.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_lorawan_tx_queue.c), all 12 unit test functions were declared with global external linkage (`void test_...`) without prior prototype declarations:
- `test_tc_que_01_empty_queue_process_tick`
- `test_tc_que_02_single_periodic_telemetry_enqueue`
- `test_tc_que_03_urgent_alert_preemption`
- `test_tc_que_04_periodic_deduplication_rule`
- `test_tc_que_05_duty_cycle_blocking_enforcement`
- `test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass`
- `test_tc_que_07_confirmed_ack_completion_callback`
- `test_tc_que_08_confirmed_nack_retry_counter`
- `test_tc_que_09_max_retries_dropped_and_callback`
- `test_tc_que_10_capacity_overflow_and_null_guards`
- `test_tc_que_11_full_priority_tier_hierarchy`
- `test_tc_que_12_playback_preemption_on_alert`

Under the project's strict compiler flag configuration (`-std=c99 -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror`), GCC enforces that every function with external linkage must be preceded by a prototype declaration in an included header or file scope. Because these functions are only referenced locally within the test file via `RUN_TEST` inside `main()`, they should be restricted to internal linkage using the `static` storage-class specifier.

---

## 3. Resolution & Code Changes

### Resolution Steps

1. Added `static` storage-class specifiers to all 12 unit test function definitions in [`tests/unit/test_lorawan_tx_queue.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_lorawan_tx_queue.c):
   - `static void test_tc_que_01_empty_queue_process_tick(void)`
   - `static void test_tc_que_02_single_periodic_telemetry_enqueue(void)`
   - `static void test_tc_que_03_urgent_alert_preemption(void)`
   - `static void test_tc_que_04_periodic_deduplication_rule(void)`
   - `static void test_tc_que_05_duty_cycle_blocking_enforcement(void)`
   - `static void test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass(void)`
   - `static void test_tc_que_07_confirmed_ack_completion_callback(void)`
   - `static void test_tc_que_08_confirmed_nack_retry_counter(void)`
   - `static void test_tc_que_09_max_retries_dropped_and_callback(void)`
   - `static void test_tc_que_10_capacity_overflow_and_null_guards(void)`
   - `static void test_tc_que_11_full_priority_tier_hierarchy(void)`
   - `static void test_tc_que_12_playback_preemption_on_alert(void)`
2. Retained external linkage for Unity lifecycle hooks `setUp()` and `tearDown()` (prototyped by `unity.h`) and `main()`.

### Code Diff
```diff
diff --git a/tests/unit/test_lorawan_tx_queue.c b/tests/unit/test_lorawan_tx_queue.c
index b5efabe..a288e7a 100644
--- a/tests/unit/test_lorawan_tx_queue.c
+++ b/tests/unit/test_lorawan_tx_queue.c
@@ -78,7 +78,7 @@ void tearDown(void) {
 /**
  * @brief TC-QUE-01: Empty Queue Process Tick
  */
-void test_tc_que_01_empty_queue_process_tick(void) {
+static void test_tc_que_01_empty_queue_process_tick(void) {
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_init());
 
     status_t st = lorawan_tx_queue_process_step();
@@ -97,7 +97,7 @@ void test_tc_que_01_empty_queue_process_tick(void) {
 /**
  * @brief TC-QUE-02: Single Periodic Telemetry Enqueue
  */
-void test_tc_que_02_single_periodic_telemetry_enqueue(void) {
+static void test_tc_que_02_single_periodic_telemetry_enqueue(void) {
     const uint8_t periodic_buf[12] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                       0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
 
@@ -122,7 +122,7 @@ void test_tc_que_02_single_periodic_telemetry_enqueue(void) {
 /**
  * @brief TC-QUE-03: Urgent Alert Preemption (Priority Inversion)
  */
-void test_tc_que_03_urgent_alert_preemption(void) {
+static void test_tc_que_03_urgent_alert_preemption(void) {
     const uint8_t periodic_buf[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
     const uint8_t alert_buf[4]     = {0xAA, 0xBB, 0xCC, 0xDD};
 
@@ -170,7 +170,7 @@ void test_tc_que_03_urgent_alert_preemption(void) {
 /**
  * @brief TC-QUE-04: Periodic Deduplication Rule
  */
-void test_tc_que_04_periodic_deduplication_rule(void) {
+static void test_tc_que_04_periodic_deduplication_rule(void) {
     const uint8_t frame_a[12] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                                  0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
     const uint8_t frame_b[12] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
@@ -192,7 +192,7 @@ void test_tc_que_04_periodic_deduplication_rule(void) {
 /**
  * @brief TC-QUE-05: Duty Cycle Blocking Enforcement
  */
-void test_tc_que_05_duty_cycle_blocking_enforcement(void) {
+static void test_tc_que_05_duty_cycle_blocking_enforcement(void) {
     /* Arm regional duty-cycle off-time */
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_update_duty_cycle(865062500U, 51U));
@@ -231,7 +231,7 @@ void test_tc_que_05_duty_cycle_blocking_enforcement(void) {
 /**
  * @brief TC-QUE-06: Urgent Alert Duty-Cycle Emergency Bypass
  */
-void test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass(void) {
+static void test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass(void) {
     /* Set active duty off-time */
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_init(LORAWAN_REGION_IN865, 0U));
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_regional_update_duty_cycle(865062500U, 51U));
@@ -255,7 +255,7 @@ void test_tc_que_06_urgent_alert_duty_cycle_emergency_bypass(void) {
 /**
  * @brief TC-QUE-07: Confirmed ACK Completion Callback
  */
-void test_tc_que_07_confirmed_ack_completion_callback(void) {
+static void test_tc_que_07_confirmed_ack_completion_callback(void) {
     const uint8_t payload[16] = {0x0F};
     uint32_t user_magic = 0xCAFEBABE;
 
@@ -280,7 +280,7 @@ void test_tc_que_07_confirmed_ack_completion_callback(void) {
 /**
  * @brief TC-QUE-08: Confirmed NACK Retry Counter
  */
-void test_tc_que_08_confirmed_nack_retry_counter(void) {
+static void test_tc_que_08_confirmed_nack_retry_counter(void) {
     const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
 
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(payload, 8U, true,
@@ -312,7 +312,7 @@ void test_tc_que_08_confirmed_nack_retry_counter(void) {
 /**
  * @brief TC-QUE-09: Max Retries Dropped & Callback Invocation
  */
-void test_tc_que_09_max_retries_dropped_and_callback(void) {
+static void test_tc_que_09_max_retries_dropped_and_callback(void) {
     const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
 
     TEST_ASSERT_EQUAL(STATUS_OK, lorawan_tx_queue_enqueue_playback(payload, 8U, true,
@@ -347,7 +347,7 @@ void test_tc_que_09_max_retries_dropped_and_callback(void) {
 /**
  * @brief TC-QUE-10: Capacity Overflow & NULL Parameter Guards
  */
-void test_tc_que_10_capacity_overflow_and_null_guards(void) {
+static void test_tc_que_10_capacity_overflow_and_null_guards(void) {
     const uint8_t dummy[4] = {1, 2, 3, 4};
 
     /* Fill all 8 queue slots */
@@ -394,7 +394,7 @@ void test_tc_que_10_capacity_overflow_and_null_guards(void) {
 /**
  * @brief Additional Test: MAC Response Enqueue and Full Priority Tier Hierarchy
  */
-void test_tc_que_11_full_priority_tier_hierarchy(void) {
+static void test_tc_que_11_full_priority_tier_hierarchy(void) {
     const uint8_t d[4] = {0x55, 0xAA, 0x55, 0xAA};
 
     /* Enqueue in reverse priority order: Tier 3, Tier 2, Tier 1, Tier 0 */
@@ -434,7 +434,7 @@ void test_tc_que_11_full_priority_tier_hierarchy(void) {
 /**
  * @brief Additional Test: Flash Playback Preemption on Alert Enqueue
  */
-void test_tc_que_12_playback_preemption_on_alert(void) {
+static void test_tc_que_12_playback_preemption_on_alert(void) {
     /* Initialize flash storage and push a telemetry record */
     TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_init());
     uint8_t dummy_telemetry[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
```

---

## 4. Verification & Prevention Guidelines

1. **Internal Linkage for Unity Test Cases**:
   - Always declare all test case functions with `static void test_<name>(void)` in Unity unit test source files.
   - External linkage should only be used when a function is part of a public API defined in a corresponding `.h` header.
2. **Adherence to Coding Standards**:
   - Strictly follow [`context/coding-standards.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/coding-standards.md) Rule 3 on scope and linkage control.
3. **Compiler Flag Parity**:
   - Ensure local developer builds run with `-Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror` to mirror GitHub Actions CI environments before submitting commits.
