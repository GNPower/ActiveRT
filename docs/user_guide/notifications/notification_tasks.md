# ActiveRT Notification Tasks

A *notification Active Object* extends a regular AO with a lightweight
ISR-to-task signalling channel. In addition to its event queue(s), the AO
has a **notification handler** that is invoked when a task notification
arrives without allocating an event from a pool.

## When to Use Notifications

Use task notifications when:

- An ISR needs to signal the AO at very high rates (eg., every DMA
  completion at 100 kHz) where event allocation overhead matters.
- The notification carries only a small integer value (32-bit), not a
  structured payload.
- You want the lowest possible ISR-exit latency.

Use regular event posting when:

- The trigger carries a structured payload (use an event type with fields).
- Multiple consumers need to receive the same signal.

## Creating a Notification AO

```c
/* Notification handler called from within the AO's task */
void my_notify_handler(activert_active_t *me, uint32_t notify_value)
{
    my_ao_t *self = (my_ao_t *)me;
    /* Process the notification value */
    handle_isr_trigger(self, notify_value);
}

/* Dispatch handler for normal events */
void my_dispatch(activert_active_t *me, const activert_event_t *e)
{
    switch (e->sig) {
        case ACTIVERT_INIT_SIG:  /* ... */ break;
        case CMD_SIG:            /* ... */ break;
    }
}

/* Static storage requires queue set storage for the semaphore */
static activert_active_t        s_my_ao;
static StaticSemaphore_t        s_notify_sem_cb;
/* ... queue, queue set, stack storage ... */

activert_active_t *my_ao = NULL;

/* Initialise */
void app_init(void)
{
    my_ao = activert_active_create_with_notification_static(
        &s_my_ao,
        my_dispatch,
        my_notify_handler,
        5,                 /* priority */
        queue_cfgs,
        num_queues,
        queue_storages,
        queue_cbs,
        stack,
        &s_task_cb,
        &s_notify_sem_cb,
        queue_set_storage,
        &s_queue_set_cb
    );
}
```

## Sending a Notification

From a task:

```c
activert_active_notify(my_ao, notify_value);
```

From an ISR:

```c
void DMA_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    activert_active_notify_from_isr(my_ao, DMA->TRANSFER_COUNT, &woken);
    portYIELD_FROM_ISR(woken);
}
```

## How It Works Internally

A notification AO places a binary semaphore into its queue set alongside
its regular event queue(s). When `activert_active_notify_from_isr` is
called:

1. The 32-bit notification value is written to the AO's `notification.pending_value` field (atomic on Cortex-M).
2. `xSemaphoreGiveFromISR` is called on the AO's notification semaphore.
3. The queue set wakes, `xQueueSelectFromSet` returns the semaphore handle.
4. The event loop recognises the semaphore, reads `pending_value`, and calls
   `notification_handler(me, pending_value)`.

This is equivalent to a zero-copy, zero-allocation event path.

## Limitations

- Only one notification value is stored. If the ISR fires again before the
  AO processes the first notification, the value is overwritten. Design
  your system to tolerate this, or use a regular event queue if every
  occurrence must be processed individually.
- The notification semaphore counts as one slot in the queue set, so the
  queue set must be sized accordingly (handled automatically by
  `activert_active_create_with_notification_static`).
