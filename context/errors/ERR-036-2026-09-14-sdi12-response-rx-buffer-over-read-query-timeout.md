# Error Report: ERR-036 - SDI-12 Response Acquisition RX Buffer Over-Read Causing Soil Probe Multi-Stage Query Timeout

## Metadata

| Field | Details |
| :--- | :--- |
| **Error ID** | `ERR-036` |
| **Date & Time** | 2026-09-14 15:21:08 +05:30 |
| **Commit SHA** | [`4ad93b1`](https://github.com/Thejus26/rain-predict/commit/4ad93b14ae571f69bdf991bae211617e1d38623c) |
| **Component / Subsystem** | Testing & Driver Communication (SDI-12 / UART Bus) |
| **Sprint / Task** | `Sprint 4: Sensor Drivers & Remote Field Bus Interfaces` / `S4-T5.3: SDI-12 Unit Test Suite` |
| **Severity** | High (CI Host Test Suite Failure in `test_sdi12`) |
| **Impacted Files** | [`firmware/drivers/src/sdi12_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/sdi12_driver.c) |

---

## 1. Description & Symptoms

During automated CI execution of the test suite (`ctest --test-dir build-host --output-on-failure`), Test #25 (`sdi12`) failed with assertion failures in `test_sdi12_query_soil_probe_orchestration` and `test_sdi12_query_soil_probe_clamping_and_errors`:

```text
25/25 Test #25: sdi12 ............................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:567:test_sdi12_init_defaults:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:568:test_sdi12_direction_control:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:569:test_sdi12_send_break_and_mark_execution:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:570:test_sdi12_transmit_command_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:571:test_sdi12_transmit_command_missing_exclamation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:572:test_sdi12_transmit_command_empty_and_overflow:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:573:test_sdi12_wake_and_transmit_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:574:test_sdi12_wake_and_transmit_null_and_invalid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:575:test_sdi12_receive_response_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:576:test_sdi12_receive_response_missing_crlf:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:577:test_sdi12_receive_response_timeout:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:578:test_sdi12_receive_response_null_and_overflow:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:579:test_sdi12_deinit:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:582:test_sdi12_format_command_standard:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:583:test_sdi12_format_command_data_and_extended:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:584:test_sdi12_format_command_defensive:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:585:test_sdi12_parse_measurement_info_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:586:test_sdi12_parse_measurement_info_address_mismatch:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:587:test_sdi12_parse_measurement_info_defensive:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:588:test_sdi12_parse_data_response_three_values:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:589:test_sdi12_parse_data_response_negative_values:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:590:test_sdi12_parse_data_response_scientific_notation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:591:test_sdi12_parse_data_response_defensive:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:592:test_sdi12_parse_identification_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:593:test_sdi12_parse_identification_no_serial_and_wildcard:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:594:test_sdi12_parse_identification_defensive:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:483:test_sdi12_query_soil_probe_orchestration:FAIL: Expected 0 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:500:test_sdi12_query_soil_probe_clamping_and_errors:FAIL: Expected 0 Was 2
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:597:test_sdi12_query_address_discovery:PASS

-----------------------
29 Tests 2 Failures 0 Ignored
FAIL

Errors while running CTest

96% tests passed, 1 tests failed out of 25

Total Test time (real) =   0.15 sec

The following tests FAILED:
	 25 - sdi12 (Failed)
Error: Process completed with exit code 8.
```

---

## 2. Root Cause Analysis

1. **Unbounded Buffer Drain in `sdi12_receive_response()`**:
   - In [`firmware/drivers/src/sdi12_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/sdi12_driver.c), `sdi12_receive_response()` calls `uart_bus_receive()` requesting `max_len - 1U` bytes (e.g. 63 bytes):
     ```c
     status_t status = uart_bus_receive(UART_PORT_SDI12,
                                        (uint8_t *)p_out_buf,
                                        max_len - 1U,
                                        &bytes_received,
                                        timeout_ms);
     ```
   - In the mock transport layer, `uart_bus_receive()` drains all currently available bytes in the RX ring buffer up to the requested capacity.

2. **Ring Buffer Over-Drain in Multi-Stage Soil Probe Queries**:
   - In [`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c), `test_sdi12_query_soil_probe_orchestration` and `test_sdi12_query_soil_probe_clamping_and_errors` pre-inject the concatenated Stage 1 (`aM!` acknowledgment `00003\r\n`) and Stage 2 (`aD0!` data response `0+0.354+22.10+1.24\r\n`) into the UART RX FIFO:
     ```c
     const char *combined_resp = "00003\r\n0+0.354+22.10+1.24\r\n";
     (void)mock_uart_inject_rx(UART_PORT_SDI12, (const uint8_t *)combined_resp, (uint16_t)strlen(combined_resp));
     ```
   - During Step 2 of `sdi12_query_soil_probe()`, the first call to `sdi12_receive_response()` requests up to 63 bytes, draining **all 30 bytes** of both responses into the first response buffer.
   - When the function proceeds to Step 5 (issuing `aD0!` and expecting the data response), the UART RX ring buffer is already empty (`rx_count == 0`).
   - Consequently, Step 5 times out and returns `STATUS_ERR_TIMEOUT` (`2`), causing `test_sdi12_query_soil_probe_orchestration` and `test_sdi12_query_soil_probe_clamping_and_errors` to fail with `Expected 0 Was 2`.

3. **SDI-12 Standard Frame Termination**:
   - According to the SDI-12 specification (v1.4), all valid SDI-12 sensor responses are single-line ASCII strings terminated by `<CR><LF>` (`\r\n`).
   - The response reader should stream bytes one by one (or inspect the incoming stream) and terminate reception immediately upon encountering the `\r\n` line delimiter, preserving any subsequent frames in the serial buffer.

---

## 3. Resolution & Code Changes

### Applied Resolution
1. **Frame-Bounded Byte-by-Byte Stream Ingestion**:
   - Refactored `sdi12_receive_response()` in [`firmware/drivers/src/sdi12_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/sdi12_driver.c) to read incoming data byte-by-byte via `uart_bus_receive()`.
   - Bounded reception to terminate immediately once the trailing `\r\n` sequence is encountered, preventing multi-frame drain across serial buffers.
   - Handled error/timeout conditions gracefully: if timeout occurs when 0 bytes have been received, returns `status` (`STATUS_ERROR_TIMEOUT`); if partial bytes were collected prior to timeout without encountering `\r\n`, safely terminates the buffer and returns `STATUS_ERROR_INVALID_FRAME`.

### Applied Code Diff
```diff
diff --git a/firmware/drivers/src/sdi12_driver.c b/firmware/drivers/src/sdi12_driver.c
index b963cbb..38119a8 100644
--- a/firmware/drivers/src/sdi12_driver.c
+++ b/firmware/drivers/src/sdi12_driver.c
@@ -201,13 +201,35 @@ status_t sdi12_receive_response(char *p_out_buf,
     *p_rx_len = 0U;
 
     uint16_t bytes_received = 0U;
-    status_t status = uart_bus_receive(UART_PORT_SDI12,
-                                       (uint8_t *)p_out_buf,
-                                       max_len - 1U,
-                                       &bytes_received,
-                                       timeout_ms);
-    if (status != STATUS_OK) {
-        return status;
+
+    while (bytes_received < max_len - 1U) {
+        uint8_t byte = 0U;
+        uint16_t byte_len = 0U;
+        status_t status = uart_bus_receive(UART_PORT_SDI12,
+                                           &byte,
+                                           1U,
+                                           &byte_len,
+                                           timeout_ms);
+        if (status != STATUS_OK) {
+            if (bytes_received > 0U) {
+                p_out_buf[bytes_received] = '\0';
+                *p_rx_len = bytes_received;
+                return STATUS_ERROR_INVALID_FRAME;
+            }
+            return status;
+        }
+
+        if (byte_len == 0U) {
+            break;
+        }
+
+        p_out_buf[bytes_received++] = (char)byte;
+
+        if (bytes_received >= 2U &&
+            p_out_buf[bytes_received - 2U] == '\r' &&
+            p_out_buf[bytes_received - 1U] == '\n') {
+            break;
+        }
     }
 
     /* Null-terminate response string */
```

---

## 4. Verification & Prevention Guidelines

1. **Delimiter-Bounded Streaming for Line Protocols**:
   - For ASCII-based serial protocols (SDI-12, NMEA 0183, AT commands), stream parsers must strictly bound consumption to frame delimiters (`\r\n`) rather than attempting bulk buffer reads of arbitrary capacity.
2. **Multi-Stage Orchestration Isolation**:
   - Ensure that multi-stage protocol sequences (e.g. Start Measurement `aM!` -> Data Request `aD0!`) test harness fixtures do not inadvertently corrupt inter-stage buffer state.
3. **Automated Verification**:
   - Re-run CTest across all 25 unit test suites to confirm 100% pass rate.
