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
