/**
 * @file    test_flash_storage.c
 * @brief   ThrowTheSwitch Unity unit tests for STM32WLE5 Flash storage driver (S5-T3.1).
 * @details Verifies page erase, 64-bit double-word programming, arbitrary byte buffer writing,
 *          partition boundary guards, 8-byte alignment verification, and 1->0 physical bitwise AND behavior.
 */

#include "unity.h"
#include "flash_storage.h"
#include <string.h>

/* ========================================================================== */
/* Test Setup & Teardown Invariants                                           */
/* ========================================================================== */

void setUp(void) {
    /* Initialize flash storage and erase all NVM pages before each test */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_init());
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_erase_all_nvm_pages());
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_init());
}

void tearDown(void) {
    /* Verify flash returns to locked state or clean state */
}

/* ========================================================================== */
/* Helper Functions                                                           */
/* ========================================================================== */

static void generate_dummy_telemetry_payload(uint8_t *payload, uint16_t id) {
    for (uint8_t i = 0; i < FLASH_RING_TELEMETRY_PAYLOAD_SIZE; i++) {
        payload[i] = (uint8_t)((id + i) & 0xFFU);
    }
}

/* ========================================================================== */
/* Category A: Low-Level Flash HAL Driver Tests (S5-T3.1)                      */
/* ========================================================================== */

/**
 * @brief TC-S5-T3.1-05: Page Erase Operation & is_page_erased check.
 */
static void test_flash_page_erase_and_check(void) {
    /* Page 120 initially erased by setUp */
    TEST_ASSERT_TRUE(flash_storage_is_page_erased(120U));

    /* Write non-0xFF double word */
    uint64_t data = 0x123456789ABCDEF0ULL;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_dword(FLASH_STORAGE_BASE_ADDR, data));
    TEST_ASSERT_FALSE(flash_storage_is_page_erased(120U));

    /* Re-erase page 120 */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_erase_page(120U));
    TEST_ASSERT_TRUE(flash_storage_is_page_erased(120U));
}

/**
 * @brief TC-S5-T3.1-06: 64-bit Double-Word Programming and Readback.
 */
static void test_flash_write_dword_success(void) {
    uint64_t write_val = 0xAABBCCDDEEFF0011ULL;
    uint32_t target_addr = FLASH_STORAGE_BASE_ADDR + 16U;

    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_dword(target_addr, write_val));

    uint64_t read_val = 0ULL;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_read_bytes(target_addr, (uint8_t *)&read_val, sizeof(uint64_t)));
    TEST_ASSERT_EQUAL_HEX64(write_val, read_val);
}

/**
 * @brief TC-S5-T3.1-09: Bitwise AND Inversion Behavior (1 -> 0 only without erase).
 */
static void test_flash_bitwise_and_programming(void) {
    uint32_t target_addr = FLASH_STORAGE_BASE_ADDR;
    uint64_t initial_val = 0xAA55AA55AA55AA55ULL;
    uint64_t second_val  = 0x0000000000000000ULL;

    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_dword(target_addr, initial_val));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_dword(target_addr, second_val));

    uint64_t result_val = 0xFFFFFFFFFFFFFFFFULL;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_read_bytes(target_addr, (uint8_t *)&result_val, sizeof(uint64_t)));
    TEST_ASSERT_EQUAL_HEX64(0x0000000000000000ULL, result_val);
}

/**
 * @brief TC-S5-T3.1-02, TC-S5-T3.1-03, TC-S5-T3.1-04: Address and Page Bounds Guards.
 */
static void test_flash_out_of_bounds_guards(void) {
    /* Below partition (Page 119 - protected firmware application area) */
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_erase_page(119U));
    /* Above partition (Page 128) */
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_erase_page(128U));

    /* Address below partition base (0x0803BFF8 is Page 119) */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS, flash_storage_write_dword(0x0803BFF8U, 0ULL));
    /* Address above partition end (0x08040000 exceeds 256 KB Flash) */
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS, flash_storage_write_dword(0x08040000U, 0ULL));

    /* Read out of bounds */
    uint8_t buf[8];
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS, flash_storage_read_bytes(0x0803BFF8U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS, flash_storage_read_bytes(0x08040000U, buf, sizeof(buf)));

    /* Page check out of bounds */
    TEST_ASSERT_FALSE(flash_storage_is_page_erased(119U));
    TEST_ASSERT_FALSE(flash_storage_is_page_erased(128U));
}

/**
 * @brief TC-S5-T3.1-10: Unaligned Double-Word Write Rejection.
 */
static void test_flash_unaligned_write_rejection(void) {
    /* 0x0803C004 is 4-byte aligned but not 8-byte aligned */
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_write_dword(0x0803C004U, 0x1234ULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_write_dword(0x0803C002U, 0x1234ULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_write_dword(0x0803C007U, 0x1234ULL));
}

/**
 * @brief TC-S5-T3.1-07: Arbitrary Byte Buffer Writing with Read-Modify-Write Preservation.
 */
static void test_flash_write_bytes_arbitrary_length(void) {
    uint8_t src[19];
    for (uint8_t i = 0; i < 19; i++) {
        src[i] = (uint8_t)(i + 0x30);
    }

    uint32_t addr = FLASH_STORAGE_BASE_ADDR + 32U;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_bytes(addr, src, 19U));

    uint8_t dst[19];
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_read_bytes(addr, dst, 19U));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, dst, 19U);

    /* Verify untouched bytes in the double-word trailing area remain 0xFF */
    uint8_t trailing_byte = 0;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_read_bytes(addr + 19U, &trailing_byte, 1U));
    TEST_ASSERT_EQUAL_HEX8(FLASH_STORAGE_ERASED_BYTE, trailing_byte);
}

/**
 * @brief TC-S5-T3.1-08: Full NVM Partition Bulk Erase (All 8 pages).
 */
static void test_flash_erase_all_nvm_pages(void) {
    /* Dirty pages 120 and 126 */
    uint64_t dummy = 0x12345678ULL;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_dword(FLASH_STORAGE_BASE_ADDR, dummy));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_dword(FLASH_STORAGE_BASE_ADDR + (6 * FLASH_STORAGE_PAGE_SIZE), dummy));

    TEST_ASSERT_FALSE(flash_storage_is_page_erased(120U));
    TEST_ASSERT_FALSE(flash_storage_is_page_erased(126U));

    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_erase_all_nvm_pages());

    for (uint32_t p = FLASH_STORAGE_START_PAGE; p <= FLASH_STORAGE_END_PAGE; p++) {
        TEST_ASSERT_TRUE(flash_storage_is_page_erased(p));
    }
}

/**
 * @brief TC-S5-T3.1-01: NULL Pointer and Zero Length Safety Guards.
 */
static void test_flash_null_pointer_guards(void) {
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_storage_write_bytes(FLASH_STORAGE_BASE_ADDR, NULL, 10U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_storage_read_bytes(FLASH_STORAGE_BASE_ADDR, NULL, 10U));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_bytes(FLASH_STORAGE_BASE_ADDR, (const uint8_t *)"A", 0U));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_read_bytes(FLASH_STORAGE_BASE_ADDR, (uint8_t *)"A", 0U));
}

/**
 * @brief Lock and Unlock Sequences.
 */
static void test_flash_lock_unlock(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_unlock());
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_lock());
}

/* ========================================================================== */
/* Category B: Circular Ring Buffer & Wear-Leveling Tests (S5-T3.2)            */
/* ========================================================================== */

/**
 * @brief TC-RING-01: Fresh Partition Initialization.
 */
static void test_ring_fresh_init(void) {
    flash_ring_state_t state;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_get_state(&state));

    TEST_ASSERT_EQUAL_UINT32(0U, state.valid_count);
    TEST_ASSERT_EQUAL_UINT32(0U, state.head_index);
    TEST_ASSERT_EQUAL_UINT32(0U, state.tail_index);
    TEST_ASSERT_TRUE(state.is_empty);
    TEST_ASSERT_FALSE(state.is_full);
    TEST_ASSERT_EQUAL_UINT32(0U, flash_ring_get_count());
    TEST_ASSERT_EQUAL_UINT32(FLASH_RING_TOTAL_CAPACITY, flash_ring_get_capacity());
    TEST_ASSERT_TRUE(flash_ring_is_empty());
    TEST_ASSERT_FALSE(flash_ring_is_full());
}

/**
 * @brief TC-RING-02: Single Push & State Verification.
 */
static void test_ring_single_push(void) {
    uint8_t payload[FLASH_RING_TELEMETRY_PAYLOAD_SIZE];
    generate_dummy_telemetry_payload(payload, 1U);

    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(payload, 101U));
    TEST_ASSERT_EQUAL_UINT32(1U, flash_ring_get_count());
    TEST_ASSERT_FALSE(flash_ring_is_empty());

    flash_ring_state_t state;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_get_state(&state));
    TEST_ASSERT_EQUAL_UINT32(1U, state.head_index);
    TEST_ASSERT_EQUAL_UINT32(0U, state.tail_index);
    TEST_ASSERT_EQUAL_UINT32(1U, state.valid_count);
    TEST_ASSERT_EQUAL_UINT16(102U, state.next_seq_id);
}

/**
 * @brief TC-RING-03: Peek vs Pop Data Integrity and FIFO Ordering.
 */
static void test_ring_peek_and_pop_fifo(void) {
    uint8_t p1[12], p2[12], p3[12];
    generate_dummy_telemetry_payload(p1, 10U);
    generate_dummy_telemetry_payload(p2, 20U);
    generate_dummy_telemetry_payload(p3, 30U);

    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p1, 100U));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p2, 101U));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p3, 102U));
    TEST_ASSERT_EQUAL_UINT32(3U, flash_ring_get_count());

    /* Peek offset 0 (oldest) */
    flash_record_t peek_rec;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_peek(0U, &peek_rec));
    TEST_ASSERT_EQUAL_HEX16(FLASH_RING_MAGIC_VALID, peek_rec.magic_status);
    TEST_ASSERT_EQUAL_UINT16(100U, peek_rec.sequence_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p1, peek_rec.payload, 12U);
    TEST_ASSERT_EQUAL_UINT32(3U, flash_ring_get_count()); /* Unchanged */

    /* Peek offset 1 */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_peek(1U, &peek_rec));
    TEST_ASSERT_EQUAL_HEX16(FLASH_RING_MAGIC_VALID, peek_rec.magic_status);
    TEST_ASSERT_EQUAL_UINT16(101U, peek_rec.sequence_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p2, peek_rec.payload, 12U);

    /* Pop first record */
    flash_record_t pop_rec;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_pop(&pop_rec));
    TEST_ASSERT_EQUAL_UINT16(100U, pop_rec.sequence_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p1, pop_rec.payload, 12U);
    TEST_ASSERT_EQUAL_UINT32(2U, flash_ring_get_count());

    /* Pop second record */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_pop(&pop_rec));
    TEST_ASSERT_EQUAL_UINT16(101U, pop_rec.sequence_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p2, pop_rec.payload, 12U);
    TEST_ASSERT_EQUAL_UINT32(1U, flash_ring_get_count());

    /* Pop third record */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_pop(&pop_rec));
    TEST_ASSERT_EQUAL_UINT16(102U, pop_rec.sequence_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p3, pop_rec.payload, 12U);
    TEST_ASSERT_EQUAL_UINT32(0U, flash_ring_get_count());
    TEST_ASSERT_TRUE(flash_ring_is_empty());

    /* Pop on empty buffer returns STATUS_ERROR_EMPTY */
    TEST_ASSERT_EQUAL(STATUS_ERROR_EMPTY, flash_ring_pop(&pop_rec));
}

/**
 * @brief TC-RING-04: Batch Mark Transmitted.
 */
static void test_ring_mark_transmitted(void) {
    uint8_t p[12];
    for (uint16_t i = 0; i < 5; i++) {
        generate_dummy_telemetry_payload(p, i);
        TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, (uint16_t)(i + 1U)));
    }
    TEST_ASSERT_EQUAL_UINT32(5U, flash_ring_get_count());

    /* Mark first 3 transmitted */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_mark_transmitted(3U));
    TEST_ASSERT_EQUAL_UINT32(2U, flash_ring_get_count());

    /* Verify next pop returns 4th record (seq_id 4) */
    flash_record_t pop_rec;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_pop(&pop_rec));
    TEST_ASSERT_EQUAL_UINT16(4U, pop_rec.sequence_id);
}

/**
 * @brief TC-RING-05: Page Boundary Rollover & Automatic Erase.
 */
static void test_ring_page_boundary_rollover(void) {
    uint8_t p[12];
    /* Fill Page 120 with 128 records */
    for (uint16_t i = 0; i < 128; i++) {
        generate_dummy_telemetry_payload(p, i);
        TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, i));
    }
    TEST_ASSERT_EQUAL_UINT32(128U, flash_ring_get_count());

    /* Push 129th record -> enters Page 121 (slot 128) */
    generate_dummy_telemetry_payload(p, 128);
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, 128U));
    TEST_ASSERT_EQUAL_UINT32(129U, flash_ring_get_count());

    flash_ring_state_t state;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_get_state(&state));
    TEST_ASSERT_EQUAL_UINT32(129U, state.head_index);
    TEST_ASSERT_EQUAL_UINT32(0U, state.tail_index);
}

/**
 * @brief TC-RING-06: Full Ring Buffer Overwrite (896 Records).
 */
static void test_ring_full_capacity_overwrite(void) {
    uint8_t p[12];
    /* Fill entire 896 record capacity across Pages 120..126 */
    for (uint16_t i = 0; i < FLASH_RING_TOTAL_CAPACITY; i++) {
        generate_dummy_telemetry_payload(p, i);
        TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, i));
    }
    TEST_ASSERT_EQUAL_UINT32(FLASH_RING_TOTAL_CAPACITY, flash_ring_get_count());
    TEST_ASSERT_TRUE(flash_ring_is_full());

    /* Push 897th record (causes wrap-around overwrite) */
    generate_dummy_telemetry_payload(p, 999);
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, 999U));

    TEST_ASSERT_EQUAL_UINT32(FLASH_RING_TOTAL_CAPACITY, flash_ring_get_count());
    TEST_ASSERT_TRUE(flash_ring_is_full());

    /* Oldest record (seq 0) dropped, tail advanced to 1 */
    flash_ring_state_t state;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_get_state(&state));
    TEST_ASSERT_EQUAL_UINT32(1U, state.tail_index);

    flash_record_t pop_rec;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_pop(&pop_rec));
    TEST_ASSERT_EQUAL_UINT16(1U, pop_rec.sequence_id);
}

/**
 * @brief TC-RING-07: Power-Loss Recovery & Fast Boot Scan.
 */
static void test_ring_power_loss_recovery(void) {
    uint8_t p[12];
    for (uint16_t i = 0; i < 75; i++) {
        generate_dummy_telemetry_payload(p, i);
        TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, (uint16_t)(i + 10U)));
    }
    TEST_ASSERT_EQUAL_UINT32(75U, flash_ring_get_count());

    /* Simulate MCU reset / reboot by re-calling init */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_init());

    flash_ring_state_t state;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_get_state(&state));
    TEST_ASSERT_EQUAL_UINT32(75U, state.valid_count);
    TEST_ASSERT_EQUAL_UINT32(75U, state.head_index);
    TEST_ASSERT_EQUAL_UINT32(0U, state.tail_index);
    TEST_ASSERT_EQUAL_UINT16(85U, state.next_seq_id);
}

/**
 * @brief TC-RING-08: Corrupted Slot Detection & Recovery.
 */
static void test_ring_corrupted_slot_recovery(void) {
    uint8_t p[12];
    for (uint16_t i = 0; i < 10; i++) {
        generate_dummy_telemetry_payload(p, i);
        TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, i));
    }

    /* Corrupt magic word in slot 5 */
    uint32_t slot5_addr = FLASH_STORAGE_BASE_ADDR + (5U * FLASH_RING_RECORD_SIZE);
    uint16_t corrupted_magic = 0x5A5AU;
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_bytes(slot5_addr, (const uint8_t *)&corrupted_magic, sizeof(uint16_t)));

    /* Re-init scanner */
    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_init());

    /* Valid count is 9 (corrupted slot skipped) */
    TEST_ASSERT_EQUAL_UINT32(9U, flash_ring_get_count());
}

/**
 * @brief TC-RING-09: Ring Clear Resets All Data Pages.
 */
static void test_ring_clear_resets_all(void) {
    uint8_t p[12];
    for (uint16_t i = 0; i < 50; i++) {
        generate_dummy_telemetry_payload(p, i);
        TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_push(p, i));
    }
    TEST_ASSERT_EQUAL_UINT32(50U, flash_ring_get_count());

    TEST_ASSERT_EQUAL(STATUS_OK, flash_ring_clear());
    TEST_ASSERT_EQUAL_UINT32(0U, flash_ring_get_count());
    TEST_ASSERT_TRUE(flash_ring_is_empty());
}

/**
 * @brief TC-RING-10: Defensive Parameter Clamping & NULL Guards.
 */
static void test_ring_parameter_guards(void) {
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_ring_push(NULL, 1U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_ring_pop(NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_ring_peek(0U, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_ring_get_state(NULL));

    flash_record_t dummy;
    TEST_ASSERT_EQUAL(STATUS_ERROR_OUT_OF_BOUNDS, flash_ring_peek(0U, &dummy));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_ring_mark_transmitted(0U));
}

/* ========================================================================== */
/* Main Unity Test Runner                                                     */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Category A: Low-Level Flash HAL Driver Tests */
    RUN_TEST(test_flash_page_erase_and_check);
    RUN_TEST(test_flash_write_dword_success);
    RUN_TEST(test_flash_bitwise_and_programming);
    RUN_TEST(test_flash_out_of_bounds_guards);
    RUN_TEST(test_flash_unaligned_write_rejection);
    RUN_TEST(test_flash_write_bytes_arbitrary_length);
    RUN_TEST(test_flash_erase_all_nvm_pages);
    RUN_TEST(test_flash_null_pointer_guards);
    RUN_TEST(test_flash_lock_unlock);

    /* Category B: Circular Ring Buffer & Wear-Leveling Tests */
    RUN_TEST(test_ring_fresh_init);
    RUN_TEST(test_ring_single_push);
    RUN_TEST(test_ring_peek_and_pop_fifo);
    RUN_TEST(test_ring_mark_transmitted);
    RUN_TEST(test_ring_page_boundary_rollover);
    RUN_TEST(test_ring_full_capacity_overwrite);
    RUN_TEST(test_ring_power_loss_recovery);
    RUN_TEST(test_ring_corrupted_slot_recovery);
    RUN_TEST(test_ring_clear_resets_all);
    RUN_TEST(test_ring_parameter_guards);

    return UNITY_END();
}
