# Error Report: ERR-007 - Mock I2C Device Search Unintended State Mutation

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-007` |
| **Date & Time** | `2026-09-11 14:33:02 +0530` |
| **Commit SHA** | [`0a33eec`](https://github.com/Thejus26/rain-predict/commit/0a33eec3e8d20e597f9edaea71168d97d04b0a31) |
| **Sprint / Task** | Sprint 1 (S1-T2.2 Mock I2C Bus Driver) |
| **Severity** | High (Logical Bug / Test Invalidation) |
| **Impacted Files** | [`tests/mocks/mock_i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/mocks/mock_i2c_bus.c) |

---

## 1. Description & Symptoms

Unit tests checking whether an unconfigured I2C device address responded (e.g. `i2c_bus_is_device_ready(0x76)` or `i2c_bus_read(0x76, ...)`) inadvertently created a new virtual mock device at that address instead of returning `STATUS_ERR_SENSOR_NO_RESPONSE`. Consequently, non-existent devices appeared "ready", masking real bus disconnect faults.

---

## 2. Root Cause Analysis

In `mock_i2c_bus.c`, the helper `find_or_create_device()` was called across both mutation paths (e.g., `mock_i2c_set_register`) and read/inspection paths (`i2c_bus_read`, `i2c_bus_is_device_ready`, `mock_i2c_get_read_count`). When a query was performed for an address not previously configured, `find_or_create_device()` allocated a new mock device slot and marked it as configured with all-zero registers.

---

## 3. Resolution & Code Changes

Separated pure lookup from creation logic:
1. Created `find_device(dev_addr)` which only searches existing configured devices and returns `NULL` if not found.
2. Restricted `find_or_create_device(dev_addr)` strictly to configuration write paths (`mock_i2c_set_register`, etc.).
3. Updated read and status inspection functions to use `find_device()`:

```diff
-static mock_device_t *find_or_create_device(uint8_t dev_addr) {
+static mock_device_t *find_device(uint8_t dev_addr) {
     for (size_t i = 0; i < MOCK_I2C_MAX_DEVICES; i++) {
         if (s_devices[i].is_configured && s_devices[i].address == dev_addr) {
             return &s_devices[i];
         }
     }
+    return NULL;
+}
+
+static mock_device_t *find_or_create_device(uint8_t dev_addr) {
+    mock_device_t *p_dev = find_device(dev_addr);
+    if (p_dev != NULL) {
+        return p_dev;
+    }
     for (size_t i = 0; i < MOCK_I2C_MAX_DEVICES; i++) {
         if (!s_devices[i].is_configured) {
             s_devices[i].address = dev_addr;
```

---

## 4. Verification & Prevention Guidelines

- Ensure query/read functions are strictly read-only and have zero side effects on internal state or mock registries.
- Test negative cases explicitly: verify that uninitialized mock addresses return error codes and do not alter allocation tables.
