# Error Report: ERR-029 - Modbus Response Byte Count Test Truncated Buffer CRC Status Discrepancy

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-029` |
| **Date & Time** | 2026-09-14 13:43:00 +05:30 |
| **Commit SHA** | [`246541c`](https://github.com/Thejus26/rain-predict/commit/246541c8c8fa73f26da077a5cce6e1073542e563) |
| **Component / Subsystem** | Testing & Driver Validation (Modbus RTU) |
| **Sprint & Task** | Sprint 4 (`S4-T4.1` & `S4-T4.4`) |
| **Severity** | Medium (CI Host Test Runner Failure in `test_modbus_rtu`) |
| **Impacted Files** | [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c) |

---

## 1. Description & Symptoms

During automated CI execution of the test suite (`Run ctest --test-dir build-host --output-on-failure`), Test #24 (`modbus_rtu`) failed in `test_modbus_parse_resp_byte_count_mismatches`:

```text
      Start 24: modbus_rtu
24/24 Test #24: modbus_rtu .......................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:491:test_modbus_constants_and_types:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:492:test_modbus_crc16_calculation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:493:test_modbus_build_req_null_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:494:test_modbus_build_req_slave_addr_validation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:495:test_modbus_build_req_reg_count_validation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:496:test_modbus_build_req_buffer_size_guard:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:497:test_modbus_build_req_valid_frames:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:498:test_modbus_parse_resp_null_and_bounds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:499:test_modbus_parse_resp_crc_corruption:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:500:test_modbus_parse_resp_address_and_fc_mismatch:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:306:test_modbus_parse_resp_byte_count_mismatches:FAIL: Expected 15 Was 5
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:502:test_modbus_parse_resp_exception_frames:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:503:test_modbus_parse_resp_success_unpacking:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:504:test_modbus_decode_thp_null_and_bounds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:505:test_modbus_decode_thp_standard_values:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:506:test_modbus_decode_thp_subzero_and_clamping:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_modbus_rtu.c:507:test_modbus_exception_to_str:PASS

-----------------------
17 Tests 1 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c), test case `test_modbus_parse_resp_byte_count_mismatches` (Case B) tests how the response parser handles an unexpected total frame length:

```c
    /* Case B: Byte count is 6, but total buffer length is only 10 (truncated frame) */
    memcpy(resp, "\x01\x03\x06\x09\x94\x22\x92\x27\x94", 9);
    crc = modbus_crc16(resp, 9U);
    resp[9] = (uint8_t)(crc & 0xFFU);
    resp[10] = (uint8_t)((crc >> 8) & 0xFFU);

    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, 10U, reg_data, &ex));
```

In [`firmware/drivers/src/modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/modbus_rtu.c#L115-L162), `modbus_parse_read_holding_registers_resp()` performs validations in strict pipeline sequence:
1. **Minimum Frame Size Check** (`resp_len >= 5`): Passes since $10 \ge 5$.
2. **CRC-16 Checksum Verification**: Calculates CRC over `resp_len - 2` bytes (i.e. $10 - 2 = 8$ bytes) and compares against `resp[8]` and `resp[9]`.
   - In the test setup, `crc` was computed over 9 bytes and placed at `resp[9]` and `resp[10]`.
   - Byte 8 contains the payload byte `0x94`, and byte 9 contains `(crc & 0xFF)`.
   - Therefore, the 8-byte CRC calculation mismatches `(resp[8] | (resp[9] << 8))`.
   - The parser immediately returns `STATUS_ERROR_CRC` (`STATUS_ERR_CRC_MISMATCH` = enum value `5`).
3. **Total Frame Length Check** (`resp_len == 3 + byte_count + 2`): Returns `STATUS_ERROR_INVALID_FRAME` (`STATUS_ERR_DATA_CORRUPT` = enum value `15`).
   - Because the CRC check failed first, execution never reached the frame length validation check.
   - The Unity assertion expecting `STATUS_ERROR_INVALID_FRAME` (15) failed because it received `STATUS_ERROR_CRC` (5).

---

## 3. Resolution & Code Changes

### Resolution Steps
1. Updated `test_modbus_parse_resp_byte_count_mismatches()` in [`tests/unit/test_modbus_rtu.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_modbus_rtu.c):
   - **Case B**: Replaced the 11-byte buffer truncated call with a dedicated 10-byte buffer (`resp_short[10]`) having its CRC computed over 8 bytes and stored at indices 8 and 9. This ensures CRC verification succeeds, isolating the subsequent total frame length mismatch check ($10 \ne 3 + 6 + 2$) which returns `STATUS_ERROR_INVALID_FRAME`.
   - **Case C**: Added an over-length frame test with a 12-byte buffer (`resp_long[12]`) having its CRC computed over 10 bytes and stored at indices 10 and 11, confirming that oversized frames also trigger `STATUS_ERROR_INVALID_FRAME`.

### Code Diff
```diff
diff --git a/tests/unit/test_modbus_rtu.c b/tests/unit/test_modbus_rtu.c
--- a/tests/unit/test_modbus_rtu.c
+++ b/tests/unit/test_modbus_rtu.c
@@ -297,14 +297,23 @@ static void test_modbus_parse_resp_byte_count_mismatches(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
                           modbus_parse_read_holding_registers_resp(1U, 3U, resp_wrong_bc, sizeof(resp_wrong_bc), reg_data, &ex));
 
-    /* Case B: Byte count is 6, but total buffer length is only 10 (truncated frame) */
-    memcpy(resp, "\x01\x03\x06\x09\x94\x22\x92\x27\x94", 9);
-    crc = modbus_crc16(resp, 9U);
-    resp[9] = (uint8_t)(crc & 0xFFU);
-    resp[10] = (uint8_t)((crc >> 8) & 0xFFU);
-
-    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
-                          modbus_parse_read_holding_registers_resp(1U, 3U, resp, 10U, reg_data, &ex));
+    /* Case B: Byte count is 6 (expects 11 bytes total), but frame is 10 bytes with valid 8-byte CRC */
+    uint8_t resp_short[10] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x00U, 0x00U};
+    crc = modbus_crc16(resp_short, 8U);
+    resp_short[8] = (uint8_t)(crc & 0xFFU);
+    resp_short[9] = (uint8_t)((crc >> 8) & 0xFFU);
+
+    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
+                          modbus_parse_read_holding_registers_resp(1U, 3U, resp_short, sizeof(resp_short), reg_data, &ex));
+
+    /* Case C: Byte count is 6 (expects 11 bytes total), but frame is 12 bytes with valid 10-byte CRC */
+    uint8_t resp_long[12] = {0x01U, 0x03U, 0x06U, 0x09U, 0x94U, 0x22U, 0x92U, 0x27U, 0x94U, 0xAAU, 0x00U, 0x00U};
+    crc = modbus_crc16(resp_long, 10U);
+    resp_long[10] = (uint8_t)(crc & 0xFFU);
+    resp_long[11] = (uint8_t)((crc >> 8) & 0xFFU);
+
+    TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_FRAME,
+                          modbus_parse_read_holding_registers_resp(1U, 3U, resp_long, sizeof(resp_long), reg_data, &ex));
 }
```

---

## 4. Verification & Prevention Guidelines

1. **Layered Verification Isolation**:
   - When crafting unit tests for high-level packet structure and header mismatch assertions, ensure that all upstream protocol checks (such as CRCs and minimum length checks) are valid for the test payload so that the targeted validation branch is explicitly reached and tested.
2. **Deterministic Status Code Mapping**:
   - Ensure test assertions match the exact failure mode triggered by the packet's layout.
3. **Multi-Sided Boundary Coverage**:
   - Test both under-length (`resp_short[10]`) and over-length (`resp_long[12]`) frame conditions against expected payload sizes.
