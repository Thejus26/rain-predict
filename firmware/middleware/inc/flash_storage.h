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
/* Circular Ring Buffer Geometry & Magic Headers                              */
/* ========================================================================== */

/** @brief Number of data pages dedicated to circular telemetry records (Pages 120..126) */
#define FLASH_RING_DATA_PAGES               7U

/** @brief Page index reserved for Station Configuration and Metadata Header (Page 127) */
#define FLASH_RING_METADATA_PAGE            127U

/** @brief Size of an individual offline telemetry record slot in bytes (2 double-words) */
#define FLASH_RING_RECORD_SIZE              16U

/** @brief Number of 16-byte records per 2 KB page (2048 / 16 = 128) */
#define FLASH_RING_RECORDS_PER_PAGE         128U

/** @brief Total record capacity across all 7 data pages (7 * 128 = 896 records) */
#define FLASH_RING_TOTAL_CAPACITY           (FLASH_RING_DATA_PAGES * FLASH_RING_RECORDS_PER_PAGE)

/** @brief 16-bit Magic Status: Unwritten / Erased slot */
#define FLASH_RING_MAGIC_ERASED             0xFFFFU

/** @brief 16-bit Magic Status: Valid pending telemetry record */
#define FLASH_RING_MAGIC_VALID              0xAA55U

/** @brief 16-bit Magic Status: Transmitted / Acknowledged record (invalidated in-place) */
#define FLASH_RING_MAGIC_TRANSMITTED        0x0000U

/** @brief Packed periodic telemetry payload length in bytes */
#define FLASH_RING_TELEMETRY_PAYLOAD_SIZE   12U

/* ========================================================================== */
/* Type Definitions & Structures                                              */
/* ========================================================================== */

/**
 * @brief  Structure representing a single 16-byte offline telemetry record.
 */
typedef struct {
    uint16_t magic_status;                                /**< Slot status: 0xAA55 (Valid), 0x0000 (Transmitted), 0xFFFF (Erased) */
    uint16_t sequence_id;                                 /**< Monotonically incrementing measurement sequence counter */
    uint8_t  payload[FLASH_RING_TELEMETRY_PAYLOAD_SIZE]; /**< 12-byte packed binary telemetry frame */
} flash_record_t;

/**
 * @brief  Live runtime status of the Flash wear-leveling ring buffer.
 */
typedef struct {
    uint32_t head_index;        /**< Index of next slot to write (0..895) */
    uint32_t tail_index;        /**< Index of oldest un-transmitted record (0..895) */
    uint32_t valid_count;       /**< Total number of pending valid records (0..896) */
    uint16_t next_seq_id;       /**< Next sequence ID to assign */
    bool     is_full;           /**< True if buffer is at maximum capacity (896 records) */
    bool     is_empty;          /**< True if buffer has zero pending valid records */
} flash_ring_state_t;

/* ========================================================================== */
/* Low-Level Flash HAL Driver Prototypes (S5-T3.1)                             */
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

/* ========================================================================== */
/* Wear-Leveling Circular Ring Buffer Prototypes (S5-T3.2)                     */
/* ========================================================================== */

/**
 * @brief  Initializes the Flash wear-leveling circular ring buffer.
 * @details Performs a fast boot-time scan across Pages 120..126 to reconstruct head,
 *          tail, record count, and sequence ID pointers without writing to Flash.
 * @return STATUS_OK on success, or error status code.
 */
status_t flash_ring_init(void);

/**
 * @brief  Pushes a 12-byte packed telemetry payload into the Flash ring buffer.
 * @details Constructs a 16-byte record (0xAA55 magic + sequence_id + payload) and
 *          programs it using two 64-bit double-word writes. Automatically erases new
 *          pages on boundary crossing. If buffer is full, overwrites oldest record.
 * @param[in] payload Pointer to 12-byte packed telemetry buffer.
 * @param[in] seq_id  Monotonic sequence counter for this record.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if payload is NULL,
 *         or hardware write error.
 */
status_t flash_ring_push(const uint8_t *payload, uint16_t seq_id);

/**
 * @brief  Pops (reads and invalidates) the oldest pending telemetry record.
 * @details Reads the record at tail_index, marks its magic word as 0x0000 in Flash,
 *          advances tail_index, and decrements valid_count.
 * @param[out] out_record Pointer to destination flash_record_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if out_record is NULL,
 *         or STATUS_ERROR_EMPTY if no valid records exist.
 */
status_t flash_ring_pop(flash_record_t *out_record);

/**
 * @brief  Peeks at a pending record at a given offset from the tail without popping.
 * @param[in]  offset_from_tail 0 for oldest record, 1 for second oldest, etc.
 * @param[out] out_record       Pointer to destination flash_record_t structure.
 * @return STATUS_OK on success, STATUS_ERROR_NULL_POINTER if out_record is NULL,
 *         or STATUS_ERROR_OUT_OF_BOUNDS if offset >= valid_count.
 */
status_t flash_ring_peek(uint32_t offset_from_tail, flash_record_t *out_record);

/**
 * @brief  Marks a specified number of oldest valid records as transmitted (0x0000).
 * @param[in] count Number of records to invalidate from the current tail.
 * @return STATUS_OK on success, STATUS_ERROR_INVALID_PARAM if count == 0,
 *         or STATUS_ERROR_OUT_OF_BOUNDS if count > valid_count.
 */
status_t flash_ring_mark_transmitted(uint32_t count);

/**
 * @brief  Returns the current number of valid pending offline records.
 * @return Total un-transmitted records (0 to 896).
 */
uint32_t flash_ring_get_count(void);

/**
 * @brief  Returns the maximum record capacity of the ring buffer.
 * @return Fixed capacity (896 records).
 */
uint32_t flash_ring_get_capacity(void);

/**
 * @brief  Checks whether the ring buffer is completely full.
 * @return true if valid_count == 896, false otherwise.
 */
bool flash_ring_is_full(void);

/**
 * @brief  Checks whether the ring buffer has zero valid pending records.
 * @return true if valid_count == 0, false otherwise.
 */
bool flash_ring_is_empty(void);

/**
 * @brief  Retrieves a copy of the live ring buffer state structure.
 * @param[out] out_state Pointer to destination flash_ring_state_t structure.
 * @return STATUS_OK on success, or STATUS_ERROR_NULL_POINTER.
 */
status_t flash_ring_get_state(flash_ring_state_t *out_state);

/**
 * @brief  Returns the current head slot index (0..895).
 * @return Next slot index to write.
 */
uint32_t flash_ring_get_head_index(void);

/**
 * @brief  Returns the current tail slot index (0..895).
 * @return Oldest un-transmitted record slot index.
 */
uint32_t flash_ring_get_tail_index(void);

/**
 * @brief  Erases all 7 data pages (Pages 120..126) and resets ring buffer state to empty.
 * @return STATUS_OK on success, or error status code.
 */
status_t flash_ring_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_STORAGE_H */
