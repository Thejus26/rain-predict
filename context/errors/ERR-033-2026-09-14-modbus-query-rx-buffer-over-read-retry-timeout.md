# Error Report: ERR-033 - Modbus Query Slave RX Buffer Over-Read Causing Automatic Retry Recovery Timeout

## Metadata

| Field | Details |
| :--- | :--- |
| **Error ID** | `ERR-033` |
| **Date & Time** | 2026-09-14 14:34:00 IST |
| **Commit SHA** | [`ddd88ac`](https://github.com/Thejus26/rain-predict/commit/ddd88aca134cb4c92a51bbc517228807c0e828ed) |
| **Component / Subsystem** | Testing & Driver Communication (Modbus RTU / UART Bus) |
| **Sprint / Task** | `Sprint 4: Sensor Drivers & Remote Field Bus Interfaces` / `S4-T4.4: Modbus RTU Unit Tests` |
| **Severity** | High (CI Host Test Suite Failure in `test_modbus_rtu`) |
| **Impacted Files** | [`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c) |

---

## 1. Description & Symptoms

During automated CI execution of the test suite (`ctest --test-dir build-host --output-on-failure`), Test #24 (`modbus_rtu`) failed with an assertion failure in `test_modbus_query_slave_thp_retry_recovery`:

```text
24/24 Test #24: modbus_rtu .......................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1012:test_modbus_constants_and_types:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1013:test_modbus_crc16_calculation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1014:test_modbus_build_req_null_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1015:test_modbus_build_req_slave_addr_validation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1016:test_modbus_build_req_reg_count_validation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1017:test_modbus_build_req_buffer_size_guard:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1018:test_modbus_build_req_valid_frames:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1019:test_modbus_parse_resp_null_and_bounds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1020:test_modbus_parse_resp_crc_corruption:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1021:test_modbus_parse_resp_address_and_fc_mismatch:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1022:test_modbus_parse_resp_byte_count_mismatches:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1023:test_modbus_parse_resp_exception_frames:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1024:test_modbus_parse_resp_success_unpacking:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1025:test_modbus_decode_thp_null_and_bounds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1026:test_modbus_decode_thp_standard_values:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1027:test_modbus_decode_thp_subzero_and_clamping:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1028:test_modbus_exception_to_str:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1031:test_modbus_crc16_bitwise_vectors:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1032:test_modbus_crc16_lut_vectors:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1033:test_modbus_crc16_bitwise_vs_lut_equivalence:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1034:test_modbus_crc16_streaming_update:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1035:test_modbus_validate_frame_crc_engine:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1036:test_modbus_append_crc16_engine:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1039:test_modbus_dir_init_and_toggling:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1040:test_modbus_guard_timing_and_constants:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1041:test_modbus_query_null_and_boundary_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1042:test_modbus_query_slave_raw_success:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1043:test_modbus_query_slave_thp_success:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:978:test_modbus_query_slave_thp_retry_recovery:FAIL: Expected 0 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1045:test_modbus_query_slave_raw_timeout:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1046:test_modbus_query_slave_thp_timeout_retries:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1047:test_modbus_query_slave_raw_exception:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1048:test_modbus_query_slave_raw_crc_mismatch:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:1049:test_modbus_query_slave_raw_uninit_recovery:PASS

-----------------------
34 Tests 1 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

1. **Max Buffer Size Request vs Expected Frame Length**:
   - In [`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c), `modbus_query_slave_raw()` calls `uart_bus_receive()` passing `sizeof(rx_buf)` (`MODBUS_MAX_FRAME_SIZE` = 256 bytes) as the requested length:
     ```c
     status = uart_bus_receive(UART_PORT_RS485,
                               rx_buf,
                               sizeof(rx_buf),
                               &rx_len,
                               timeout_ms);
     ```
   - In `uart_bus_receive()`, when the requested length is 256 bytes, the function enters a polling loop `while (p_state->rx_count < length)` and blocks until `timeout_ms` expires.
   - Upon timeout, `uart_bus_receive()` pops **all** available bytes currently present in the UART RX ring buffer (`copy_count = p_state->rx_count`).

2. **Ring Buffer Over-Drain in Multi-Frame Test Scenarios**:
   - In [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c), the test case `test_modbus_query_slave_thp_retry_recovery` pre-injects two separate 15-byte responses into the RX FIFO:
     1. `bad_resp` (15 bytes, corrupted CRC) for Attempt 1.
     2. `good_resp` (15 bytes, valid data) for Attempt 2 retry recovery.
   - When Attempt 1 runs, `uart_bus_receive()` attempts to read up to 256 bytes, waits for `timeout_ms`, and drains **all 30 bytes** (`bad_resp` + `good_resp`) into `rx_buf`.
   - `modbus_parse_read_holding_registers_resp()` fails because the buffer length (30 bytes) does not match the expected frame structure or fails CRC verification across the concatenated frames, correctly triggering a retry.
   - However, when `modbus_query_slave_thp()` proceeds to Attempt 2, the RX ring buffer has already been completely drained (`rx_count == 0`).
   - Consequently, Attempt 2 and Attempt 3 time out, causing `modbus_query_slave_thp()` to return `STATUS_ERR_TIMEOUT` (`2`) instead of `STATUS_OK` (`0`).

3. **Deterministic Modbus FC03 Response Sizing**:
   - For a Modbus Function Code 0x03 Read Holding Registers request of $N$ registers (`reg_count`), the expected normal response frame length is precisely:
     $$\text{Length} = 1 \text{ (Slave Addr)} + 1 \text{ (FC)} + 1 \text{ (Byte Count)} + (2 \times N) \text{ (Data Bytes)} + 2 \text{ (CRC-16)} = 5 + 2N \text{ bytes}$$
   - When `uart_bus_receive()` is requested to receive `expected_rx_len = 5U + (2U * reg_count)` bytes:
     - As soon as the expected $5 + 2N$ bytes arrive, `uart_bus_receive()` exits immediately without waiting for `timeout_ms`.
     - In retry test fixtures, exactly 15 bytes (`bad_resp`) are consumed in Attempt 1, leaving the remaining 15 bytes (`good_resp`) intact in the FIFO for Attempt 2.
     - For exception frames (5 bytes), `uart_bus_receive()` waits for `timeout_ms` and returns the 5 received bytes, allowing `modbus_parse_read_holding_registers_resp()` to decode the exception code properly.

---

## 3. Resolution & Code Changes

### Fix Strategy & Applied Resolution
1. **Bounded Frame Length Calculation**:
   - Calculated the precise expected frame size for Modbus FC03 responses based on requested register count:
     `uint16_t expected_rx_len = 3U + (reg_count * 2U) + 2U;`
   - Added destination buffer capacity guard: `if (expected_rx_len > (uint16_t)sizeof(rx_buf)) { expected_rx_len = (uint16_t)sizeof(rx_buf); }`.
2. **Deterministic Stream Ingestion**:
   - Passed `expected_rx_len` to `uart_bus_receive()` instead of `sizeof(rx_buf)`.
   - In single-frame transactions, `uart_bus_receive()` returns immediately upon receiving the expected frame bytes without waiting for timeout.
   - In multi-attempt retry scenarios, each attempt consumes exactly one frame from the mock RX ring buffer, preserving subsequent retry frames for subsequent attempts.
   - Exception responses (5 bytes) remain handled via bounded timeout fallback.

### Applied Code Diff
```diff
diff --git a/firmware/drivers/src/modbus_rtu.c b/firmware/drivers/src/modbus_rtu.c
index 5b77a41..8871548 100644
--- a/firmware/drivers/src/modbus_rtu.c
+++ b/firmware/drivers/src/modbus_rtu.c
@@ -464,9 +464,14 @@ status_t modbus_query_slave_raw(uint8_t slave_addr,
     modbus_set_direction_rx();
 
     /* Step 8: Await Slave Response with Bounded Software Timeout */
+    /* Normal FC03 response: Slave(1) + FC(1) + ByteCount(1) + 2*N data + CRC(2) */
+    uint16_t expected_rx_len = 3U + (reg_count * 2U) + 2U;
+    if (expected_rx_len > (uint16_t)sizeof(rx_buf)) {
+        expected_rx_len = (uint16_t)sizeof(rx_buf);
+    }
     status = uart_bus_receive(UART_PORT_RS485,
                               rx_buf,
-                              sizeof(rx_buf),
+                              expected_rx_len,
                               &rx_len,
                               timeout_ms);
     if (status != STATUS_OK) {
```

---

## 4. Verification & Prevention Guidelines

1. **Exact Frame Bounding for Protocol Decoders**:
   - In serial communication protocols with deterministic frame lengths (Modbus RTU, SDI-12), always request the specific expected frame byte count from stream receivers rather than maximum buffer capacity.
   - This prevents unneeded timeout latency and avoids consuming subsequent back-to-back frames queued in the ring buffer.
2. **Multi-Attempt Mock Stimulus Isolation**:
   - When unit testing retry logic with pre-injected test vectors, verify that each execution attempt cleanly pops only its allocated frame bytes from the mock transport layer.
3. **Automated Verification**:
   - Re-run CTest on the complete test suite to ensure all 34 test cases in `test_modbus_rtu` pass cleanly.
