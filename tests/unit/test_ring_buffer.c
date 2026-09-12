/**
 * @file    test_ring_buffer.c
 * @brief   Comprehensive unit tests for generic static circular ring buffer.
 * @details Validates FIFO ordering, drop/overwrite overflow policies, pointer wrap-around,
 *          non-destructive peeking, and defensive parameter validation.
 */

#include "unity.h"
#include "ring_buffer.h"
#include <string.h>

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure;
    uint8_t  sensor_id;
} test_record_t;

static uint8_t s_storage_bytes[64];
static test_record_t s_storage_records[8];
static ring_buffer_t s_rb;

void setUp(void) {
    (void)memset(s_storage_bytes, 0, sizeof(s_storage_bytes));
    (void)memset(s_storage_records, 0, sizeof(s_storage_records));
    (void)memset(&s_rb, 0, sizeof(s_rb));
}

void tearDown(void) {
}

/**
 * @brief Test initialization parameter validation and NULL safety.
 */
static void test_ring_buffer_init_validation(void) {
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR,
                      ring_buffer_init(NULL, s_storage_bytes, 8, sizeof(uint8_t), RING_BUFFER_DROP_NEW));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR,
                      ring_buffer_init(&s_rb, NULL, 8, sizeof(uint8_t), RING_BUFFER_DROP_NEW));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM,
                      ring_buffer_init(&s_rb, s_storage_bytes, 0, sizeof(uint8_t), RING_BUFFER_DROP_NEW));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM,
                      ring_buffer_init(&s_rb, s_storage_bytes, 8, 0, RING_BUFFER_DROP_NEW));
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM,
                      ring_buffer_init(&s_rb, s_storage_bytes, 8, sizeof(uint8_t), (ring_buffer_mode_t)99));
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 8, sizeof(uint8_t), RING_BUFFER_DROP_NEW));
    TEST_ASSERT_EQUAL_UINT16(0, ring_buffer_get_count(&s_rb));
    TEST_ASSERT_EQUAL_UINT16(8, ring_buffer_get_capacity(&s_rb));
    TEST_ASSERT_TRUE(ring_buffer_is_empty(&s_rb));
    TEST_ASSERT_FALSE(ring_buffer_is_full(&s_rb));
}

/**
 * @brief Test sequential FIFO push and pop operations with primitive types.
 */
static void test_ring_buffer_fifo_ordering_primitives(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 8, sizeof(uint32_t), RING_BUFFER_DROP_NEW));

    const uint32_t in_vals[] = { 100, 200, 300, 400 };
    for (size_t i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &in_vals[i]));
    }
    TEST_ASSERT_EQUAL_UINT16(4, ring_buffer_get_count(&s_rb));
    TEST_ASSERT_FALSE(ring_buffer_is_empty(&s_rb));
    TEST_ASSERT_FALSE(ring_buffer_is_full(&s_rb));

    for (size_t i = 0; i < 4; i++) {
        uint32_t out_val = 0;
        TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out_val));
        TEST_ASSERT_EQUAL_UINT32(in_vals[i], out_val);
    }
    TEST_ASSERT_TRUE(ring_buffer_is_empty(&s_rb));
    TEST_ASSERT_EQUAL_UINT16(0, ring_buffer_get_count(&s_rb));

    /* Pop on empty must return STATUS_ERR_BUSY */
    uint32_t dummy = 0;
    TEST_ASSERT_EQUAL(STATUS_ERR_BUSY, ring_buffer_pop(&s_rb, &dummy));
}

/**
 * @brief Test FIFO push and pop operations with multi-byte structs.
 */
static void test_ring_buffer_fifo_ordering_structs(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_records, 4, sizeof(test_record_t), RING_BUFFER_DROP_NEW));

    test_record_t r1 = { .timestamp = 1000, .temperature = 22.5f, .pressure = 845.0f, .sensor_id = 1 };
    test_record_t r2 = { .timestamp = 2000, .temperature = 23.0f, .pressure = 844.5f, .sensor_id = 2 };

    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &r1));
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &r2));

    test_record_t out_r;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out_r));
    TEST_ASSERT_EQUAL_UINT32(1000, out_r.timestamp);
    TEST_ASSERT_EQUAL_FLOAT(22.5f, out_r.temperature);
    TEST_ASSERT_EQUAL_FLOAT(845.0f, out_r.pressure);
    TEST_ASSERT_EQUAL_UINT8(1, out_r.sensor_id);

    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out_r));
    TEST_ASSERT_EQUAL_UINT32(2000, out_r.timestamp);
    TEST_ASSERT_EQUAL_FLOAT(23.0f, out_r.temperature);
    TEST_ASSERT_EQUAL_FLOAT(844.5f, out_r.pressure);
    TEST_ASSERT_EQUAL_UINT8(2, out_r.sensor_id);
}

/**
 * @brief Test DROP_NEW full-buffer policy rejection.
 */
static void test_ring_buffer_drop_new_full(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 3, sizeof(uint8_t), RING_BUFFER_DROP_NEW));

    const uint8_t b1 = 1, b2 = 2, b3 = 3, b4 = 4;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b1));
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b2));
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b3));
    TEST_ASSERT_TRUE(ring_buffer_is_full(&s_rb));

    /* 4th push must be rejected with STATUS_ERR_BUSY */
    TEST_ASSERT_EQUAL(STATUS_ERR_BUSY, ring_buffer_push(&s_rb, &b4));
    TEST_ASSERT_EQUAL_UINT16(3, ring_buffer_get_count(&s_rb));

    uint8_t out = 0;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out));
    TEST_ASSERT_EQUAL_UINT8(1, out);
}

/**
 * @brief Test OVERWRITE_OLD full-buffer policy eviction.
 */
static void test_ring_buffer_overwrite_old_full(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 3, sizeof(uint8_t), RING_BUFFER_OVERWRITE_OLD));

    const uint8_t b1 = 10, b2 = 20, b3 = 30, b4 = 40;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b1));
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b2));
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b3));
    TEST_ASSERT_TRUE(ring_buffer_is_full(&s_rb));

    /* Pushing 4th item evicts oldest (10) and advances tail */
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &b4));
    TEST_ASSERT_EQUAL_UINT16(3, ring_buffer_get_count(&s_rb));
    TEST_ASSERT_TRUE(ring_buffer_is_full(&s_rb));

    uint8_t out = 0;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out));
    TEST_ASSERT_EQUAL_UINT8(20, out);
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out));
    TEST_ASSERT_EQUAL_UINT8(30, out);
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &out));
    TEST_ASSERT_EQUAL_UINT8(40, out);
    TEST_ASSERT_TRUE(ring_buffer_is_empty(&s_rb));
}

/**
 * @brief Test continuous push-pop cycling across pointer boundaries.
 */
static void test_ring_buffer_wrap_around_cycling(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 4, sizeof(uint16_t), RING_BUFFER_DROP_NEW));

    for (uint16_t i = 0; i < 500; i++) {
        uint16_t val_in = i;
        uint16_t val_out = 0;
        TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &val_in));
        TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_pop(&s_rb, &val_out));
        TEST_ASSERT_EQUAL_UINT16(i, val_out);
    }
}

/**
 * @brief Test non-destructive indexed peeking.
 */
static void test_ring_buffer_indexed_peeking(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 5, sizeof(uint8_t), RING_BUFFER_DROP_NEW));

    const uint8_t vals[] = { 11, 22, 33 };
    for (size_t i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &vals[i]));
    }

    uint8_t peek_val = 0;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_peek(&s_rb, &peek_val));
    TEST_ASSERT_EQUAL_UINT8(11, peek_val);

    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_peek_at(&s_rb, 0, &peek_val));
    TEST_ASSERT_EQUAL_UINT8(11, peek_val);

    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_peek_at(&s_rb, 1, &peek_val));
    TEST_ASSERT_EQUAL_UINT8(22, peek_val);

    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_peek_at(&s_rb, 2, &peek_val));
    TEST_ASSERT_EQUAL_UINT8(33, peek_val);

    /* Index out of bounds */
    TEST_ASSERT_EQUAL(STATUS_ERR_INVALID_PARAM, ring_buffer_peek_at(&s_rb, 3, &peek_val));
    TEST_ASSERT_EQUAL_UINT16(3, ring_buffer_get_count(&s_rb));
}

/**
 * @brief Test clear operation and null safety for utility getters.
 */
static void test_ring_buffer_clear_and_null_checks(void) {
    TEST_ASSERT_EQUAL(STATUS_OK,
                      ring_buffer_init(&s_rb, s_storage_bytes, 4, sizeof(uint8_t), RING_BUFFER_DROP_NEW));

    uint8_t val = 42;
    TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &val));
    TEST_ASSERT_EQUAL_UINT16(1, ring_buffer_get_count(&s_rb));

    ring_buffer_clear(&s_rb);
    TEST_ASSERT_TRUE(ring_buffer_is_empty(&s_rb));
    TEST_ASSERT_EQUAL_UINT16(0, ring_buffer_get_count(&s_rb));

    /* NULL check robust handling */
    TEST_ASSERT_TRUE(ring_buffer_is_empty(NULL));
    TEST_ASSERT_FALSE(ring_buffer_is_full(NULL));
    TEST_ASSERT_EQUAL_UINT16(0, ring_buffer_get_count(NULL));
    TEST_ASSERT_EQUAL_UINT16(0, ring_buffer_get_capacity(NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_push(NULL, &val));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_pop(NULL, &val));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_peek(NULL, &val));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_peek_at(NULL, 0, &val));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_push(&s_rb, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_pop(&s_rb, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_peek(&s_rb, NULL));
    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_peek_at(&s_rb, 0, NULL));
    ring_buffer_clear(NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ring_buffer_init_validation);
    RUN_TEST(test_ring_buffer_fifo_ordering_primitives);
    RUN_TEST(test_ring_buffer_fifo_ordering_structs);
    RUN_TEST(test_ring_buffer_drop_new_full);
    RUN_TEST(test_ring_buffer_overwrite_old_full);
    RUN_TEST(test_ring_buffer_wrap_around_cycling);
    RUN_TEST(test_ring_buffer_indexed_peeking);
    RUN_TEST(test_ring_buffer_clear_and_null_checks);
    return UNITY_END();
}
