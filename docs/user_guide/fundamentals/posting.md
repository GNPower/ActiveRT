# Posting to Active Objects

## The Allocate → Fill → Post Sequence

Posting an event is always a three-step operation:

```c
/* 1. Allocate from the pool */
my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
if (evt == NULL) {
    return;   /* pool exhausted, handled by the pool's overflow policy */
}

/* 2. Fill the payload */
evt->base.sig = MY_SIG;
evt->value    = 42;

/* 3. Post to the Active Object */
activert_active_post(my_ao, &evt->base);
/* Do NOT touch evt after this point, the AO now owns it */
```

**Ownership rule:** once you call `activert_active_post`, the event belongs
to ActiveRT. It will be freed back to the pool automatically after the
dispatch handler returns. Never read or write the event after posting.

---

## ISR-Safe Posting

Use the `_from_isr` variants when posting from an interrupt handler:

```c
void UART_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;

    uart_event_t *evt = (uart_event_t *)
        activert_event_pool_alloc_from_isr(uart_pool, &woken);

    if (evt != NULL) {
        evt->base.sig = UART_RX_SIG;
        evt->byte     = UART->DR;
        activert_active_post_from_isr(uart_ao, &evt->base, &woken);
    }

    portYIELD_FROM_ISR(woken);
}
```

`woken` is set to `pdTRUE` if the post unblocked a higher-priority task.
`portYIELD_FROM_ISR` ensures the scheduler runs at ISR exit if needed.

---

## What Happens After Dispatch

After your dispatch handler returns, ActiveRT:

1. Checks `event->pool`, if non-NULL, calls `activert_event_pool_free`
   to return the event to its pool.
2. If `pool == NULL` (a dynamically-allocated overflow event created by
   `ACTIVERT_POOL_OVERFLOW_DYNAMIC`), calls `vPortFree` instead.

You never need to free posted events manually.

---

## Posting to Multiple Active Objects

The same event **cannot** be posted to two Active Objects simultaneously,
only one AO owns it at a time. To fan out one event to multiple AOs,
allocate a separate event for each:

```c
void broadcast_sensor_reading(uint32_t reading)
{
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        sensor_event_t *evt = (sensor_event_t *)
            activert_event_pool_alloc(sensor_pool);
        if (evt) {
            evt->base.sig = SENSOR_SIG;
            evt->reading  = reading;
            activert_active_post(subscribers[i], &evt->base);
        }
    }
}
```

---

## Pool Exhaustion

If `activert_event_pool_alloc` returns `NULL`, the pool is full. Always
check for `NULL` before filling and posting. The right response depends on
your application:

- **Drop silently** - return without posting. Appropriate for periodic
  sensor readings where the latest value supersedes older ones.
- **Queue for retry** - post a `RETRY_SIG` to yourself with a delay.
- **Assert** - configure the pool with `ACTIVERT_POOL_OVERFLOW_ASSERT` so
  that exhaustion traps immediately during development.

The statistics system tracks `allocs_failed` and `events_dropped` so you
can monitor exhaustion rates at runtime.
