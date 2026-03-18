# Pool Concepts and Sizing

## Why Not `pvPortMalloc`?

FreeRTOS provides `pvPortMalloc` for dynamic allocation, but it has two
properties that make it unsuitable for high-rate event passing:

- **Non-deterministic latency.** `pvPortMalloc` with heap_4 scans a free
  list. Allocation time grows with heap fragmentation.
- **Fragmentation.** Repeated alloc/free cycles of varying sizes produce
  unusable gaps that cannot be reclaimed without heap managers to coalesce
  free blocks (heap_4/heap_5).

An event pool avoids both problems by allocating from a **fixed-size,
pre-allocated array**. Every slot is the same size, allocation is an O(1)
bitmap scan, and fragmentation is impossible.

---

## Structure of an Event Pool

```text
Pool (capacity = 8, event_size = sizeof(my_event_t)):

Static storage:
  ┌──────────────────────────────────────────────────┐
  │  uint8_t  storage[8 * sizeof(my_event_t)]        │  <- event array
  │  uint8_t  bitmap[1]   (8 bits, 1 per slot)       │  <- allocation map
  │  StaticSemaphore_t mutex_cb                      │  <- FreeRTOS mutex
  │  activert_event_pool_t control block             │  <- pool metadata
  └──────────────────────────────────────────────────┘

bitmap: 0b00101001   →  slots 0, 3, 5 allocated; 1, 2, 4, 6, 7 free
```

`activert_event_pool_alloc` finds the first zero bit, sets it, and returns
a pointer to that slot. `activert_event_pool_free` clears the bit. Both
operations take O(1) time, bounded by the number of bytes in the bitmap
(at most `pool_size / 8` bytes to scan).

The pool also holds a FreeRTOS mutex that makes alloc and free thread-safe
from any task context, as well as `_from_isr` variants which use critical
sections to ensure interrupt-safe alloc and free. There are also optional
statistics counters provided to track pool usage which can be enabled via
`ACTIVERT_ENABLE_STATS`.

---

## One Pool Per Event Type

A pool holds events of exactly one type. The pool is created with a fixed
`event_size` equal to `sizeof(your_event_type)`. If you have events with
different payloads, create a separate pool for each type:

```c
/* Small control events */
typedef struct { activert_event_t base; uint8_t cmd; } ctrl_event_t;
ACTIVERT_EVENT_POOL_DEFINE(ctrl_pool, ctrl_event_t, 4, ACTIVERT_POOL_OVERFLOW_ASSERT);

/* Large data events */
typedef struct { activert_event_t base; uint8_t data[64]; } data_event_t;
ACTIVERT_EVENT_POOL_DEFINE(data_pool, data_event_t, 16, ACTIVERT_POOL_OVERFLOW_DROP);
```

This prevents large events from exhausting slots needed by small ones, and
keeps each pool's memory footprint predictable.

---

## Sizing a Pool

The capacity you need depends on the **maximum number of events that can
be simultaneously in-flight** (allocated but not yet freed). An event is
in-flight from the moment `activert_event_pool_alloc` returns until the
dispatch handler returns (at which point ActiveRT frees it automatically).

A practical sizing approach:

1. Count the number of producers (tasks + ISRs) that can alloc from this
   pool simultaneously.
2. Count the maximum queue depth of all AOs that receive this event type.
3. Add a small margin (x1.5 to x2) to ensure the pool is larger than both
   the number of produceres and maximum queue depth.

```text
Example: one ISR that fires at 10 Hz, posting to an AO with queue depth 8.
  Max in-flight = 8 (all queued) + 1 (being dispatched) = 9 → round up to 16.
```

When in doubt, start with a capacity equal to the longest receiving queue
depth plus two and monitor the `peak_allocated` statistic in development
to verify.

---

## Static Memory Footprint

The total RAM used by a pool:

```text
sizeof(activert_event_pool_t)           <- control block (~40 bytes)
+ event_size * capacity                 <- event storage
+ ceil(capacity / 8)                    <- bitmap
+ sizeof(StaticSemaphore_t)             <- FreeRTOS mutex storage
```

Example: 16 slots of a 24-byte event type on a 32-bit target:

```text
40 + (24 × 16) + 2 + ~80  =  ~506 bytes total
```
