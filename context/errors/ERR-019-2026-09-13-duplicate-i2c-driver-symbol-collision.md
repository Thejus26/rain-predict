# Error Report: ERR-019 - Duplicate I2C Driver Symbol Collision (Mock vs Production)

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-019` |
| **Date & Time** | `2026-09-13 13:05:44 +0530` |
| **Commit SHA** | [`03aa895`](https://github.com/Thejus26/rain-predict/commit/03aa89542832ced59948914a0741462d724a8a39) |
| **Sprint / Task** | Sprint 3 (S3-T2.1 Bounded I2C Master Bus Driver) |
| **Severity** | High (Linker Symbol Collision) |
| **Impacted Files** | [`firmware/drivers/inc/i2c_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/i2c_bus.h)<br>[`firmware/drivers/src/i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/i2c_bus.c)<br>[`tests/mocks/mock_i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/mocks/mock_i2c_bus.c)<br>[`tests/CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/CMakeLists.txt) |

---

## 1. Description & Symptoms

When linking unit tests that required both `firmware_drivers` and `test_mocks` static libraries, the linker threw fatal duplicate symbol definition errors:
```
multiple definition of `i2c_bus_init'
multiple definition of `i2c_bus_read'
multiple definition of `i2c_bus_write'
multiple definition of `i2c_bus_deinit'
multiple definition of `i2c_bus_read16'
multiple definition of `i2c_bus_write16'
multiple definition of `i2c_bus_is_device_ready'
multiple definition of `i2c_bus_recover'
```

---

## 2. Root Cause Analysis

In Sprint 1, `tests/mocks/mock_i2c_bus.c` provided full implementations of the public `i2c_bus_*` functions for early test harness development. In Sprint 3 (S3-T2.1), the production driver `firmware/drivers/src/i2c_bus.c` was created, containing both STM32 HAL hardware driver code and a host-simulation backend. When test executables linked both libraries, both objects defined the identical global function symbols.

---

## 3. Resolution & Code Changes

1. Standardized `firmware/drivers/src/i2c_bus.c` as the sole implementation of all `i2c_bus_*` symbols (with `#if defined(HAVE_STM32WLXX_HAL)` for target hardware and `#else` for host simulation).
2. Exposed `i2c_bus_test_*` hooks in `firmware/drivers/inc/i2c_bus.h` for test inspection and fault injection.
3. Refactored `tests/mocks/mock_i2c_bus.c` to remove its own `i2c_bus_*` function definitions and convert all `mock_i2c_*` functions into thin wrappers around the official `i2c_bus_test_*` engine:

```c
/* mock_i2c_bus.c now routes to production host-simulation engine */
status_t mock_i2c_set_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t value) {
    return i2c_bus_test_set_slave_reg(dev_addr, reg_addr, value);
}
```

---

## 4. Verification & Prevention Guidelines

- Host simulation implementations of production driver interfaces must reside inside the driver source file itself (`firmware/drivers/src/<driver>.c`) behind `#if !defined(TARGET)` guards rather than duplicated in separate test mock files.
- Mock helper libraries should serve as test control fixtures rather than re-implementing production symbol names.
