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
}

void tearDown(void) {
    /* Verify flash returns to locked state or clean state */
}

/* ========================================================================== */
/* Category A: Low-Level Flash HAL Driver Tests (S5-T3.1)                      */
/* ========================================================================== */

/**
 * @brief TC-S5-T3.1-05: Page Erase Operation & is_page_erased check.
 */
void test_flash_page_erase_and_check(void) {
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
void test_flash_write_dword_success(void) {
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
void test_flash_bitwise_and_programming(void) {
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
void test_flash_out_of_bounds_guards(void) {
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
void test_flash_unaligned_write_rejection(void) {
    /* 0x0803C004 is 4-byte aligned but not 8-byte aligned */
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_write_dword(0x0803C004U, 0x1234ULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_write_dword(0x0803C002U, 0x1234ULL));
    TEST_ASSERT_EQUAL(STATUS_ERROR_INVALID_PARAM, flash_storage_write_dword(0x0803C007U, 0x1234ULL));
}

/**
 * @brief TC-S5-T3.1-07: Arbitrary Byte Buffer Writing with Read-Modify-Write Preservation.
 */
void test_flash_write_bytes_arbitrary_length(void) {
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
void test_flash_erase_all_nvm_pages(void) {
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
void test_flash_null_pointer_guards(void) {
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_storage_write_bytes(FLASH_STORAGE_BASE_ADDR, NULL, 10U));
    TEST_ASSERT_EQUAL(STATUS_ERROR_NULL_POINTER, flash_storage_read_bytes(FLASH_STORAGE_BASE_ADDR, NULL, 10U));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_write_bytes(FLASH_STORAGE_BASE_ADDR, (const uint8_t *)"A", 0U));
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_read_bytes(FLASH_STORAGE_BASE_ADDR, (uint8_t *)"A", 0U));
}

/**
 * @brief Lock and Unlock Sequences.
 */
void test_flash_lock_unlock(void) {
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_unlock());
    TEST_ASSERT_EQUAL(STATUS_OK, flash_storage_lock());
}

/* ========================================================================== */
/* Main Unity Test Runner                                                     */
/* ========================================================================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_flash_page_erase_and_check);
    RUN_TEST(test_flash_write_dword_success);
    RUN_TEST(test_flash_bitwise_and_programming);
    RUN_TEST(test_flash_out_of_bounds_guards);
    RUN_TEST(test_flash_unaligned_write_rejection);
    RUN_TEST(test_flash_write_bytes_arbitrary_length);
    RUN_TEST(test_flash_erase_all_nvm_pages);
    RUN_TEST(test_flash_null_pointer_guards);
    RUN_TEST(test_flash_lock_unlock);

    return UNITY_END();
}
