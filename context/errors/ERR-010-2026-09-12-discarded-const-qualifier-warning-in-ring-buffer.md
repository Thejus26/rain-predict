# Error Report: ERR-010 - Discarded const Qualifier in test_ring_buffer.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-010` |
| **Date & Time** | `2026-09-12 09:06:19 +0530` |
| **Commit SHA** | [`1360240`](https://github.com/Thejus26/rain-predict/commit/13602402966521f32b1328e5f97e393430eeb5bd) |
| **Sprint / Task** | Sprint 2 (S2-T1.1 Generic Ring Buffer Tests) |
| **Severity** | Medium (Compiler Diagnostic under Strict C99) |
| **Impacted Files** | [`tests/unit/test_ring_buffer.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_ring_buffer.c) |

---

## 1. Description & Symptoms

Building `test_ring_buffer` failed with compiler diagnostic `-Wdiscarded-qualifiers`:
```
tests/unit/test_ring_buffer.c:226:49: error: passing argument 2 of 'ring_buffer_pop' discards 'const' qualifier from pointer target type [-Werror=discarded-qualifiers]
```

---

## 2. Root Cause Analysis

In `test_ring_buffer_clear_and_null_checks()`, a local variable was declared as `const uint8_t val = 42;`. While passing `&val` to `ring_buffer_push(..., const void *p_item)` is valid, passing `&val` to output parameters like `ring_buffer_pop(..., void *p_item)` and `ring_buffer_peek(..., void *p_item)` discards the `const` qualifier because the function writes output into the destination pointer.

---

## 3. Resolution & Code Changes

Changed `val` to non-const `uint8_t val = 42;` and expanded NULL pointer boundary checks:

```diff
-    const uint8_t val = 42;
+    uint8_t val = 42;
     TEST_ASSERT_EQUAL(STATUS_OK, ring_buffer_push(&s_rb, &val));
...
+    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_push(&s_rb, NULL));
+    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_pop(&s_rb, NULL));
+    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_peek(&s_rb, NULL));
+    TEST_ASSERT_EQUAL(STATUS_ERR_NULL_PTR, ring_buffer_peek_at(&s_rb, 0, NULL));
```

---

## 4. Verification & Prevention Guidelines

- Ensure destination output pointers passed into getter/pop functions are non-const buffers.
- Enforce strict `const void *` on input data parameters and `void *` on output buffer parameters across generic middleware drivers.
