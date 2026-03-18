# Single Queue

A *simple* Active Object has exactly one event queue. All posted events
enter this queue in FIFO order and are dispatched one at a time.

## When to Use a Single Queue

A single queue is the right choice when:

- All events for the AO come from a single priority class
- You do not need ISR + task notifications on the same AO
- Simplicity is more important than fine-grained scheduling control

## Creating a Simple AO

```c
ACTIVERT_ACTIVE_DEFINE_SIMPLE(
    my_ao,        /* handle: activert_active_t *my_ao        */
    my_dispatch,  /* dispatch function                        */
    5,            /* FreeRTOS priority                        */
    2048,         /* stack bytes                              */
    8             /* queue depth (max queued events)          */
);

/* Inside a function: */
ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
```

## Queue Depth

The queue depth is the maximum number of events that can be waiting for
dispatch at any moment. If the queue is full when `activert_active_post`
is called, the post fails silently and the stats counter
`queues[0].stats.posts_failed` is incremented.

Choose the queue depth based on the maximum burst of events you expect
between two consecutive dispatch cycles. A depth of 4–16 is typical;
very high depths waste static RAM.

## Inspecting Queue State

```c
/* Current number of events waiting */
size_t depth = activert_queue_get_depth(my_ao, 0);

/* Free slots remaining */
size_t free  = activert_queue_get_free_space(my_ao, 0);

/* Predicates */
bool full  = activert_queue_is_full(my_ao, 0);
bool empty = activert_queue_is_empty(my_ao, 0);

/* Flush all pending events (discard without dispatching) */
activert_queue_flush(my_ao, 0);
```

## Internally

A single-queue AO does **not** use a FreeRTOS queue set. The event loop
blocks directly on the single `QueueHandle_t`. This keeps memory use
and latency lower than a multi-queue AO.

When `ACTIVERT_ENABLE_STATS` is enabled, each queue tracks:

| Statistic | Description |
| --- | --- |
| `posts_attempted` | Total post calls |
| `posts_succeeded` | Events successfully enqueued |
| `posts_failed` | Posts that found the queue full |
| `current_depth` | Current occupancy |
| `peak_depth` | Highest observed occupancy since reset |
