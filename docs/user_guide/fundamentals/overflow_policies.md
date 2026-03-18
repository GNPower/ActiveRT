# Overflow Policies

When `activert_event_pool_alloc` is called and every slot is occupied, the
pool is *exhausted*. The **overflow policy** controls what happens next.
You choose a policy per-pool at creation time:

```c
ACTIVERT_EVENT_POOL_DEFINE(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
//                                                  ^^^^^^^^^^^^^^^^^^^^^^^^^^
//                                                  policy chosen here
```

---

## `ACTIVERT_POOL_OVERFLOW_DROP`

`activert_event_pool_alloc` returns `NULL`. No event is allocated. If
the caller checks for `NULL` and returns without posting, the event is
silently discarded.

```c
my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
if (evt == NULL) {
    /* pool full - drop this event */
    return;
}
```

**When to use:** The best default for most production applications.
Dropping events under overload is a graceful degradation: the system stays
running and handles events when capacity recovers. This is suitable for periodic
sensor readings or status updates where the latest value is what matters.

**Statistics:** `pool->stats.allocs_failed` increments on every drop.
The health system raises a `WARNING` when the failure rate exceeds 5% and
`CRITICAL` above 50%.

---

## `ACTIVERT_POOL_OVERFLOW_ASSERT`

`activert_event_pool_alloc` calls `configASSERT(0)`, which traps the
system according to your `FreeRTOSConfig.h` assertion handler (typically
prints a diagnostic and halts).

```c
ACTIVERT_EVENT_POOL_DEFINE(cmd_pool, cmd_event_t, 4, ACTIVERT_POOL_OVERFLOW_ASSERT);
```

**When to use:** During development, or for pools where exhaustion
represents a design error that should never occur in a correctly-sized
system. This is suitable for commands that must never be dropped (e.g. safety-critical
actuator commands).

**Note:** Do not use `ASSERT` in production if pool exhaustion is
a plausible outcome of normal operation (eg. an ISR that can burst events).

---

## `ACTIVERT_POOL_OVERFLOW_DYNAMIC`

Requires `ACTIVERT_ENABLE_DYNAMIC_ALLOCATION 1` in your config.

When the pool is exhausted, `pvPortMalloc(pool->event_size)` is called
to allocate a heap event. The event's `pool` pointer is set to `NULL` to
mark it as heap-allocated. When the dispatch handler returns, ActiveRT
detects the `NULL` pool pointer and calls `vPortFree` instead of returning
the slot to the bitmap.

```c
/* In activert_config.h or before including activert.h */
#define ACTIVERT_ENABLE_DYNAMIC_ALLOCATION 1

ACTIVERT_EVENT_POOL_DEFINE(burst_pool, burst_event_t, 8,
                           ACTIVERT_POOL_OVERFLOW_DYNAMIC);
```

**When to use:** When the pool is normally sufficient but occasional
short bursts must not be dropped and heap fragmentation is acceptable.
Examples: a bursty USB receive path, or a logging pool where log entries
must never be lost.

**Cost:** Each heap-allocated overflow event incurs a `pvPortMalloc` +
`vPortFree` pair. On heap_4 this is O(free_list_size). If your system
relies on this path frequently, your pool is undersized and you should
increase the static capacity instead.

**Safety note:** If `pvPortMalloc` itself fails (heap exhausted),
the alloc returns `NULL`. The pool's `allocs_failed` counter still
increments. Always check for `NULL` even with the `DYNAMIC` policy.

---

## Choosing a Policy

| Scenario | Recommended policy |
| --- | --- |
| Default / general-purpose | `DROP` |
| Must never lose an event (safety-critical) | `ASSERT` |
| Development / debugging (catch sizing errors) | `ASSERT` |
| Occasional bursts OK to heap-allocate | `DYNAMIC` |
| Production with strict no-heap requirement | `DROP` or `ASSERT` |
