/**
 * @file    flash_storage.c
 * @brief   STM32WLE5 on-chip Flash memory driver implementation for offline telemetry storage.
 * @details Implements 2 KB physical page erase, 64-bit double-word programming, and memory-mapped
 *          direct read access for dedicated NVM Pages 120-127 (16 KB total).
 *          Dual-target architecture supporting host mock emulation and STM32CubeWL target HAL.
 */

#include "flash_storage.h"
#include <string.h>

#if defined(__has_include)
#if __has_include("stm32wlxx_hal.h")
#define HAVE_STM32WLXX_HAL 1
#endif
#endif

#if defined(HOST_TEST) || !defined(STM32WLE5xx) || !defined(HAVE_STM32WLXX_HAL)
/* ========================================================================== */
/* Host Desktop Simulation / Unity Mock Emulation Engine                      */
/* ========================================================================== */

static uint8_t s_mock_flash_nvm[FLASH_STORAGE_TOTAL_CAPACITY_BYTES];
static bool s_mock_flash_unlocked = false;
static bool s_mock_initialized = false;

/* Persistent Page 127 journal tracking */
static uint32_t s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
static uint32_t s_metadata_generation = 0U;
static uint32_t s_metadata_epoch = 0U;
static flash_ring_metadata_t s_cached_metadata;
static bool s_cached_metadata_valid = false;

static void ensure_mock_initialized(void) {
    if (!s_mock_initialized) {
        memset(s_mock_flash_nvm, FLASH_STORAGE_ERASED_BYTE, sizeof(s_mock_flash_nvm));
        s_mock_initialized = true;
    }
}

static inline bool is_valid_nvm_address(uint32_t addr, size_t len) {
    if (addr < FLASH_STORAGE_BASE_ADDR || len == 0U) {
        return false;
    }
    uint64_t end_addr = (uint64_t)addr + (uint64_t)len - 1ULL;
    return (end_addr <= (uint64_t)FLASH_STORAGE_END_ADDR);
}

status_t flash_storage_init(void) {
    ensure_mock_initialized();
    s_mock_flash_unlocked = false;
    s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
    s_metadata_generation = 0U;
    s_metadata_epoch = 0U;
    s_cached_metadata_valid = false;
    return STATUS_OK;
}

status_t flash_storage_unlock(void) {
    ensure_mock_initialized();
    s_mock_flash_unlocked = true;
    return STATUS_OK;
}

status_t flash_storage_lock(void) {
    s_mock_flash_unlocked = false;
    return STATUS_OK;
}

status_t flash_storage_erase_page(uint32_t page_num) {
    if (page_num < FLASH_STORAGE_START_PAGE || page_num > FLASH_STORAGE_END_PAGE) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    ensure_mock_initialized();
    uint32_t offset = (page_num - FLASH_STORAGE_START_PAGE) * FLASH_STORAGE_PAGE_SIZE;
    memset(&s_mock_flash_nvm[offset], FLASH_STORAGE_ERASED_BYTE, FLASH_STORAGE_PAGE_SIZE);

    if (page_num == FLASH_RING_METADATA_PAGE) {
        s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
        s_metadata_generation = 0U;
        s_metadata_epoch = 0U;
        s_cached_metadata_valid = false;
    }

    return STATUS_OK;
}

status_t flash_storage_erase_all_nvm_pages(void) {
    status_t status = flash_storage_unlock();
    if (status != STATUS_OK) {
        return status;
    }

    for (uint32_t p = FLASH_STORAGE_START_PAGE; p <= FLASH_STORAGE_END_PAGE; p++) {
        status = flash_storage_erase_page(p);
        if (status != STATUS_OK) {
            (void)flash_storage_lock();
            return status;
        }
    }

    return flash_storage_lock();
}

status_t flash_storage_write_dword(uint32_t address, uint64_t data) {
    if (!is_valid_nvm_address(address, FLASH_STORAGE_PROG_UNIT_BYTES)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    if ((address & (FLASH_STORAGE_PROG_UNIT_BYTES - 1U)) != 0U) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    ensure_mock_initialized();
    uint32_t offset = address - FLASH_STORAGE_BASE_ADDR;

    /* Physical Flash behavior: programming only transitions 1s to 0s (bitwise AND) */
    uint64_t existing_dword = 0ULL;
    memcpy(&existing_dword, &s_mock_flash_nvm[offset], sizeof(uint64_t));
    uint64_t programmed_dword = existing_dword & data;
    memcpy(&s_mock_flash_nvm[offset], &programmed_dword, sizeof(uint64_t));

    return STATUS_OK;
}

status_t flash_storage_write_bytes(uint32_t address, const uint8_t *data, size_t length) {
    if (data == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (length == 0U) {
        return STATUS_OK;
    }
    if (!is_valid_nvm_address(address, length)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    status_t status = flash_storage_unlock();
    if (status != STATUS_OK) {
        return status;
    }

    size_t bytes_written = 0U;
    uint32_t current_addr = address;

    while (bytes_written < length) {
        uint32_t dword_aligned_addr = current_addr & ~(uint32_t)(FLASH_STORAGE_PROG_UNIT_BYTES - 1U);
        uint32_t offset_in_dword = current_addr & (FLASH_STORAGE_PROG_UNIT_BYTES - 1U);

        uint8_t dword_buf[FLASH_STORAGE_PROG_UNIT_BYTES];
        memset(dword_buf, FLASH_STORAGE_ERASED_BYTE, FLASH_STORAGE_PROG_UNIT_BYTES);

        size_t chunk_len = FLASH_STORAGE_PROG_UNIT_BYTES - offset_in_dword;
        if (chunk_len > (length - bytes_written)) {
            chunk_len = length - bytes_written;
        }

        /* Read-modify-write preservation of untouched bytes in double-word */
        if (chunk_len < FLASH_STORAGE_PROG_UNIT_BYTES) {
            uint32_t mock_off = dword_aligned_addr - FLASH_STORAGE_BASE_ADDR;
            memcpy(dword_buf, &s_mock_flash_nvm[mock_off], FLASH_STORAGE_PROG_UNIT_BYTES);
        }

        memcpy(&dword_buf[offset_in_dword], &data[bytes_written], chunk_len);

        uint64_t dword_val = 0ULL;
        memcpy(&dword_val, dword_buf, sizeof(uint64_t));

        status = flash_storage_write_dword(dword_aligned_addr, dword_val);
        if (status != STATUS_OK) {
            (void)flash_storage_lock();
            return status;
        }

        bytes_written += chunk_len;
        current_addr += (uint32_t)chunk_len;
    }

    return flash_storage_lock();
}

status_t flash_storage_read_bytes(uint32_t address, uint8_t *buffer, size_t length) {
    if (buffer == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (length == 0U) {
        return STATUS_OK;
    }
    if (!is_valid_nvm_address(address, length)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    ensure_mock_initialized();
    uint32_t offset = address - FLASH_STORAGE_BASE_ADDR;
    memcpy(buffer, &s_mock_flash_nvm[offset], length);
    return STATUS_OK;
}

bool flash_storage_is_page_erased(uint32_t page_num) {
    if (page_num < FLASH_STORAGE_START_PAGE || page_num > FLASH_STORAGE_END_PAGE) {
        return false;
    }

    ensure_mock_initialized();
    uint32_t offset = (page_num - FLASH_STORAGE_START_PAGE) * FLASH_STORAGE_PAGE_SIZE;
    for (size_t i = 0U; i < FLASH_STORAGE_PAGE_SIZE; i++) {
        if (s_mock_flash_nvm[offset + i] != FLASH_STORAGE_ERASED_BYTE) {
            return false;
        }
    }
    return true;
}

#else
/* ========================================================================== */
/* Target STM32WLE5 Hardware Register & HAL Driver Implementation             */
/* ========================================================================== */

#if defined(HAVE_STM32WLXX_HAL)
#include "stm32wlxx_hal.h"
#endif

static inline bool is_valid_nvm_address(uint32_t addr, size_t len) {
    if (addr < FLASH_STORAGE_BASE_ADDR || len == 0U) {
        return false;
    }
    uint64_t end_addr = (uint64_t)addr + (uint64_t)len - 1ULL;
    return (end_addr <= (uint64_t)FLASH_STORAGE_END_ADDR);
}

status_t flash_storage_init(void) {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
    s_metadata_generation = 0U;
    s_metadata_epoch = 0U;
    s_cached_metadata_valid = false;
    return STATUS_OK;
}

status_t flash_storage_unlock(void) {
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return STATUS_ERROR_HARDWARE;
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    return STATUS_OK;
}

status_t flash_storage_lock(void) {
    if (HAL_FLASH_Lock() != HAL_OK) {
        return STATUS_ERROR_HARDWARE;
    }
    return STATUS_OK;
}

status_t flash_storage_erase_page(uint32_t page_num) {
    if (page_num < FLASH_STORAGE_START_PAGE || page_num > FLASH_STORAGE_END_PAGE) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    bool relock = false;
    if (READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U) {
        if (HAL_FLASH_Unlock() != HAL_OK) {
            return STATUS_ERROR_HARDWARE;
        }
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        relock = true;
    }

    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.Page        = page_num;
    erase_init.NbPages     = 1U;

    uint32_t page_error = 0U;
    HAL_StatusTypeDef hal_status = HAL_FLASHEx_Erase(&erase_init, &page_error);

    if (relock) {
        (void)HAL_FLASH_Lock();
    }

    if (hal_status != HAL_OK) {
        return STATUS_ERROR_HARDWARE;
    }

    if (page_num == FLASH_RING_METADATA_PAGE) {
        s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
        s_metadata_generation = 0U;
        s_metadata_epoch = 0U;
        s_cached_metadata_valid = false;
    }

    return STATUS_OK;
}

status_t flash_storage_erase_all_nvm_pages(void) {
    status_t status = flash_storage_unlock();
    if (status != STATUS_OK) {
        return status;
    }

    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.Page        = FLASH_STORAGE_START_PAGE;
    erase_init.NbPages     = FLASH_STORAGE_TOTAL_PAGES;

    uint32_t page_error = 0U;
    HAL_StatusTypeDef hal_status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    (void)flash_storage_lock();

    if (hal_status != HAL_OK) {
        return STATUS_ERROR_HARDWARE;
    }

    return STATUS_OK;
}

status_t flash_storage_write_dword(uint32_t address, uint64_t data) {
    if (!is_valid_nvm_address(address, FLASH_STORAGE_PROG_UNIT_BYTES)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }
    if ((address & (FLASH_STORAGE_PROG_UNIT_BYTES - 1U)) != 0U) {
        return STATUS_ERROR_INVALID_PARAM;
    }

    bool relock = false;
    if (READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U) {
        if (HAL_FLASH_Unlock() != HAL_OK) {
            return STATUS_ERROR_HARDWARE;
        }
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        relock = true;
    }

    HAL_StatusTypeDef hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data);

    if (relock) {
        (void)HAL_FLASH_Lock();
    }

    if (hal_status != HAL_OK) {
        return STATUS_ERROR_HARDWARE;
    }

    return STATUS_OK;
}

status_t flash_storage_write_bytes(uint32_t address, const uint8_t *data, size_t length) {
    if (data == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (length == 0U) {
        return STATUS_OK;
    }
    if (!is_valid_nvm_address(address, length)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    status_t status = flash_storage_unlock();
    if (status != STATUS_OK) {
        return status;
    }

    size_t bytes_written = 0U;
    uint32_t current_addr = address;

    while (bytes_written < length) {
        uint32_t dword_aligned_addr = current_addr & ~(uint32_t)(FLASH_STORAGE_PROG_UNIT_BYTES - 1U);
        uint32_t offset_in_dword = current_addr & (FLASH_STORAGE_PROG_UNIT_BYTES - 1U);

        uint8_t dword_buf[FLASH_STORAGE_PROG_UNIT_BYTES];
        memset(dword_buf, FLASH_STORAGE_ERASED_BYTE, FLASH_STORAGE_PROG_UNIT_BYTES);

        size_t chunk_len = FLASH_STORAGE_PROG_UNIT_BYTES - offset_in_dword;
        if (chunk_len > (length - bytes_written)) {
            chunk_len = length - bytes_written;
        }

        if (chunk_len < FLASH_STORAGE_PROG_UNIT_BYTES) {
            memcpy(dword_buf, (const void *)(uintptr_t)dword_aligned_addr, FLASH_STORAGE_PROG_UNIT_BYTES);
        }

        memcpy(&dword_buf[offset_in_dword], &data[bytes_written], chunk_len);

        uint64_t dword_val = 0ULL;
        memcpy(&dword_val, dword_buf, sizeof(uint64_t));

        status = flash_storage_write_dword(dword_aligned_addr, dword_val);
        if (status != STATUS_OK) {
            (void)flash_storage_lock();
            return status;
        }

        bytes_written += chunk_len;
        current_addr += (uint32_t)chunk_len;
    }

    return flash_storage_lock();
}

status_t flash_storage_read_bytes(uint32_t address, uint8_t *buffer, size_t length) {
    if (buffer == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (length == 0U) {
        return STATUS_OK;
    }
    if (!is_valid_nvm_address(address, length)) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    /* Direct memory-mapped read from STM32 Flash space */
    memcpy(buffer, (const void *)(uintptr_t)address, length);
    return STATUS_OK;
}

bool flash_storage_is_page_erased(uint32_t page_num) {
    if (page_num < FLASH_STORAGE_START_PAGE || page_num > FLASH_STORAGE_END_PAGE) {
        return false;
    }

    uint32_t page_addr = FLASH_STORAGE_BASE_ADDR + ((page_num - FLASH_STORAGE_START_PAGE) * FLASH_STORAGE_PAGE_SIZE);
    const uint64_t *p_dword = (const uint64_t *)(uintptr_t)page_addr;
    size_t num_dwords = FLASH_STORAGE_PAGE_SIZE / sizeof(uint64_t);

    for (size_t i = 0U; i < num_dwords; i++) {
        if (p_dword[i] != FLASH_STORAGE_ERASED_DWORD) {
            return false;
        }
    }
    return true;
}

#endif

/* ========================================================================== */
/* Circular Ring Buffer State & Internal Helpers (S5-T3.2)                     */
/* ========================================================================== */

static flash_ring_state_t s_ring_state = {
    .head_index  = 0U,
    .tail_index  = 0U,
    .valid_count = 0U,
    .next_seq_id = 0U,
    .is_full     = false,
    .is_empty    = true
};

static bool s_ring_initialized = false;

static uint32_t flash_ring_slot_to_addr(uint32_t slot_idx) {
    if (slot_idx >= FLASH_RING_TOTAL_CAPACITY) {
        return 0U;
    }
    uint32_t page_offset = slot_idx / FLASH_RING_RECORDS_PER_PAGE;
    uint32_t slot_in_page = slot_idx % FLASH_RING_RECORDS_PER_PAGE;
    return FLASH_STORAGE_BASE_ADDR + (page_offset * FLASH_STORAGE_PAGE_SIZE) +
           (slot_in_page * FLASH_RING_RECORD_SIZE);
}

static status_t flash_ring_read_slot(uint32_t slot_idx, flash_record_t *record) {
    if (record == NULL || slot_idx >= FLASH_RING_TOTAL_CAPACITY) {
        return STATUS_ERROR_INVALID_PARAM;
    }
    uint32_t addr = flash_ring_slot_to_addr(slot_idx);
    return flash_storage_read_bytes(addr, (uint8_t *)record, FLASH_RING_RECORD_SIZE);
}

/* ========================================================================== */
/* Wear-Leveling Circular Ring Buffer Implementation (S5-T3.2)                 */
/* ========================================================================== */

status_t flash_ring_init(void) {
    status_t status = flash_storage_init();
    if (status != STATUS_OK) {
        return status;
    }

    s_ring_state.head_index  = 0U;
    s_ring_state.tail_index  = 0U;
    s_ring_state.valid_count = 0U;
    s_ring_state.next_seq_id = 0U;
    s_ring_state.is_full     = false;
    s_ring_state.is_empty    = true;

    uint32_t first_valid_idx  = FLASH_RING_TOTAL_CAPACITY;
    uint32_t first_erased_idx = FLASH_RING_TOTAL_CAPACITY;
    uint32_t last_valid_idx   = FLASH_RING_TOTAL_CAPACITY;
    uint32_t max_seq_slot     = 0U;
    uint16_t max_seq_id       = 0U;
    bool     has_records      = false;

    /* Scan all 896 record slots across Pages 120..126 */
    for (uint32_t i = 0U; i < FLASH_RING_TOTAL_CAPACITY; i++) {
        flash_record_t rec;
        if (flash_ring_read_slot(i, &rec) != STATUS_OK) {
            continue;
        }

        if (rec.magic_status == FLASH_RING_MAGIC_VALID) {
            if (first_valid_idx == FLASH_RING_TOTAL_CAPACITY) {
                first_valid_idx = i;
            }
            last_valid_idx = i;
            s_ring_state.valid_count++;
            if (!has_records || rec.sequence_id > max_seq_id) {
                max_seq_id = rec.sequence_id;
                max_seq_slot = i;
                has_records = true;
            }
        } else if (rec.magic_status == FLASH_RING_MAGIC_ERASED) {
            if (first_erased_idx == FLASH_RING_TOTAL_CAPACITY) {
                first_erased_idx = i;
            }
        }
    }

    if (s_ring_state.valid_count == 0U) {
        /* Buffer is completely empty */
        s_ring_state.head_index  = (first_erased_idx != FLASH_RING_TOTAL_CAPACITY) ? first_erased_idx : 0U;
        s_ring_state.tail_index  = s_ring_state.head_index;
        s_ring_state.is_empty    = true;
        s_ring_state.is_full     = false;
        s_ring_state.next_seq_id = 0U;
    } else {
        s_ring_state.is_empty = false;
        s_ring_state.next_seq_id = (uint16_t)(max_seq_id + 1U);

        if (first_erased_idx != FLASH_RING_TOTAL_CAPACITY) {
            s_ring_state.head_index = first_erased_idx;
            s_ring_state.is_full = false;
        } else {
            /* Full buffer or wrap-around boundary */
            s_ring_state.head_index = (max_seq_slot + 1U) % FLASH_RING_TOTAL_CAPACITY;
            s_ring_state.is_full = (s_ring_state.valid_count >= FLASH_RING_TOTAL_CAPACITY);
        }

        /* Determine tail index */
        uint32_t determined_tail = first_valid_idx;
        if (s_ring_state.valid_count >= FLASH_RING_TOTAL_CAPACITY) {
            determined_tail = (max_seq_slot + 1U) % FLASH_RING_TOTAL_CAPACITY;
        } else if (first_valid_idx == 0U && last_valid_idx == (FLASH_RING_TOTAL_CAPACITY - 1U)) {
            for (uint32_t i = 1U; i < FLASH_RING_TOTAL_CAPACITY; i++) {
                flash_record_t curr, prev;
                if (flash_ring_read_slot(i, &curr) == STATUS_OK &&
                    flash_ring_read_slot(i - 1U, &prev) == STATUS_OK) {
                    if (curr.magic_status == FLASH_RING_MAGIC_VALID &&
                        prev.magic_status != FLASH_RING_MAGIC_VALID) {
                        determined_tail = i;
                        break;
                    }
                }
            }
        }
        s_ring_state.tail_index = determined_tail;
    }

    s_ring_initialized = true;
    return STATUS_OK;
}

status_t flash_ring_push(const uint8_t *payload, uint16_t seq_id) {
    if (!s_ring_initialized) {
        status_t st = flash_ring_init();
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (payload == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }

    uint32_t target_slot = s_ring_state.head_index;
    uint32_t page_idx = FLASH_STORAGE_START_PAGE + (target_slot / FLASH_RING_RECORDS_PER_PAGE);

    /* Check if target slot resides on a page boundary that requires erase */
    if ((target_slot % FLASH_RING_RECORDS_PER_PAGE) == 0U) {
        if (!flash_storage_is_page_erased(page_idx) && !s_ring_state.is_full) {
            status_t erase_st = flash_storage_erase_page(page_idx);
            if (erase_st != STATUS_OK) {
                return erase_st;
            }

            /* If the erased page contained the tail, advance tail to next page */
            uint32_t tail_page = FLASH_STORAGE_START_PAGE + (s_ring_state.tail_index / FLASH_RING_RECORDS_PER_PAGE);
            if (tail_page == page_idx && !s_ring_state.is_empty) {
                s_ring_state.tail_index = ((target_slot / FLASH_RING_RECORDS_PER_PAGE + 1U) % FLASH_RING_DATA_PAGES) *
                                          FLASH_RING_RECORDS_PER_PAGE;
            }
        }
    }

    /* In host mock emulation, if overwriting an existing slot, reset slot memory to 0xFF */
#if defined(HOST_TEST) || !defined(STM32WLE5xx) || !defined(HAVE_STM32WLXX_HAL)
    if (s_ring_state.is_full) {
        uint32_t slot_addr_temp = flash_ring_slot_to_addr(target_slot);
        uint32_t mock_offset = slot_addr_temp - FLASH_STORAGE_BASE_ADDR;
        memset(&s_mock_flash_nvm[mock_offset], FLASH_STORAGE_ERASED_BYTE, FLASH_RING_RECORD_SIZE);
    }
#endif

    /* Construct 16-byte record */
    flash_record_t record;
    record.magic_status = FLASH_RING_MAGIC_VALID;
    record.sequence_id  = seq_id;
    memcpy(record.payload, payload, FLASH_RING_TELEMETRY_PAYLOAD_SIZE);

    /* Write record in two 64-bit double-word operations */
    uint32_t slot_addr = flash_ring_slot_to_addr(target_slot);
    uint64_t dword0 = 0ULL;
    uint64_t dword1 = 0ULL;
    memcpy(&dword0, &record, sizeof(uint64_t));
    memcpy(&dword1, ((const uint8_t *)&record) + sizeof(uint64_t), sizeof(uint64_t));

    status_t st = flash_storage_write_dword(slot_addr, dword0);
    if (st != STATUS_OK) {
        return st;
    }

    st = flash_storage_write_dword(slot_addr + sizeof(uint64_t), dword1);
    if (st != STATUS_OK) {
        return st;
    }

    /* Advance head pointer */
    s_ring_state.head_index = (s_ring_state.head_index + 1U) % FLASH_RING_TOTAL_CAPACITY;
    s_ring_state.next_seq_id = (uint16_t)(seq_id + 1U);
    s_ring_state.is_empty = false;

    if (s_ring_state.valid_count < FLASH_RING_TOTAL_CAPACITY) {
        s_ring_state.valid_count++;
        if (s_ring_state.valid_count == FLASH_RING_TOTAL_CAPACITY) {
            s_ring_state.is_full = true;
        }
    } else {
        /* Buffer is full: overwrite oldest record, advance tail */
        s_ring_state.tail_index = (s_ring_state.tail_index + 1U) % FLASH_RING_TOTAL_CAPACITY;
        s_ring_state.is_full = true;
    }

    return STATUS_OK;
}

status_t flash_ring_pop(flash_record_t *out_record) {
    if (!s_ring_initialized) {
        status_t st = flash_ring_init();
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (out_record == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (s_ring_state.valid_count == 0U) {
        return STATUS_ERROR_EMPTY;
    }

    /* Find next valid record starting at tail_index */
    flash_record_t rec;
    status_t st = STATUS_ERROR_EMPTY;
    uint32_t scanned = 0U;

    while (scanned < FLASH_RING_TOTAL_CAPACITY) {
        uint32_t slot = s_ring_state.tail_index;
        st = flash_ring_read_slot(slot, &rec);
        if (st == STATUS_OK && rec.magic_status == FLASH_RING_MAGIC_VALID) {
            break;
        }
        s_ring_state.tail_index = (s_ring_state.tail_index + 1U) % FLASH_RING_TOTAL_CAPACITY;
        scanned++;
    }

    if (scanned >= FLASH_RING_TOTAL_CAPACITY || rec.magic_status != FLASH_RING_MAGIC_VALID) {
        s_ring_state.valid_count = 0U;
        s_ring_state.is_empty = true;
        s_ring_state.is_full = false;
        return STATUS_ERROR_EMPTY;
    }

    *out_record = rec;

    /* Invalidate record in Flash (write 0x0000 to magic word) */
    uint32_t slot_addr = flash_ring_slot_to_addr(s_ring_state.tail_index);
    uint16_t transmitted_magic = FLASH_RING_MAGIC_TRANSMITTED;
    st = flash_storage_write_bytes(slot_addr, (const uint8_t *)&transmitted_magic, sizeof(uint16_t));
    if (st != STATUS_OK) {
        return st;
    }

    /* Advance tail */
    s_ring_state.tail_index = (s_ring_state.tail_index + 1U) % FLASH_RING_TOTAL_CAPACITY;
    if (s_ring_state.valid_count > 0U) {
        s_ring_state.valid_count--;
    }
    s_ring_state.is_full = false;
    if (s_ring_state.valid_count == 0U) {
        s_ring_state.is_empty = true;
    }

    return STATUS_OK;
}

status_t flash_ring_peek(uint32_t offset_from_tail, flash_record_t *out_record) {
    if (!s_ring_initialized) {
        status_t st = flash_ring_init();
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (out_record == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    if (offset_from_tail >= s_ring_state.valid_count) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    /* Walk forward offset_from_tail valid records from tail_index */
    uint32_t valid_seen = 0U;
    uint32_t slot = s_ring_state.tail_index;

    for (uint32_t i = 0U; i < FLASH_RING_TOTAL_CAPACITY; i++) {
        flash_record_t rec;
        status_t st = flash_ring_read_slot(slot, &rec);
        if (st == STATUS_OK && rec.magic_status == FLASH_RING_MAGIC_VALID) {
            if (valid_seen == offset_from_tail) {
                *out_record = rec;
                return STATUS_OK;
            }
            valid_seen++;
        }
        slot = (slot + 1U) % FLASH_RING_TOTAL_CAPACITY;
    }

    return STATUS_ERROR_OUT_OF_BOUNDS;
}

status_t flash_ring_mark_transmitted(uint32_t count) {
    if (!s_ring_initialized) {
        status_t st = flash_ring_init();
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (count == 0U) {
        return STATUS_ERROR_INVALID_PARAM;
    }
    if (count > s_ring_state.valid_count) {
        return STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t marked = 0U;
    while (marked < count && s_ring_state.valid_count > 0U) {
        flash_record_t rec;
        status_t st = flash_ring_read_slot(s_ring_state.tail_index, &rec);
        if (st == STATUS_OK && rec.magic_status == FLASH_RING_MAGIC_VALID) {
            uint32_t slot_addr = flash_ring_slot_to_addr(s_ring_state.tail_index);
            uint16_t trans_magic = FLASH_RING_MAGIC_TRANSMITTED;
            st = flash_storage_write_bytes(slot_addr, (const uint8_t *)&trans_magic, sizeof(uint16_t));
            if (st != STATUS_OK) {
                return st;
            }
            s_ring_state.valid_count--;
            marked++;
        }
        s_ring_state.tail_index = (s_ring_state.tail_index + 1U) % FLASH_RING_TOTAL_CAPACITY;
    }

    s_ring_state.is_full = false;
    if (s_ring_state.valid_count == 0U) {
        s_ring_state.is_empty = true;
    }

    return STATUS_OK;
}

uint32_t flash_ring_get_count(void) {
    if (!s_ring_initialized) {
        (void)flash_ring_init();
    }
    return s_ring_state.valid_count;
}

uint32_t flash_ring_get_capacity(void) {
    return FLASH_RING_TOTAL_CAPACITY;
}

bool flash_ring_is_full(void) {
    if (!s_ring_initialized) {
        (void)flash_ring_init();
    }
    return s_ring_state.is_full;
}

bool flash_ring_is_empty(void) {
    if (!s_ring_initialized) {
        (void)flash_ring_init();
    }
    return s_ring_state.is_empty;
}

uint32_t flash_ring_get_head_index(void) {
    if (!s_ring_initialized) {
        (void)flash_ring_init();
    }
    return s_ring_state.head_index;
}

uint32_t flash_ring_get_tail_index(void) {
    if (!s_ring_initialized) {
        (void)flash_ring_init();
    }
    return s_ring_state.tail_index;
}

status_t flash_ring_get_state(flash_ring_state_t *out_state) {
    if (!s_ring_initialized) {
        status_t st = flash_ring_init();
        if (st != STATUS_OK) {
            return st;
        }
    }
    if (out_state == NULL) {
        return STATUS_ERROR_NULL_POINTER;
    }
    memcpy(out_state, &s_ring_state, sizeof(flash_ring_state_t));
    return STATUS_OK;
}

status_t flash_ring_clear(void) {
    for (uint32_t p = FLASH_STORAGE_START_PAGE; p < (FLASH_STORAGE_START_PAGE + FLASH_RING_DATA_PAGES); p++) {
        status_t st = flash_storage_erase_page(p);
        if (st != STATUS_OK) {
            return st;
        }
    }

    s_ring_state.head_index  = 0U;
    s_ring_state.tail_index  = 0U;
    s_ring_state.valid_count = 0U;
    s_ring_state.next_seq_id = 0U;
    s_ring_state.is_full     = false;
    s_ring_state.is_empty    = true;
    s_ring_initialized       = true;

    return STATUS_OK;
}

/* ========================================================================== */
/* Persistent Metadata Header & Journal Implementation (S8-T1.1)              */
/* ========================================================================== */

uint16_t flash_metadata_calc_crc16(const uint8_t *p_data, size_t length) {
    if (p_data == NULL || length == 0U) {
        return 0xFFFFU;
    }
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0U; i < length; i++) {
        crc ^= ((uint16_t)p_data[i] << 8);
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static bool is_slot_erased(const flash_ring_metadata_t *entry) {
    const uint64_t *dwords = (const uint64_t *)(const void *)entry;
    for (size_t i = 0U; i < (FLASH_METADATA_ENTRY_SIZE / sizeof(uint64_t)); i++) {
        if (dwords[i] != FLASH_STORAGE_ERASED_DWORD) {
            return false;
        }
    }
    return true;
}

bool flash_metadata_is_consistent(const flash_ring_metadata_t *p_meta) {
    if (p_meta == NULL) {
        return false;
    }
    if (p_meta->magic != FLASH_METADATA_MAGIC) {
        return false;
    }
    if (p_meta->head_index >= FLASH_RING_TOTAL_CAPACITY ||
        p_meta->tail_index >= FLASH_RING_TOTAL_CAPACITY ||
        p_meta->valid_count > FLASH_RING_TOTAL_CAPACITY) {
        return false;
    }
    uint16_t expected_crc = flash_metadata_calc_crc16((const uint8_t *)p_meta, 30U);
    if (expected_crc != p_meta->crc16) {
        return false;
    }
    return true;
}

status_t flash_metadata_find_latest(flash_ring_metadata_t *p_meta, uint32_t *p_active_slot) {
    if (p_meta == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    uint32_t latest_generation = 0U;
    bool found_valid = false;
    bool all_erased = true;
    uint32_t active_slot_idx = 0U;
    flash_ring_metadata_t latest_entry;
    memset(&latest_entry, 0, sizeof(latest_entry));

    for (uint32_t slot = 0U; slot < FLASH_METADATA_ENTRIES_PER_PAGE; slot++) {
        uint32_t addr = FLASH_METADATA_PAGE_BASE_ADDR + (slot * FLASH_METADATA_ENTRY_SIZE);
        flash_ring_metadata_t entry;
        status_t st = flash_storage_read_bytes(addr, (uint8_t *)&entry, FLASH_METADATA_ENTRY_SIZE);
        if (st != STATUS_OK) {
            continue;
        }

        if (is_slot_erased(&entry)) {
            continue;
        }

        /* Non-erased slot encountered */
        all_erased = false;

        if (flash_metadata_is_consistent(&entry)) {
            if (!found_valid || entry.generation > latest_generation) {
                latest_generation = entry.generation;
                latest_entry = entry;
                active_slot_idx = slot;
                found_valid = true;
            }
        }
    }

    if (found_valid) {
        memcpy(p_meta, &latest_entry, sizeof(flash_ring_metadata_t));
        if (p_active_slot != NULL) {
            *p_active_slot = active_slot_idx;
        }
        return STATUS_OK;
    }

    if (all_erased) {
        return STATUS_ERR_NOT_FOUND;
    }

    return STATUS_ERR_INTEGRITY;
}

status_t flash_metadata_commit(flash_ring_metadata_t *p_meta) {
    if (p_meta == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    /* Synchronize active_slot and generation if not initialized in RAM */
    if (s_metadata_active_slot >= FLASH_METADATA_ENTRIES_PER_PAGE) {
        flash_ring_metadata_t latest;
        uint32_t slot = 0U;
        status_t st = flash_metadata_find_latest(&latest, &slot);
        if (st == STATUS_OK) {
            s_metadata_active_slot = slot;
            s_metadata_generation = latest.generation;
            s_metadata_epoch = latest.epoch;
        } else if (st == STATUS_ERR_NOT_FOUND) {
            s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
            s_metadata_generation = 0U;
            s_metadata_epoch = p_meta->epoch;
        } else {
            /* Corrupted Page 127: erase and reset */
            status_t erase_st = flash_storage_erase_page(FLASH_RING_METADATA_PAGE);
            if (erase_st != STATUS_OK) {
                return erase_st;
            }
            s_metadata_active_slot = FLASH_METADATA_ENTRIES_PER_PAGE;
            s_metadata_generation = 0U;
            s_metadata_epoch = p_meta->epoch + 1U;
        }
    }

    uint32_t target_slot;
    if (s_metadata_active_slot >= FLASH_METADATA_ENTRIES_PER_PAGE) {
        target_slot = 0U;
    } else {
        target_slot = s_metadata_active_slot + 1U;
    }

    /* Rollover if all 64 slots consumed */
    if (target_slot >= FLASH_METADATA_ENTRIES_PER_PAGE) {
        uint32_t saved_gen = s_metadata_generation;
        uint32_t saved_epoch = s_metadata_epoch;
        status_t erase_st = flash_storage_erase_page(FLASH_RING_METADATA_PAGE);
        if (erase_st != STATUS_OK) {
            return erase_st;
        }
        target_slot = 0U;
        s_metadata_generation = saved_gen;
        s_metadata_epoch = saved_epoch + 1U;
        s_metadata_active_slot = 0U;
    }

    /* Prepare entry */
    p_meta->magic = FLASH_METADATA_MAGIC;
    s_metadata_generation++;
    p_meta->generation = s_metadata_generation;
    p_meta->epoch = s_metadata_epoch;
    p_meta->reserved1 = 0U;
    p_meta->reserved2 = 0U;
    p_meta->crc16 = flash_metadata_calc_crc16((const uint8_t *)p_meta, 30U);

    uint32_t slot_addr = FLASH_METADATA_PAGE_BASE_ADDR + (target_slot * FLASH_METADATA_ENTRY_SIZE);

    /* Program 32 bytes (4 double-words) within unlock/lock window */
    status_t st = flash_storage_unlock();
    if (st != STATUS_OK) {
        return st;
    }

    const uint64_t *p_dwords = (const uint64_t *)(const void *)p_meta;
    for (uint32_t i = 0U; i < (FLASH_METADATA_ENTRY_SIZE / FLASH_STORAGE_PROG_UNIT_BYTES); i++) {
        uint32_t dword_addr = slot_addr + (i * FLASH_STORAGE_PROG_UNIT_BYTES);
        st = flash_storage_write_dword(dword_addr, p_dwords[i]);
        if (st != STATUS_OK) {
            (void)flash_storage_lock();
            return st;
        }
    }

    (void)flash_storage_lock();

    /* Verify written entry via readback */
    flash_ring_metadata_t readback;
    st = flash_storage_read_bytes(slot_addr, (uint8_t *)&readback, FLASH_METADATA_ENTRY_SIZE);
    if (st != STATUS_OK) {
        return st;
    }
    if (memcmp(&readback, p_meta, FLASH_METADATA_ENTRY_SIZE) != 0) {
        return STATUS_ERR_DATA_CORRUPT;
    }

    /* Update static RAM cache */
    s_metadata_active_slot = target_slot;
    memcpy(&s_cached_metadata, p_meta, sizeof(flash_ring_metadata_t));
    s_cached_metadata_valid = true;

    return STATUS_OK;
}
