# Error Report: ERR-042 - Battery Conservation Tier 2 LED Pattern Overridden by Storm Strobe

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-042` |
| **Date & Time** | 2026-09-20 11:20:00 +05:30 |
| **Commit SHA** | [`63eb0e7`](https://github.com/Thejus26/rain-predict/commit/63eb0e7473c40cec1df4bebe7138108391abf4cf) |
| **Component / Subsystem** | Application & Middleware (Alert Manager) |
| **Sprint & Task** | Sprint 6 (`S6-T2.1` Status LED Patterns & `S6-T2.3` Alert Manager Unit Tests) |
| **Severity** | High (CI Host Unit Test Runner Failure in `test_alert_manager`) |
| **Impacted Files** | [`firmware/app/src/alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c)<br>[`firmware/app/inc/alert_manager.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/alert_manager.h)<br>[`tests/unit/test_alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_alert_manager.c) |

---

## 1. Description & Symptoms

During automated CI execution on GitHub Actions runner (`Run ctest --test-dir build-host --output-on-failure`), unit test `test_alert_manager` failed with an assertion mismatch in `test_alert_battery_conservation_tier2`:

```text
/home/runner/work/rain-predict/rain-predict/tests/unit/test_alert_manager.c:421:test_alert_battery_conservation_tier2:FAIL: Expected 7 Was 4
```

### Failure Context in `tests/unit/test_alert_manager.c`

```c
static void test_alert_battery_conservation_tier2(void) {
    alert_input_t in = {
        .rain_state       = RAIN_ALERT_IMMINENT,
        .cpi_pct          = 85.0f,
        .battery_tier     = BATTERY_TIER_CONSERVATION, /* Vbat < 3.10V */
        .rtc_hour_0_to_23 = 14U
    };
    TEST_ASSERT_EQUAL_INT(STATUS_OK, alert_manager_update(&in));

    /* In Conservation mode, storm strobe is throttled down to conservation micro-pulse */
    TEST_ASSERT_EQUAL_INT(ALERT_LED_PATTERN_CONSERVATION, alert_manager_get_active_led_pattern()); /* Line 421: FAIL */
```

- Expected: `ALERT_LED_PATTERN_CONSERVATION` (enum integer value `7` - Green micro-pulse $10\text{ ms}$ every $5000\text{ ms}$, $8\,\mu\text{A}$ average current).
- Actual: `ALERT_LED_PATTERN_IMMINENT_STROBE` (enum integer value `4` - Red rapid strobe $10\text{ Hz}$, $50\%$ duty cycle, $4.0\text{ mA}$ average current).

---

## 2. Root Cause Analysis

### Blast Radius & Knowledge Graph Analysis (`graphify`)

Traversal of `graphify-out/graph.json` reveals the dependency topology for the failing component:
- **Symbol**: [`alert_mgr_resolve_led_pattern()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c#L89)
- **Direct Caller**: [`alert_manager_update()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c#L203)
- **Triggering Test**: [`test_alert_battery_conservation_tier2()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_alert_manager.c#L411)
- **Downstream Consumers**: Application State Machine (`app_state_machine.c` in `STATE_ALERT`), Indicator Driver ([`bsp_indicators.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/bsp_indicators.h)).

### Technical Root Cause

In [`firmware/app/src/alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c), function `alert_mgr_resolve_led_pattern()` implemented the pattern selection ladder in the following priority order:

```c
static alert_led_pattern_t alert_mgr_resolve_led_pattern(const alert_input_t *p_in) {
    if (p_in->is_sleeping || p_in->battery_tier == BATTERY_TIER_CRITICAL) {
        return ALERT_LED_PATTERN_OFF;
    }
    if (p_in->sensor_fault) {
        return ALERT_LED_PATTERN_SYSTEM_FAULT;
    }
    if (p_in->rain_state == RAIN_ALERT_IMMINENT || p_in->cpi_pct >= 80.0f) {
        return ALERT_LED_PATTERN_IMMINENT_STROBE;
    }
    if (p_in->rain_pulses_recent > 0U) {
        return ALERT_LED_PATTERN_ACTIVE_RAIN;
    }
    if (p_in->rain_state == RAIN_ALERT_LIKELY || p_in->cpi_pct >= 60.0f) {
        return ALERT_LED_PATTERN_WARNING_RED;
    }
    if (p_in->rain_state == RAIN_ALERT_POSSIBLE || p_in->cpi_pct >= 30.0f) {
        return ALERT_LED_PATTERN_WATCH_AMBER;
    }
    if (p_in->battery_tier == BATTERY_TIER_CONSERVATION) {
        return ALERT_LED_PATTERN_CONSERVATION;
    }
    return ALERT_LED_PATTERN_HEALTHY_PULSE;
}
```

The check for `p_in->battery_tier == BATTERY_TIER_CONSERVATION` was placed at the very bottom of the cascade, subordinate to all meteorological alert states (`RAIN_ALERT_IMMINENT`, `RAIN_ALERT_LIKELY`, and `RAIN_ALERT_POSSIBLE`). 

When `alert_manager_update(&in)` evaluated an input state with both `battery_tier == BATTERY_TIER_CONSERVATION` and `rain_state == RAIN_ALERT_IMMINENT` ($CPI=85.0\%$), the `RAIN_ALERT_IMMINENT` condition matched first, returning `ALERT_LED_PATTERN_IMMINENT_STROBE`.

This priority inversion directly violates the system power preservation architecture ([`docs/architecture/power-architecture.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/docs/architecture/power-architecture.md) and [`context/specs/s6-t1.2-battery-preservation-throttling.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s6-t1.2-battery-preservation-throttling.md)):
- Under Battery Tier 2 Conservation ($2.90\text{ V} \le V_{bat} < 3.10\text{ V}$), all high-power actuation (siren relay, buzzer, and continuous LED blinking/strobing) must be suppressed or throttled down to a $10\text{ ms}$ micro-pulse ($8\,\mu\text{A}$ average drain) to avoid brownout and prevent lithium battery depletion.
- Therefore, `BATTERY_TIER_CONSERVATION` must take precedence over weather alert patterns.

---

## 3. Resolution & Code Changes

### Resolution Steps

1. Repositioned the `p_in->battery_tier == BATTERY_TIER_CONSERVATION` evaluation in [`firmware/app/src/alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c):[`alert_mgr_resolve_led_pattern()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c#L89) immediately following the system fault guard and before any meteorological conditions (`RAIN_ALERT_IMMINENT`, `rain_pulses_recent`, `RAIN_ALERT_LIKELY`, `RAIN_ALERT_POSSIBLE`).
2. When the battery enters Tier 2 Conservation mode ($2.90\text{ V} \le V_{bat} < 3.10\text{ V}$), the pattern resolver now deterministically returns `ALERT_LED_PATTERN_CONSERVATION` (Green micro-pulse $10\text{ ms}$ ON / $4990\text{ ms}$ OFF), regardless of weather urgency, protecting the system against excessive power draw and battery brownouts.
3. Updated the AST knowledge graph via `graphify update .`.

### Code Diff

```diff
diff --git a/firmware/app/src/alert_manager.c b/firmware/app/src/alert_manager.c
index e547a5e..1c3bdb6 100644
--- a/firmware/app/src/alert_manager.c
+++ b/firmware/app/src/alert_manager.c
@@ -93,6 +93,9 @@ static alert_led_pattern_t alert_mgr_resolve_led_pattern(const alert_input_t *p_
     if (p_in->sensor_fault) {
         return ALERT_LED_PATTERN_SYSTEM_FAULT;
     }
+    if (p_in->battery_tier == BATTERY_TIER_CONSERVATION) {
+        return ALERT_LED_PATTERN_CONSERVATION;
+    }
     if (p_in->rain_state == RAIN_ALERT_IMMINENT || p_in->cpi_pct >= 80.0f) {
         return ALERT_LED_PATTERN_IMMINENT_STROBE;
     }
@@ -105,9 +108,6 @@ static alert_led_pattern_t alert_mgr_resolve_led_pattern(const alert_input_t *p_
     if (p_in->rain_state == RAIN_ALERT_POSSIBLE || p_in->cpi_pct >= 30.0f) {
         return ALERT_LED_PATTERN_WATCH_AMBER;
     }
-    if (p_in->battery_tier == BATTERY_TIER_CONSERVATION) {
-        return ALERT_LED_PATTERN_CONSERVATION;
-    }
     return ALERT_LED_PATTERN_HEALTHY_PULSE;
 }
```

---

## 4. Verification & Prevention Guidelines

### Verification Checklist

- [x] Repositioned `BATTERY_TIER_CONSERVATION` condition in `alert_mgr_resolve_led_pattern()`.
- [x] Verified `test_alert_battery_conservation_tier2()` assertion invariants: active pattern resolves to `ALERT_LED_PATTERN_CONSERVATION` (7).
- [x] Knowledge graph refreshed via `graphify update .`.
- [x] Confirmed zero dynamic memory allocation and strict adherence to project coding standards.

### Prevention Guidelines

1. **Power Overrides Precedence Principle**:
   - In battery-operated remote embedded telemetry devices, hardware power preservation interlocks (deep sleep, critical cutoff, low-battery throttling) must strictly supersede application-level sensor alarms and operational actuation.
2. **Exhaustive Priority Matrix Testing**:
   - For all decision and arbitration functions, unit test matrices must explicitly test combinations of conflicting inputs (e.g. highest emergency condition concurrent with lowest battery condition) to verify deterministic priority resolution.
