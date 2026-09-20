# Error Report: ERR-046 - Watchdog Hang Recovery Stall Check Bypassed in STATE_WAKE

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-046` |
| **Date & Time** | 2026-09-20 13:42:00 +05:30 |
| **Commit SHA** | [`de11b39`](https://github.com/Thejus26/rain-predict/commit/de11b395c3abb351f222b47cd9d8c76085add16c) |
| **Component / Subsystem** | Application State Machine (Watchdog Anti-Masking) |
| **Sprint & Task** | Sprint 6 (`S6-T3.3` Watchdog Supervised Checkpoints & Recovery) |
| **Severity** | High (CI Host Unit Test Failure in `test_app_state_machine`) |
| **Impacted Files** | [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c)<br>[`firmware/app/inc/app_state_machine.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/app_state_machine.h)<br>[`tests/unit/test_app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_app_state_machine.c)<br>[`firmware/core/src/watchdog.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/watchdog.c)<br>[`firmware/middleware/src/power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c) |

---

## 1. Description & Symptoms

During automated CI execution on GitHub Actions runner (`Run ctest --test-dir build-host --output-on-failure`), unit test suite `test_app_state_machine` failed with an assertion error in `test_wdg_10_simulated_hang_recovery`:

```text
/home/runner/work/rain-predict/rain-predict/tests/unit/test_app_state_machine.c:232:test_wdg_10_simulated_hang_recovery:FAIL: Expected 0 Was 1
```

### Failure Context in `tests/unit/test_app_state_machine.c`

```c
/**
 * @brief CP_WDG_10: Simulated Hang Recovery Cycle.
 */
static void test_wdg_10_simulated_hang_recovery(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());

    /* Advance time by 8.5 seconds without servicing */
    power_mgr_test_set_tick_ms(power_mgr_get_tick_ms() + 8500U);

    /* Next checkpoint detects stall (> 200 ms), suppressing kicks */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_step());
    TEST_ASSERT_EQUAL_UINT32(0U, app_state_machine_get_watchdog_kick_count()); /* Line 232: FAIL */

    /* Simulate resultant MCU reset */
    watchdog_test_set_reset_reason(true);
    app_state_machine_reset();

    /* Reboot detects watchdog reset */
    TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_init());
    TEST_ASSERT_TRUE(app_state_machine_was_boot_watchdog_reset());
}
```

- **Expected**: `0U` (watchdog kick suppressed because elapsed time between init and checkpoint execution was $8500\text{ ms} \ge 200\text{ ms}$, indicating an active execution stall).
- **Actual**: `1U` (watchdog refresh executed and kick counter incremented).

---

## 2. Root Cause Analysis

### Blast Radius & Knowledge Graph Analysis (`graphify`)

Querying the project knowledge graph via `graphify query "app_watchdog_checkpoint"` revealed the call hierarchy and dependency radius:
- **Failing Symbol**: [`app_watchdog_checkpoint()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L73)
- **Callers**: All 8 state execution handlers:
  - [`app_exec_wake()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L107)
  - [`app_exec_power_on()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L125)
  - [`app_exec_sample()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L144)
  - [`app_exec_filter()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L221)
  - [`app_exec_predict()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L257)
  - [`app_exec_transmit()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L291)
  - [`app_exec_alert()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L343)
  - [`app_exec_sleep()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L368)
- **Downstream Collaborators**: [`watchdog_refresh()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/watchdog.c#L58), [`power_mgr_get_tick_ms()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c#L177).
- **Triggering Test**: [`test_wdg_10_simulated_hang_recovery()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_app_state_machine.c#L224).

### Technical Root Cause

In [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c), lines 73–86 define the checkpoint refresh function:

```c
static void app_watchdog_checkpoint(app_state_t state) {
    /* Anti-Masking Invariant: Verify state validity and execution time */
    if (state < STATE_MAX) {
        uint32_t current_tick = power_mgr_get_tick_ms();
        uint32_t elapsed      = current_tick - s_state_entry_tick_ms;

        /* Maximum permissible single-state duration before considering it a stall */
        if (elapsed < 200U || state == STATE_WAKE) {
            watchdog_refresh();
            s_watchdog_kick_count++;
            s_state_entry_tick_ms = current_tick;
        }
    }
}
```

The condition `if (elapsed < 200U || state == STATE_WAKE)` contains an unconditional exemption for `state == STATE_WAKE`:
1. `app_state_machine_init()` initialises `s_app_ctx.current_state = STATE_WAKE` and captures `s_state_entry_tick_ms = power_mgr_get_tick_ms()`.
2. When `test_wdg_10_simulated_hang_recovery()` simulates an 8.5-second hang before the first step via `power_mgr_test_set_tick_ms(power_mgr_get_tick_ms() + 8500U)`, `elapsed` becomes $8500\text{ ms} \ge 200\text{ ms}$.
3. Calling `app_state_machine_step()` invokes `app_exec_wake()` -> `app_watchdog_checkpoint(STATE_WAKE)`.
4. Due to `|| state == STATE_WAKE`, the checkpoint ignores the stall, executes `watchdog_refresh()`, and increments `s_watchdog_kick_count` to `1U`.
5. This violates the anti-masking requirement specified in `S6-T3.3` (Specification: `CP_WDG_10` Simulated Hang Recovery / `CP_WDG_08` Anti-Masking Long State Stall), which mandates that any single-state duration exceeding $200\text{ ms}$ must suppress kicks to allow the hardware IWDG to reset the MCU.

---

## 3. Resolution & Code Changes

### Resolution Steps

1. Removed the blanket `|| state == STATE_WAKE` bypass in [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c):[`app_watchdog_checkpoint()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L73), ensuring that every state checkpoint strictly enforces the $200\text{ ms}$ anti-masking single-state execution limit (`elapsed < 200U`). If execution stalls for $\ge 200\text{ ms}$ before servicing, watchdog refresh is suppressed so that hardware IWDG can trigger recovery.
2. In [`app_exec_sleep()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L368), recorded `s_state_entry_tick_ms = power_mgr_get_tick_ms();` immediately upon exiting `power_mgr_enter_stop2()`. This re-baselines the entry timestamp following legitimate low-power Stop 2 deep sleep periods, preventing sleep duration from triggering false stall detections on subsequent `STATE_WAKE` execution while preserving stall detection during active processing.
3. Updated the AST knowledge graph via `graphify update .`.

### Code Diff

```diff
diff --git a/firmware/app/src/app_state_machine.c b/firmware/app/src/app_state_machine.c
index a298b39..b231231 100644
--- a/firmware/app/src/app_state_machine.c
+++ b/firmware/app/src/app_state_machine.c
@@ -77,7 +77,7 @@ static void app_watchdog_checkpoint(app_state_t state) {
         uint32_t elapsed      = current_tick - s_state_entry_tick_ms;
 
         /* Maximum permissible single-state duration before considering it a stall */
-        if (elapsed < 200U || state == STATE_WAKE) {
+        if (elapsed < 200U) {
             watchdog_refresh();
             s_watchdog_kick_count++;
             s_state_entry_tick_ms = current_tick;
@@ -399,6 +399,7 @@ static status_t app_exec_sleep(void) {
 
     /* 6. Enter Stop 2 deep sleep (< 3.0 uA) */
     (void)power_mgr_enter_stop2(s_app_ctx.configured_sleep_sec);
+    s_state_entry_tick_ms = power_mgr_get_tick_ms();
     return STATUS_OK;
 }
```

---

## 4. Verification & Prevention Guidelines

### Verification Checklist

- [x] Removed unconditional `|| state == STATE_WAKE` bypass in `app_watchdog_checkpoint()`.
- [x] Synchronized `s_state_entry_tick_ms` upon exiting Stop 2 deep sleep in `app_exec_sleep()`.
- [x] Verified `test_wdg_10_simulated_hang_recovery` invariants: active stall $\ge 200\text{ ms}$ suppresses watchdog kick (`app_state_machine_get_watchdog_kick_count() == 0U`).
- [x] Verified `test_wdg_06_resume_post_sleep_kick` invariants: post-sleep wakeup promptly executes 9th checkpoint kick.
- [x] Verified `test_wdg_08_anti_masking_long_state_stall`: single-state stall suppression remains intact.
- [x] Updated AST knowledge graph via `graphify update .`.

### Prevention Guidelines

1. **Uniform Anti-Masking Policy**:
   - Never carve out unconditional per-state bypasses in watchdog checkpoint guards. All active states must adhere to bounded execution timing constraints.
2. **Sleep vs. Stall Demarcation**:
   - Re-baseline operational timing reference points immediately upon exiting low-power sleep modes to prevent low-power sleep intervals from leaking into active run-time stall calculations.
