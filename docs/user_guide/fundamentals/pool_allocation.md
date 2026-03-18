# Allocating and Freeing Events

## The Allocation Lifecycle

Every event follows the same path through the system:

```text
activert_event_pool_alloc()
    │  caller fills payload
    ▼
activert_active_post()           <- ownership transfers to ActiveRT
    │  event sits in queue
    ▼
dispatch_handler(me, event)      <- ActiveRT calls your handler
    │  handler reads payload
    ▼
activert_event_pool_free()       <- ActiveRT frees automatically
```

**Ownership rule:** once you call `activert_active_post`, the event
belongs to ActiveRT. Do not read, write, or free the event after posting.

---

## Task-Context Allocation

```c
my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
if (evt == NULL) {
    return;   /* pool exhausted */
}
evt->base.sig = MY_SIG;
evt->value    = 42;
activert_active_post(my_ao, &evt->base);
```

`activert_event_pool_alloc` takes the pool's internal mutex before
scanning the bitmap, so it is safe to call from multiple tasks
simultaneously. It must **not** be called from an ISR, instead use the
`_from_isr` variant.

---

## ISR-Context Allocation

```c
void MY_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;

    my_event_t *evt = (my_event_t *)
        activert_event_pool_alloc_from_isr(my_pool, &woken);

    if (evt != NULL) {
        evt->base.sig = MY_SIG;
        evt->value    = get_some_data();
        activert_active_post_from_isr(my_ao, &evt->base, &woken);
    }

    portYIELD_FROM_ISR(woken);
}
```

`activert_event_pool_alloc_from_isr` uses `taskENTER_CRITICAL_FROM_ISR`
instead of a mutex take, making it safe at interrupt priority. The
`woken` flag is set by `activert_active_post_from_isr` if the post
unblocked a higher-priority task; `portYIELD_FROM_ISR` ensures the
scheduler runs immediately at ISR exit in that case.

---

## Freeing Events Manually

ActiveRT frees the event automatically after every `dispatch_handler`
call. You should almost never need to call `activert_event_pool_free`
directly. The only cases where a manual free is needed:

**1. You allocated an event but decided not to post it:**

```c
my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
if (evt == NULL) { return; }

if (!should_send()) {
    activert_event_pool_free(&evt->base);   /* must free - you still own the event */
    return;
}

activert_active_post(my_ao, &evt->base);   /* ActiveRT frees after dispatch */
```

**2. You received an event in a dispatch handler and want to re-use it
by posting it to another Active Object**

Do not do this. Instead, allocate a
new event for each Active Object. An event can only be owned by one Active Object at a time.

---

## The `pool` Back-Pointer

Every allocated event has its `base.pool` field set to the pool it came
from. ActiveRT uses this at free time to route the event back to the
correct pool without any additional bookkeeping from the caller.

If `base.pool == NULL`, the event was heap-allocated (via
`ACTIVERT_POOL_OVERFLOW_DYNAMIC`) and ActiveRT calls `vPortFree` instead
of clearing a bitmap slot.

You can inspect which pool an event belongs to:

```c
if (e->pool == cmd_pool) { /* ... */ }
```

But modifying `e->pool` from user code is *NOT* allowed and results in
undefined behaviour.

---

## ISR-Safe Free

In the rare case you need to free an event from an ISR (eg. you allocated from
an ISR but the ISR itself decided not to post):

```c
BaseType_t woken = pdFALSE;
activert_event_pool_free_from_isr(&evt->base, &woken);
portYIELD_FROM_ISR(woken);
```
