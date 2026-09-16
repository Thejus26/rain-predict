/**
 * @file    flash_storage.h
 * @brief   STM32WLE5 on-chip Flash memory driver header for offline telemetry storage.
 * @details Provides page erase, 64-bit double-word programming, and memory-mapped read access
 *          for dedicated NVM storage partition (Pages 120-127, 16 KB total).
 *          Strict zero-dynamic-memory allocation and defensive out-of-bounds guards.
 */

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Hardware Geometry & Partition Constants                                    */
/* ========================================================================== */

/** @brief Physical Flash page size in bytes (2 KB) */
#define FLASH_STORAGE_PAGE_SIZE             2048U

/** @brief Starting physical page number allocated for NVM storage (Page 120) */
#define FLASH_STORAGE_START_PAGE            120U

/** @brief Ending physical page number allocated for NVM storage (Page 127) */
#define FLASH_STORAGE_END_PAGE              127U

/** @brief Total number of 2 KB pages in dedicated NVM partition (8 Pages = 16 KB) */
#define FLASH_STORAGE_TOTAL_PAGES           8U

/** @brief Base address of dedicated NVM storage partition */
#define FLASH_STORAGE_BASE_ADDR             0x0803C000U

/** @brief End address (inclusive) of dedicated NVM storage partition */
#define FLASH_STORAGE_END_ADDR              0x0803FFFFU

/** @brief Total capacity of dedicated NVM storage partition in bytes (16 KB) */
#define FLASH_STORAGE_TOTAL_CAPACITY_BYTES  (FLASH_STORAGE_TOTAL_PAGES * FLASH_STORAGE_PAGE_SIZE)

/** @brief Programming unit size in bytes (STM32WLE5 requires 64-bit double-word) */
#define FLASH_STORAGE_PROG_UNIT_BYTES       8U

/** @brief Maximum timeout for Flash operations in milliseconds */
#define FLASH_STORAGE_TIMEOUT_MS            50U

/** @brief Unprogrammed Flash erased byte value */
#define FLASH_STORAGE_ERASED_BYTE           0xFFU

/** @brief Unprogrammed Flash erased 64-bit double-word value */
#define FLASH_STORAGE_ERASED_DWORD          0xFFFFFFFFFFFFFFFFULL

/* ========================================================================== */
/* Function Prototypes                                                        */
/* ========================================================================== */

/**
 * @brief  Initializes the Flash storage driver and clears hardware status flags.
 * @return STATUS_OK on success, or error status code.
 */
status_t flash_storage_init(void);

/**
 * @brief  Unlocks the STM32WLE5 Flash control registers for erase/write access.
 * @return STATUS_OK on success, or STATUS_ERROR_HARDWARE on key failure.
 */
status_t flash_storage_unlock(void);

/**
 * @brief  Locks the Flash control registers to protect against unintended writes.
 * @return STATUS_OK on success.
 */
status_t flash_storage_lock(void);

/**
 * @brief  Erases a single 2 KB Flash page within the dedicated NVM partition.
 * @param[in] page_num Physical page index (must be between 120 and 127 inclusive).
 * @return STATUS_OK on success, STATUS_ERROR_INVALID_PARAM if page is outside NVM partition,
 *         STATUS_ERROR_HARDWARE if flash is locked/unresponsive, or STATUS_ERROR_TIMEOUT.
 */
status_t flash_storage_erase_page(uint32_t page_num);

/**
 * @brief  Erases all 8 pages in the dedicated NVM storage partition (16 KB total).
 * @return STATUS_OK on success, or error status code.
 */
status_t flash_storage_erase_all_nvm_pages(void);

/**
 * @brief  Programs a single 64-bit (8-byte) double-word to a 64-bit aligned Flash address.
 * @param[in] address 64-bit aligned Flash address (0x0803C000 to 0x0803FFF8).
 * @param[in] data    64-bit data word to write.
 * @return STATUS_OK on success, STATUS_ERROR_OUT_OF_BOUNDS if address is outside NVM partition,
 *         STATUS_ERROR_INVALID_PARAM if address is unaligned, STATUS_ERROR_HARDWARE if locked.
 */
status_t flash_storage_write_dword(uint32_t address, uint64_t data);

/**
 * @brief  Programs an arbitrary byte buffer into Flash with automatic 64-bit alignment padding.
 * @param[in] address Flash start address (must be within NVM storage partition).
 * @param[in] data    Pointer to source byte buffer.
 * @param[in] length  Number of bytes to write.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if data is NULL,
 *         STATUS_ERROR_OUT_OF_BOUNDS if write exceeds partition, or write error.
 */
status_t flash_storage_write_bytes(uint32_t address, const uint8_t *data, size_t length);

/**
 * @brief  Reads an arbitrary byte buffer directly from Flash memory.
 * @param[in]  address Flash source address (must be within NVM storage partition).
 * @param[out] buffer  Pointer to destination buffer to populate.
 * @param[in]  length  Number of bytes to read.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if buffer is NULL,
 *         or STATUS_ERROR_OUT_OF_BOUNDS if read exceeds partition.
 */
status_t flash_storage_read_bytes(uint32_t address, uint8_t *buffer, size_t length);

/**
 * @brief  Checks whether an entire 2 KB Flash page is in the erased (all 0xFF) state.
 * @param[in] page_num Physical page index (120 to 127).
 * @return true if page is completely erased (all 0xFF), false otherwise.
 */
bool flash_storage_is_page_erased(uint32_t page_num);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_STORAGE_H */
