# Error Report: ERR-049 - Watchdog Refresh Suppressed by Inter-Cycle Tick Advancement in 24-Hour Mission Test

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-049` |
| **Date & Time** | 2026-09-20 18:13:38 +05:30 |
| **Commit SHA** | [`e61344d`](https://github.com/Thejus26/rain-predict/commit/e61344d9acd1b1326c56287fddd4e651f2d2f0fa) |
| **Component / Subsystem** | Testing & Driver Simulation (State Machine / Watchdog Anti-Masking) |
| **Sprint & Task** | Sprint 6 (`S6-T4.1` Full System State Machine End-to-End Integration Tests) |
| **Severity** | High (CI Host Integration Test Failure in `test_state_machine`) |
| **Impacted Files** | [`tests/integration/test_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c)<br>[`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c)<br>[`firmware/app/inc/app_state_machine.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/app_state_machine.h)<br>[`tests/unit/test_app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_app_state_machine.c)<br>[`firmware/core/src/watchdog.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/watchdog.c)<br>[`firmware/middleware/src/power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/power_mgr.c) |

---

## 1. Description & Symptoms

During automated CI execution on the GitHub Actions host test runner (`Run ctest --test-dir build-host --output-on-failure`), integration test suite `test_state_machine` failed with an assertion failure in `test_integration_continuous_24hour_mission`:

```text
/home/runner/work/rain-predict/rain-predict/tests/integration/test_state_machine.c:743:test_integration_continuous_24hour_mission:FAIL: Expected 1 Was 0.  Expected TRUE Was FALSE
```

### Failure Context in `tests/integration/test_state_machine.c`

```c
/**
 * @brief IT-SM-15: Continuous 24-hour mission (96 consecutive 15-min cycles) execution.
 */
static void test_integration_continuous_24hour_mission(void) {
    for (int cycle = 0; cycle < 96; cycle++) {
        s_mock.current_tick_ms += 900000U; /* 15 min */
        TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
    }

    const app_context_t *ctx = app_state_machine_get_context();
    TEST_ASSERT_EQUAL_UINT32(96U, ctx->cycle_count);
    TEST_ASSERT_EQUAL_UINT32(96U, s_mock.lora_tx_count);
    TEST_ASSERT_EQUAL_UINT32(96U, s_mock.flash_records_stored);
    TEST_ASSERT_TRUE(s_mock.watchdog_kicks >= (96U * 8U)); /* Line 743: FAIL */
}
```

- **Expected**: `s_mock.watchdog_kicks >= 768U` (`96U * 8U` kicks across 96 complete 8-state cycles).
- **Actual**: `s_mock.watchdog_kicks == 0U`, causing `TEST_ASSERT_TRUE` to receive `0` (FALSE) instead of `1` (TRUE).

---

## 2. Root Cause Analysis

### Blast Radius & Knowledge Graph Analysis (`graphify`)

Querying the knowledge graph (`& .\.venv\Scripts\graphify.exe query "test_integration_continuous_24hour_mission"`) exposes the operational relationships across the failing test:
- **Test Entry Point**: [`test_integration_continuous_24hour_mission()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c#L733)
- **Primary Orchestrator**: [`app_state_machine_run_cycle()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L477)
- **Watchdog Supervisor**: [`app_watchdog_checkpoint()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L73) calling [`watchdog_refresh()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/watchdog.c#L58)
- **Time Source**: [`power_mgr_get_tick_ms()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c#L317) returning `s_mock.current_tick_ms`
- **Low-Power Mock**: [`power_mgr_enter_stop2()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c#L348)

### Technical Root Cause

In [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c), lines 73–86 implement the watchdog anti-masking stall guard:

```c
static void app_watchdog_checkpoint(app_state_t state) {
    /* Anti-Masking Invariant: Verify state validity and execution time */
    if (state < STATE_MAX) {
        uint32_t current_tick = power_mgr_get_tick_ms();
        uint32_t elapsed      = current_tick - s_state_entry_tick_ms;

        /* Maximum permissible single-state duration before considering it a stall */
        if (elapsed < 200U) {
            watchdog_refresh();
            s_watchdog_kick_count++;
            s_state_entry_tick_ms = current_tick;
        }
    }
}
```

In [`ERR-046`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/ERR-046-2026-09-20-watchdog-hang-recovery-stall-bypassed-in-state-wake.md), the unconditional exception for `STATE_WAKE` (`|| state == STATE_WAKE`) was removed so that execution stalls exceeding $200\text{ ms}$ would properly suppress watchdog refreshes. To prevent false stall detection across normal Stop 2 deep sleep, [`app_exec_sleep()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L368) re-baselined `s_state_entry_tick_ms = power_mgr_get_tick_ms();` immediately after exiting `power_mgr_enter_stop2()`.

However, in [`tests/integration/test_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c):
1. The mock `power_mgr_enter_stop2()` does not advance `s_mock.current_tick_ms`.
2. Instead, `test_integration_continuous_24hour_mission()` simulates the passage of 15 minutes by executing `s_mock.current_tick_ms += 900000U;` inside the test loop **before** calling `app_state_machine_run_cycle()`.
3. When `app_state_machine_run_cycle()` executes `STATE_WAKE`, `app_watchdog_checkpoint(STATE_WAKE)` calculates:
   $$\text{elapsed} = \text{current\_tick} - s\_state\_entry\_tick\_ms = (T + 900000) - T = 900000\text{ ms}$$
4. Since $900000\text{ ms} \ge 200\text{ ms}$, the anti-masking stall guard detects an apparent stall, suppressing `watchdog_refresh()`.
5. Crucially, because `s_state_entry_tick_ms = current_tick;` is inside the `if (elapsed < 200U)` block, `s_state_entry_tick_ms` is never updated.
6. As a result, all 7 subsequent states in that cycle (`STATE_POWER_ON` through `STATE_SLEEP`) also evaluate `elapsed >= 200U` and suppress their kicks.
7. This repeats identically for every one of the 96 cycles, resulting in zero watchdog refreshes for the entire 24-hour mission (`s_mock.watchdog_kicks == 0U`).

Furthermore, in STM32 hardware, `HAL_GetTick()` is driven by the Cortex-M SysTick timer which is completely halted during Stop 2 low-power modes. SysTick does not advance by 900,000 ms during sleep—it only accumulates active MCU run-time (~20 ms per cycle). Inter-cycle sleep duration is tracked exclusively by the RTC. The integration test's artificial increment of `s_mock.current_tick_ms += 900000U` between cycles simulated an active run-time stall rather than low-power deep sleep.

---

## 3. Resolution & Code Changes

### Resolution Steps

1. **Explicit Post-Sleep Wake Re-baselining in State Machine**:
   - In [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c), introduced static flag `s_waking_from_sleep`:
     - Initialized to `false` in `app_state_machine_init()` and `app_state_machine_reset()`.
     - Set to `true` in [`app_exec_sleep()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L399) immediately following the invocation of `power_mgr_enter_stop2()`.
     - In [`app_exec_wake()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c#L106), if `s_waking_from_sleep` is set, re-baselined `s_state_entry_tick_ms = power_mgr_get_tick_ms()` and cleared `s_waking_from_sleep = false` prior to evaluating `app_watchdog_checkpoint(STATE_WAKE)`.
   - This cleanly decouples low-power Stop 2 deep sleep intervals from active runtime stall detection, guaranteeing that legitimate inter-cycle sleep periods do not trigger false watchdog stall detections upon wake, while preserving the cold boot stall detection invariant (`CP_WDG_10`) where `s_waking_from_sleep` remains `false`.

2. **Integration Test Timeline Sequence Realism**:
   - In [`tests/integration/test_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c):[`test_integration_continuous_24hour_mission()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_state_machine.c#L730), adjusted the cycle loop order so that each nominal cycle executes first and advances simulated time (`s_mock.current_tick_ms += 900000U; /* 15 min */`) post-cycle, correctly modeling the physical lifecycle of an operational station (initial measurement at boot $t=0$, followed by 15-minute low-power sleep intervals between cycles).

3. **AST Knowledge Graph Update**:
   - Updated the AST knowledge graph via `graphify update .` using the project's local virtual environment (`.\.venv\Scripts\graphify.exe`).

### Code Diff

```diff
diff --git a/firmware/app/src/app_state_machine.c b/firmware/app/src/app_state_machine.c
index b231231..e95eced 100644
--- a/firmware/app/src/app_state_machine.c
+++ b/firmware/app/src/app_state_machine.c
@@ -49,6 +49,7 @@ static bool           s_initialized         = false;
 static bool           s_boot_watchdog_reset = false;
 static uint32_t       s_watchdog_kick_count = 0U;
 static uint32_t       s_state_entry_tick_ms = 0U;
+static bool           s_waking_from_sleep   = false;
 static bme280_dev_t   s_bme280_dev;
 static opt3001_dev_t  s_opt3001_dev;
 
@@ -105,6 +106,12 @@ static void app_history_push(const env_sample_t *p_sample) {
 /* ========================================================================== */
 
 static status_t app_exec_wake(void) {
+    /* Re-baseline state entry timestamp when waking from Stop 2 deep sleep */
+    if (s_waking_from_sleep) {
+        s_state_entry_tick_ms = power_mgr_get_tick_ms();
+        s_waking_from_sleep   = false;
+    }
+
     /* Checkpoint 1: WAKE */
     app_watchdog_checkpoint(STATE_WAKE);
 
@@ -399,6 +406,7 @@ static status_t app_exec_sleep(void) {
 
     /* 6. Enter Stop 2 deep sleep (< 3.0 uA) */
     (void)power_mgr_enter_stop2(s_app_ctx.configured_sleep_sec);
+    s_waking_from_sleep   = true;
     s_state_entry_tick_ms = power_mgr_get_tick_ms();
     return STATUS_OK;
 }
@@ -420,6 +428,7 @@ status_t app_state_machine_init(void) {
     /* 3. Initialize metrics */
     s_watchdog_kick_count = 0U;
     s_state_entry_tick_ms = power_mgr_get_tick_ms();
+    s_waking_from_sleep   = false;
 
     s_app_ctx.current_state  = STATE_WAKE;
     s_app_ctx.previous_state = STATE_SLEEP;
@@ -514,7 +523,8 @@ void app_state_machine_reset(void) {
     s_boot_watchdog_reset = false;
     s_watchdog_kick_count = 0U;
     s_state_entry_tick_ms = 0U;
-    s_initialized = false;
+    s_waking_from_sleep   = false;
+    s_initialized         = false;
 }
 
 uint32_t app_state_machine_get_watchdog_kick_count(void) {
diff --git a/tests/integration/test_state_machine.c b/tests/integration/test_state_machine.c
index c310e48..a85ec05 100644
--- a/tests/integration/test_state_machine.c
+++ b/tests/integration/test_state_machine.c
@@ -732,8 +732,8 @@ static void test_integration_exti_rain_wake_handling(void) {
  */
 static void test_integration_continuous_24hour_mission(void) {
     for (int cycle = 0; cycle < 96; cycle++) {
-        s_mock.current_tick_ms += 900000U; /* 15 min */
         TEST_ASSERT_EQUAL_INT(STATUS_OK, app_state_machine_run_cycle());
+        s_mock.current_tick_ms += 900000U; /* 15 min */
     }
 
     const app_context_t *ctx = app_state_machine_get_context();
```

---

## 4. Verification & Prevention Guidelines

### Verification Checklist

- [x] Created dedicated git branch `fix/ERR-049-watchdog-refresh-suppressed-in-continuous-24hr-mission`.
- [x] Implemented post-sleep wake re-baselining in `app_state_machine.c` via `s_waking_from_sleep`.
- [x] Aligned `test_integration_continuous_24hour_mission()` cycle execution and sleep timeline.
- [x] Verified `test_integration_continuous_24hour_mission` satisfies `TEST_ASSERT_TRUE(s_mock.watchdog_kicks >= (96U * 8U))` ($768 \ge 768$).
- [x] Verified all 16 integration tests (`IT-SM-01` through `IT-SM-16`) pass without timing regressions.
- [x] Verified all unit tests in `tests/unit/test_app_state_machine.c` (including `CP_WDG_08` stall detection and `CP_WDG_10` simulated hang recovery) continue to pass.
- [x] Updated AST knowledge graph via `& .\.venv\Scripts\graphify.exe update .`.

### Prevention Guidelines

1. **Hardware-Accurate Low-Power Simulation**:
   - In STM32 architectures, SysTick halts during deep sleep modes (Stop 0/1/2, Standby). Never advance SysTick timers across sleep intervals without re-baselining or accounting for the difference between core run-time clocking and RTC calendar time.
2. **Integration Test Timing Realism**:
   - When modeling multi-cycle or multi-day missions in integration test harnesses, synchronize mock clock progression with the power management lifecycle hooks (`power_mgr_enter_stop2` and `power_mgr_wake_restore`).
3. **Explicit Sleep-Wake State Demarcation**:
   - Always track active transition states versus low-power sleep suspension states with explicit state guards (`s_waking_from_sleep`) so that watchdog anti-masking timers only evaluate active CPU execution.
