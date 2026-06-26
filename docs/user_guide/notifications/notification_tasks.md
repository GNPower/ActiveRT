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

/* Static storage for a single-queue notification AO (zero heap). The queue set
 * holds every queued event PLUS the notification semaphore, so size
 * queue_set_storage with the +1 via ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES;
 * omitting the +1 lets the kernel write one pointer past the buffer. */
#define MY_QUEUE_LEN 8

static StackType_t       s_stack[1024 / sizeof(StackType_t)];
static StaticTask_t      s_task_cb;
static activert_event_t *s_queue_storage[MY_QUEUE_LEN];
static StaticQueue_t     s_queue_cb;
static StaticQueue_t     s_queue_set_cb;
static uint8_t           s_queue_set_storage[ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(MY_QUEUE_LEN)];
static StaticSemaphore_t s_notify_sem_cb;
static activert_active_t s_my_ao;
static activert_queue_t  s_queue_struct;

activert_active_t *my_ao = NULL;

/* Initialize */
void app_init(void)
{
    activert_queue_config_t cfg = {
        .signal_base = 0, 
        .signal_count = 0,          /* catch-all queue */
        .queue_length = MY_QUEUE_LEN, 
        .event_pool = my_pool
    };
    activert_event_t **queue_storages[1] = { s_queue_storage };

    my_ao = activert_active_create_with_notification_static(
        "my_ao",                    /* name */
        my_dispatch,                /* dispatch function */
        my_notify_handler,          /* notify_handler function */
        5,                          /* priority */
        s_stack,                    /* stack */
        sizeof(s_stack),            /* stack size */
        &s_task_cb,                 /* task cb */
        &cfg,                       /* queue configs */
        1,                          /* num_queues */
        &s_queue_cb,                /* queue cbs */
        queue_storages,             /* queue storages */
        &s_queue_set_cb,            /* queue set cb */
        s_queue_set_storage,        /* queue set storage */
        &s_notify_sem_cb,           /* notify semaphore callback */
        &s_my_ao,                   /* active object storage */
        &s_queue_struct             /* queue structs */
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

1. The notification bits are OR-accumulated into the AO's
   `notification.pending_value` field under a critical section, and a
   `pending` flag is set (so a notify with a value of 0 is still delivered).
2. `xSemaphoreGiveFromISR` is called on the AO's notification semaphore.
3. The queue set wakes, `xQueueSelectFromSet` returns the semaphore handle.
4. The event loop recognizes the semaphore, reads and clears `pending_value`,
   and calls `notification_handler(me, pending_value)`.

This is equivalent to a zero-copy, zero-allocation event path.

## Limitations

- Notification bits are OR-accumulated into a single 32-bit value. If the ISR
  fires several times before the AO processes the notification, the bits are
  merged (set bits are preserved) but collected into one handler call, so the
  count of occurrences is lost. Use a regular event queue if every occurrence
  must be processed individually.
- The notification semaphore counts as one slot in the queue set, so the
  queue set must be sized accordingly (handled automatically by
  `activert_active_create_with_notification_static`).
