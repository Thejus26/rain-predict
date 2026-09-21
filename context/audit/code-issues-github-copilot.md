Potential performance issues are concentrated in flash-backed storage and repeated floating-point validation; the small queue and scoring logic are unlikely to be bottlenecks.

## High-impact issues

1. **Flash ring initialization performs a full scan on every initialization**

`flash_ring_init()` reads all 896 record slots, with each slot requiring a flash read and `memcpy`. In one wrap-around case, it performs a second scan and reads two adjacent records per iteration.

- File: [`firmware/middleware/src/flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L441-L526)
- Impact: noticeable boot/resume latency and unnecessary flash bus activity.
- Improvement:
  - Store a compact persistent ring metadata header containing head, tail, count, and sequence number.
  - Use journaled metadata with a checksum/version so recovery only scans after detecting invalid metadata.
  - Cache the recovered state in RAM and avoid calling initialization implicitly from simple getters.

2. **`flash_ring_peek()` is O(N) for every offset**

The function walks from the tail and scans all slots until it finds the requested valid record. Repeatedly peeking at offsets near the end can therefore become O(N²).

- File: [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L671-L703)
- Impact: slow playback or telemetry replay when many records are queued.
- Improvement:
  - Maintain contiguous valid-record metadata where possible.
  - Return the physical slot directly from an iterator.
  - Cache the last peek position when consumers access records sequentially.

3. **Each transmitted record can cause a separate flash unlock/lock cycle**

`flash_ring_mark_transmitted()` calls `flash_storage_write_bytes()` once per record. On hardware, that function unlocks and locks flash for every record, and each write may also perform a read-modify-write operation.

- File: [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L705-L741)
- Related code: [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L316-L366)
- Impact: extra HAL calls, longer critical sections, and increased latency during batch acknowledgements.
- Improvement:
  - Unlock once, mark all records, then lock once.
  - Add a specialized `flash_ring_mark_transmitted_batch()` function.
  - Store transmission state in a compact bitmap or metadata area if that fits the reliability requirements.

4. **Page-erased checks scan an entire 2 KB page**

When pushing at a page boundary, `flash_storage_is_page_erased()` scans the complete page before deciding whether to erase it.

- File: [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L179-L191)
- Called from: [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L541-L559)
- Impact: latency spikes at page transitions, especially if flash reads are slow.
- Improvement:
  - Track page state in RAM and persist it alongside ring metadata.
  - Check only a page header or first unused slot rather than scanning the whole page.
  - Avoid erasing based solely on the page boundary; use explicit page lifecycle metadata.

## Medium-impact issues

5. **Modulo operations are used in frequently executed ring-buffer paths**

The code uses `%` for head/tail advancement in the generic ring buffer and flash ring. On small embedded CPUs, modulo by a runtime variable may compile into a division routine rather than a cheap mask.

- [`ring_buffer.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/ring_buffer.c#L47-L64)
- [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L593-L605)
- Improvement:
  - Use increment-and-compare:
    `index++; if (index == capacity) index = 0;`
  - If capacities are powers of two, use masking.
  - Measure first; this is probably lower impact than flash operations.

6. **Floating-point and NaN/ infinity checks may be expensive**

The trend detector repeatedly calls `isnan()` and `isinf()` for several fields and performs floating-point divisions and scaling.

- File: [`trend_detector.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/app/src/trend_detector.c#L15-L128)
- The composite algorithm also uses multiple floating-point operations: [`rain_algo.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/app/src/rain_algo.c#L185-L285)
- Impact: potentially significant CPU time if the build uses software floating-point or if evaluation runs frequently.
- Improvement:
  - Validate sensor values at ingestion rather than revalidating the same fields in every algorithm.
  - Store validity flags with each sample.
  - Use fixed-point or scaled integers for pressure, humidity, temperature, and lux where precision permits.
  - Avoid extrapolation divisions in the hot path by using precomputed scale factors.

7. **The same derived values are recomputed or passed through several layers**

`rain_algo_evaluate()` computes gradients, then repeatedly derives the previous lux value as `p_curr->lux - grads.delta_lux_30m` for classification and scoring.

- File: [`rain_algo.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/app/src/rain_algo.c#L221-L243)
- Impact: small individually, but avoidable in a periodic low-power firmware loop.
- Improvement:
  - Add `lux_30m_ago` directly to `multi_gradient_t`.
  - Compute all derived values once and pass a shared feature structure to the scoring functions.

8. **Flash write paths do multiple small copies and temporary buffer operations**

Unaligned writes use a stack buffer, `memset`, one or more `memcpy` calls, and another `memcpy` into a `uint64_t`.

- File: [`flash_storage.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/flash_storage.c#L316-L366)
- Impact: moderate CPU overhead and stack use, particularly for frequent small writes.
- Improvement:
  - Align record fields and writes to the flash programming unit.
  - Use a dedicated two-byte invalidation routine for the magic field.
  - Batch records or use fixed-size aligned record writes.

## Low-impact issues

9. **The LoRaWAN queue linearly scans all slots**

Queue selection, free-slot lookup, and periodic deduplication scan the entire queue.

- File: [`lorawan_tx_queue.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/lorawan_tx_queue.c#L29-L57)
- Impact: currently negligible because the capacity is only eight entries.
- Improvement: only needed if the queue grows substantially; otherwise the current implementation is simpler and appropriate.

10. **Full-structure copies in queue inspection**

`lorawan_tx_queue_peek()` copies the entire queue item, including callback and context fields.

- [`lorawan_tx_queue.c`](https://github.com/Thejus26/rain-predict/blob/b24ececa5f448e4e8e649609c2c1f3337fed2c6b/firmware/middleware/src/lorawan_tx_queue.c#L223-L238)
- Impact: minor at the current queue size.
- Improvement: expose a read-only pointer or copy only the fields required by the caller.

## Overall assessment

The most important optimization target is **flash access behavior**, not the rain scoring formulas:

1. Persist and cache ring metadata.
2. Batch flash unlock/write/lock operations.
3. Avoid full-page scans and repeated full-ring scans.
4. Move sensor validation to the acquisition boundary.
5. Profile floating-point execution before converting the algorithm to fixed-point.

The queue and classification code are already bounded by very small fixed sizes, so optimizing their linear scans is unlikely to produce a measurable benefit unless their capacities or call frequencies increase.