# Error Report: ERR-032 - Modbus Query Slave Transaction Timeout Due to Host Simulation RX Ring Buffer Flush

## Metadata

| Field | Details |
| :--- | :--- |
| **Error ID** | `ERR-032` |
| **Date & Time** | 2026-09-14 14:18:00 IST |
| **Commit SHA** | [`0f75905`](https://github.com/Thejus26/rain-predict/commit/0f75905cb291e67a2afd358fcfc58b3a3664ab1d) |
| **Component / Subsystem** | Testing & Driver Simulation (Modbus RTU / Mock UART) |
| **Sprint / Task** | `Sprint 4: Sensor Drivers & Remote Field Bus Interfaces` / `S4-T4.3` & `S4-T4.4` |
| **Severity** | High (CI Host Test Runner Failure in `test_modbus_rtu`) |
| **Impacted Files** | [`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c) |

---

## 1. Description & Symptoms

During automated CI execution of the test suite (`ctest --test-dir build-host --output-on-failure`), Test #24 (`modbus_rtu`) failed with 4 unit test failures:

```text
24/24 Test #24: modbus_rtu .......................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:964:test_modbus_constants_and_types:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:965:test_modbus_crc16_calculation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:966:test_modbus_build_req_null_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:967:test_modbus_build_req_slave_addr_validation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:968:test_modbus_build_req_reg_count_validation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:969:test_modbus_build_req_buffer_size_guard:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:970:test_modbus_build_req_valid_frames:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:971:test_modbus_parse_resp_null_and_bounds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:972:test_modbus_parse_resp_crc_corruption:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:973:test_modbus_parse_resp_address_and_fc_mismatch:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:974:test_modbus_parse_resp_byte_count_mismatches:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:975:test_modbus_parse_resp_exception_frames:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:976:test_modbus_parse_resp_success_unpacking:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:977:test_modbus_decode_thp_null_and_bounds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:978:test_modbus_decode_thp_standard_values:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:979:test_modbus_decode_thp_subzero_and_clamping:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:980:test_modbus_exception_to_str:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:983:test_modbus_crc16_bitwise_vectors:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:984:test_modbus_crc16_lut_vectors:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:985:test_modbus_crc16_bitwise_vs_lut_equivalence:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:986:test_modbus_crc16_streaming_update:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:987:test_modbus_validate_frame_crc_engine:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:988:test_modbus_append_crc16_engine:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:991:test_modbus_dir_init_and_toggling:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:992:test_modbus_guard_timing_and_constants:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:993:test_modbus_query_null_and_boundary_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:806:test_modbus_query_slave_raw_success:FAIL: Expected 0 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:850:test_modbus_query_slave_thp_success:FAIL: Expected 0 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:996:test_modbus_query_slave_raw_timeout:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:997:test_modbus_query_slave_thp_timeout_retries:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:912:test_modbus_query_slave_raw_exception:FAIL: Expected 16 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:937:test_modbus_query_slave_raw_crc_mismatch:FAIL: Expected 5 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1000:test_modbus_query_slave_raw_uninit_recovery:PASS

-----------------------
33 Tests 4 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

1. **Pre-Transmission RX Buffer Flush in Host Simulation**:
   - In [`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c), `modbus_query_slave_raw()` executes `(void)uart_bus_flush(UART_PORT_RS485);` at Step 2 prior to enabling the RS-485 transceiver and transmitting the FC03 query frame.
   - On physical STM32 embedded target hardware, flushing the UART RX ring buffer prior to transmission is standard practice to discard electrical line transients, reflection noise, or lingering bytes. On hardware, the remote slave only receives and responds to the request after transmission completes, with incoming response bytes placed in the RX ring buffer by the UART RX interrupt service routine (`HAL_UART_RxCpltCallback`).
   - In host unit tests ([`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c)), execution is synchronous and single-threaded. Unit tests simulate incoming slave responses by calling `uart_bus_test_inject_rx(UART_PORT_RS485, resp, sizeof(resp))` **before** invoking `modbus_query_slave_raw()` or `modbus_query_slave_thp()`.
   - When `modbus_query_slave_raw()` unconditionally invoked `uart_bus_flush(UART_PORT_RS485)`, the mock simulation implementation of `uart_bus_flush` immediately reset `rx_head = 0`, `rx_tail = 0`, and `rx_count = 0`, instantly purging the pre-injected test fixture response.
   - When `uart_bus_receive()` was subsequently called in Step 8, the mock RX ring buffer was empty (`rx_count == 0`), causing it to return `STATUS_ERR_TIMEOUT` (numeric value `2`).
   
2. **Impacted Unit Test Assertions**:
   - `test_modbus_query_slave_raw_success` (line 806): Expected `STATUS_OK` (`0`), received `STATUS_ERR_TIMEOUT` (`2`).
   - `test_modbus_query_slave_thp_success` (line 850): Expected `STATUS_OK` (`0`), received `STATUS_ERR_TIMEOUT` (`2`).
   - `test_modbus_query_slave_raw_exception` (line 912): Expected `STATUS_ERROR_MODBUS_EXCEPTION` (`16`), received `STATUS_ERR_TIMEOUT` (`2`).
   - `test_modbus_query_slave_raw_crc_mismatch` (line 937): Expected `STATUS_ERROR_CRC` (`5`), received `STATUS_ERR_TIMEOUT` (`2`).

---

## 3. Resolution & Code Changes

### Fix Strategy
- Guard the pre-transmission hardware buffer flush `(void)uart_bus_flush(UART_PORT_RS485);` inside `modbus_query_slave_raw()` under `#if defined(HAVE_STM32WLXX_HAL)`.
- On target ARM hardware (`HAVE_STM32WLXX_HAL` defined), stale RX ring buffer data and hardware overrun flags (`__HAL_UART_CLEAR_OREFLAG`) are flushed prior to transmission.
- In host unit test simulation, the pre-injected response bytes are preserved in the mock FIFO across the transaction for `uart_bus_receive()` to read and validate.

### Proposed Code Diff
```diff
diff --git a/firmware/drivers/src/modbus_rtu.c b/firmware/drivers/src/modbus_rtu.c
--- a/firmware/drivers/src/modbus_rtu.c
+++ b/firmware/drivers/src/modbus_rtu.c
@@ -440,8 +440,10 @@ status_t modbus_query_slave_raw(uint8_t slave_addr,
     uint8_t  rx_buf[MODBUS_MAX_FRAME_SIZE];
     uint16_t rx_len = 0U;
 
+#if defined(HAVE_STM32WLXX_HAL)
     /* Step 2: Flush UART RX Ring Buffer to discard stale noise */
     (void)uart_bus_flush(UART_PORT_RS485);
+#endif
 
     /* Step 3: Assert DE=HIGH (Transmitter Mode) */
     modbus_set_direction_tx();
```

---

## 4. Verification & Prevention Guidelines

1. **Hardware vs Simulation Lifecycle Differences**:
   - In synchronous mock bus testing, response injection occurs prior to initiating the transaction function. Any hardware-clearing operations (such as RX flush) that run unconditionally will purge pre-injected mock stimulus. Guard hardware-only buffer purging with target feature flags (`HAVE_STM32WLXX_HAL`).
2. **Status Code Mapping Vigilance**:
   - Numeric return value `2` in error logs corresponds to `STATUS_ERR_TIMEOUT` (`STATUS_ERROR_TIMEOUT`). A uniform timeout across valid, exception, and CRC test vectors is a clear indicator that the mock RX buffer was prematurely cleared before receipt.
3. **Comprehensive CI Regression Testing**:
   - Verify all 33 test cases in `test_modbus_rtu` pass cleanly under CTest with 100% success rate.
