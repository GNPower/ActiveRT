# xTaskNotify vs. Semaphore

ActiveRT uses a **hybrid notification strategy**: pure-notification Active Objects
use `xTaskNotify` directly for ultra-low overhead, while queue+notification Active
Objects use a binary semaphore. This page explains when each path is taken and why.

---

## Two Notification Paths

### Path 1: Notification-Only AO (`xTaskNotify`)

When an Active Object is created with **no event queues**, only a notification
handler, ActiveRT calls `xTaskNotify` directly:

```text
activert_active_notify(ao, value)
    │
    └── me->notification.semaphore == NULL
            │
            └── xTaskNotify(me->thread, value, eSetBits)
                        │
                        ▼
                AO task wakes from ulTaskNotifyTake / xTaskNotifyWait
                Calls notification_handler(ao, notify_bits)
```

This is the lightest path, no queue set, no semaphore, just a direct kernel
notification to the task. It is ideal for AOs whose sole purpose is to react
to interrupt or timer signals with no event queue.

### Path 2: Queue+Notification AO (Binary Semaphore)

When an Active Object has **one or more event queues** and also needs a
notification channel, ActiveRT places a **binary semaphore inside the queue set**:

```text
activert_active_notify(ao, value)
    │
    └── me->notification.semaphore != NULL
            │
            ├── Accumulates bits: pending_value |= value  (critical section)
            └── xSemaphoreGive(ao->notification.semaphore)
                        │
                        ▼
                Queue set wakes -> xQueueSelectFromSet returns semaphore handle
                        │
                        ▼
                Event loop reads pending_value
                Calls notification_handler(ao, pending_value)
```

The ISR variant mirrors this with `xSemaphoreGiveFromISR` and
`xTaskNotifyFromISR` respectively.

---

## Why Not `xTaskNotify` for Queue+Notification AOs?

FreeRTOS `QueueSets` support blocking on multiple queues and semaphores
simultaneously, but they do **not** support `xTaskNotify`. A task waiting
inside `xQueueSelectFromSet` cannot also be unblocked by `xTaskNotify`.

This is the fundamental constraint: once an AO has a queue set, the
notification channel must be routed through a semaphore so the event loop
can block on a single `xQueueSelectFromSet` call and wake for either an
event or a notification.

---

## Comparison

| Property | Notification-only (`xTaskNotify`) | Queue+Notification (semaphore) |
| --- | --- | --- |
| Event queues | None | One or more |
| Queue set | No | Yes |
| Blocking primitive | `ulTaskNotifyTake` / `xTaskNotifyWait` | `xQueueSelectFromSet` |
| ISR-safe | Yes (`xTaskNotifyFromISR`) | Yes (`xSemaphoreGiveFromISR`) |
| Overhead | Lowest | Low |
| Bit accumulation | Kernel (eSetBits) | `pending_value` field |

---

## Accumulating Flags

In the queue+notification path, multiple notifiers can fire before the AO
runs. ActiveRT accumulates bits in `pending_value` using OR so no flags are
lost as long as the AO eventually runs. The full accumulated value is
passed to `notification_handler` in one call.

In the `xTaskNotify` path, use `eSetBits` as the notify action (which
ActiveRT does) to achieve the same accumulation through the kernel's own
notify value.

If you need to preserve flag history across multiple handler invocations,
store them in the AO's own context:

```c
void my_notify_handler(activert_active_t *me, uint32_t value)
{
    my_ao_context_t *context = (my_ao_context_t *)me->context;
    context->pending_flags |= value;   /* accumulate across calls */
    process_flags(&context->pending_flags);
}
```
