# Error Report: ERR-060 - Metadata Page Rollover Resets Generation Counter Causing test_metadata_page_rollover_slot_64 Failure

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-060` |
| **Date & Time** | 2026-09-24 18:44:05 +05:30 |
| **Commit SHA** | [`7eb9a50`](https://github.com/Thejus26/rain-predict/commit/7eb9a502fb80f941c86da5a1d13dfb812fcd9a22) |
| **Component / Subsystem** | Testing & Storage (Flash NVM / Metadata Journal) |
| **Sprint & Task** | Sprint 8 (`S8-T1.1` Flash Storage Ring Buffer Persistent Metadata Header) |
| **Severity** | High (CI Unit Test Assertion Failure: `test_metadata_page_rollover_slot_64`) |
| **Impacted Files** | [`firmware/middleware/src/flash_storage.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/flash_storage.c)<br>[`firmware/middleware/inc/flash_storage.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/flash_storage.h)<br>[`tests/unit/test_flash_storage.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_flash_storage.c) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI execution of host unit test suite `test_flash_storage`, test case `test_metadata_page_rollover_slot_64` failed with CTest reporting:

```text
31/45 Test #31: flash_storage ....................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
```

In [`tests/unit/test_flash_storage.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_flash_storage.c):
- `test_metadata_page_rollover_slot_64` commits 65 sequential metadata records (`i = 0` to `64`).
- Entries 0 to 63 populate all 64 slots (32 bytes each) within Flash Page 127 (`0x0803F800` - `0x0803FFFF`).
- Entry 64 (the 65th commit) triggers page erasure of Page 127 and rolls over to slot 0 with `epoch = 1` and `generation = 65`.
- Line 760 asserts:
  ```c
  TEST_ASSERT_EQUAL_UINT32(65U, latest.generation);
  ```
- The test failed because `latest.generation` was `1` (`Expected 65 Was 1`).

---

## 2. Root Cause Analysis

In [`firmware/middleware/src/flash_storage.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/flash_storage.c):

1. **Unconditional Generation Reset in `flash_storage_erase_page`**:
   [`flash_storage_erase_page()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/flash_storage.c#L69-L85) was implemented to automatically clear active metadata state whenever Page 127 is erased:
   ```c
   if (page_num == FLASH_RING_METADATA_PAGE) {
       s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
       s_metadata_generation = 0U;
       s_cached_metadata_valid = false;
   }
   ```
2. **Rollover Execution in `flash_metadata_commit`**:
   When slot index reaches `FLASH_METADATA_ENTRIES_PER_PAGE` (64), [`flash_metadata_commit()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/flash_storage.c#L983-L990) calls `flash_storage_erase_page(FLASH_RING_METADATA_PAGE)` to reclaim the sector for the next wear cycle:
   ```c
   if (target_slot >= FLASH_METADATA_ENTRIES_PER_PAGE) {
       status_t erase_st = flash_storage_erase_page(FLASH_RING_METADATA_PAGE);
       if (erase_st != STATUS_OK) {
           return erase_st;
       }
       target_slot = 0U;
       s_metadata_epoch++;
   }
   ```
3. **Generation Counter Loss**:
   Because `flash_storage_erase_page()` unconditionally set `s_metadata_generation = 0U;`, the previous generation count of 64 was discarded.
   Subsequent execution at line 994 incremented `s_metadata_generation` from 0 to 1:
   ```c
   s_metadata_generation++;
   p_meta->generation = s_metadata_generation; // Written as 1 instead of 65
   ```
   Monotonically increasing `generation` is required across the lifetime of the sensor node (across all sector erasures/epochs) to definitively distinguish the newest journal entry.

---

## 3. Resolution & Code Changes *(Implemented)*

### Fix Strategy

1. **Preserve Generation Counter across Wear Rollover in `flash_metadata_commit`**:
   Before invoking `flash_storage_erase_page(FLASH_RING_METADATA_PAGE)` when `target_slot >= FLASH_METADATA_ENTRIES_PER_PAGE`, store `s_metadata_generation` and `s_metadata_epoch` in local temporaries (`saved_gen`, `saved_epoch`). After page erasure, restore `s_metadata_generation = saved_gen`, advance `s_metadata_epoch = saved_epoch + 1U`, and reset `s_metadata_active_slot = 0U`.
2. **Synchronize Epoch Initialization on Cold Storage Init & Full Erase**:
   Explicitly reset `s_metadata_epoch = 0U` in `flash_storage_init()` and `flash_storage_erase_page()` across both host mock and embedded hardware HAL implementations to ensure clean state initialization.
3. **Comprehensive Unit Test Expansion in `tests/unit/test_flash_storage.c`**:
   Extended `test_metadata_page_rollover_slot_64` to verify:
   - Initial rollover across 65 commits (`generation == 65`, `epoch == 1`, `head_index == 64`).
   - Cold reboot recovery (`flash_storage_init()`) verifying `flash_metadata_find_latest()` correctly recovers state from Flash.
   - Subsequent journal entry commit into slot 1 in the new epoch (`generation == 66`, `epoch == 1`, `head_index == 65`).

### Code Changes

```diff
--- a/firmware/middleware/src/flash_storage.c
+++ b/firmware/middleware/src/flash_storage.c
@@ -51,6 +51,7 @@ status_t flash_storage_init(void) {
     s_mock_flash_unlocked = false;
     s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
     s_metadata_generation = 0U;
+    s_metadata_epoch = 0U;
     s_cached_metadata_valid = false;
     return STATUS_OK;
 }
@@ -78,6 +79,7 @@ status_t flash_storage_erase_page(uint32_t page_num) {
     if (page_num == FLASH_RING_METADATA_PAGE) {
         s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
         s_metadata_generation = 0U;
+        s_metadata_epoch = 0U;
         s_cached_metadata_valid = false;
     }
 
@@ -229,6 +231,7 @@ status_t flash_storage_init(void) {
     __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
     s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
     s_metadata_generation = 0U;
+    s_metadata_epoch = 0U;
     s_cached_metadata_valid = false;
     return STATUS_OK;
 }
@@ -281,6 +284,7 @@ status_t flash_storage_erase_page(uint32_t page_num) {
     if (page_num == FLASH_RING_METADATA_PAGE) {
         s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
         s_metadata_generation = 0U;
+        s_metadata_epoch = 0U;
         s_cached_metadata_valid = false;
     }
 
@@ -981,12 +985,16 @@ status_t flash_metadata_commit(flash_ring_metadata_t *p_meta) {
 
     /* Rollover if all 64 slots consumed */
     if (target_slot >= FLASH_METADATA_ENTRIES_PER_PAGE) {
+        uint32_t saved_gen = s_metadata_generation;
+        uint32_t saved_epoch = s_metadata_epoch;
         status_t erase_st = flash_storage_erase_page(FLASH_RING_METADATA_PAGE);
         if (erase_st != STATUS_OK) {
             return erase_st;
         }
         target_slot = 0U;
-        s_metadata_epoch++;
+        s_metadata_generation = saved_gen;
+        s_metadata_epoch = saved_epoch + 1U;
+        s_metadata_active_slot = 0U;
     }
 
     /* Prepare entry */
--- a/tests/unit/test_flash_storage.c
+++ b/tests/unit/test_flash_storage.c
@@ -761,6 +761,35 @@ static void test_metadata_page_rollover_slot_64(void) {
     TEST_ASSERT_EQUAL_UINT32(1U, latest.epoch);
     TEST_ASSERT_EQUAL_UINT16(64U, latest.head_index);
     TEST_ASSERT_TRUE(flash_metadata_is_consistent(&latest));
+
+    /* Verify reboot recovery from Flash after rollover */
+    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_init());
+    memset(&latest, 0, sizeof(latest));
+    active_slot = 999U;
+    st = flash_metadata_find_latest(&latest, &active_slot);
+    TEST_ASSERT_EQUAL(STATUS_OK, st);
+    TEST_ASSERT_EQUAL_UINT32(0U, active_slot);
+    TEST_ASSERT_EQUAL_UINT32(65U, latest.generation);
+    TEST_ASSERT_EQUAL_UINT32(1U, latest.epoch);
+    TEST_ASSERT_EQUAL_UINT16(64U, latest.head_index);
+
+    /* Verify subsequent commit at slot 1 in new epoch */
+    flash_ring_metadata_t next_meta;
+    memset(&next_meta, 0, sizeof(next_meta));
+    next_meta.head_index = 65U;
+    next_meta.tail_index = 0U;
+    next_meta.valid_count = 65U;
+    next_meta.next_seq_id = 65U;
+    TEST_ASSERT_EQUAL(STATUS_OK, flash_metadata_commit(&next_meta));
+    TEST_ASSERT_EQUAL_UINT32(66U, next_meta.generation);
+    TEST_ASSERT_EQUAL_UINT32(1U, next_meta.epoch);
+
+    st = flash_metadata_find_latest(&latest, &active_slot);
+    TEST_ASSERT_EQUAL(STATUS_OK, st);
+    TEST_ASSERT_EQUAL_UINT32(1U, active_slot);
+    TEST_ASSERT_EQUAL_UINT32(66U, latest.generation);
+    TEST_ASSERT_EQUAL_UINT32(1U, latest.epoch);
+    TEST_ASSERT_EQUAL_UINT16(65U, latest.head_index);
 }
```

---

## 4. Verification & Prevention Guidelines

1. **Monotonic Generation Across Wear Cycles**:
   - The generation counter must never decrease or reset to 0 across page erasures, as it provides absolute sequencing across journal wear-leveling epochs.
2. **Comprehensive Wear-Leveling Rollover Tests**:
   - Always verify multi-epoch rollover and subsequent entry appends in journal unit test suites to ensure sector reclamation does not corrupt persistent state variables.
3. **Simulated Cold Reboots in Ring Buffer Tests**:
   - Include `flash_storage_init()` reinitialization calls post-rollover to ensure all volatile state can be unambiguously reconstructed from physical Flash records.
