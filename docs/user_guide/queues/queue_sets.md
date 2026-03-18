# Queue Sets

A *multi-queue* Active Object owns two or more queues. The AO's event
loop uses a FreeRTOS **queue set** to block on all queues simultaneously
and wake as soon as any queue receives an event.

## Why Multiple Queues?

Separate queues allow different event classes to have different depths and
different pool associations. Typical use cases:

| Use case | High-priority queue | Low-priority queue |
| --- | --- | --- |
| Motor controller | Fault / stop events (depth 2) | Status update events (depth 16) |
| Protocol stack | Command events from task (depth 4) | Received frame events from ISR (depth 32) |
| Data logger | Control signals (depth 2) | Sample events (depth 64) |

Note: FreeRTOS queue sets do not enforce scheduling priority across queues.
Events are served in arrival order regardless of which queue they came from.

## Creating a Multi-Queue AO

```c
/* Signal ranges for each queue */
#define CMD_SIG_BASE   ACTIVERT_USER_SIG
#define CMD_SIG_COUNT  4                            /* signals 16..19 */
#define DATA_SIG_BASE  (ACTIVERT_USER_SIG + 4)
#define DATA_SIG_COUNT 8                            /* signals 20..27 */

/* Queue configurations */
activert_queue_config_t queue_cfgs[2] = {
    {
        .signal_base  = CMD_SIG_BASE,
        .signal_count = CMD_SIG_COUNT,
        .queue_length = 4,
        .pool         = cmd_pool,
    },
    {
        .signal_base  = DATA_SIG_BASE,
        .signal_count = DATA_SIG_COUNT,
        .queue_length = 32,
        .pool         = data_pool,
    },
};

/* Static storage, total_signals = sum of all signal_count values */
#define TOTAL_SIGNALS  (CMD_SIG_COUNT + DATA_SIG_COUNT)

/* Stack, queue set size, and per-queue storage declared via raw API */
static activert_active_t           s_my_ao;
static StackType_t                 s_stack[4096 / sizeof(StackType_t)];
static StaticTask_t                s_task_cb;
static activert_queue_storage_t    s_queue_storages[2];   /* one per queue */
static StaticQueue_t               s_queue_cbs[2];
/* ... queue set storage sized to sum of queue_lengths + 1 if semaphore */
```

For typical use, prefer the higher-level `activert_active_create_static`
API.

## How Queue Sets Work Internally

ActiveRT creates a FreeRTOS queue set sized to the sum of all queue depths
(plus 1 if a notification semaphore is present). Each queue is added to
the set with `xQueueAddToSet`. The event loop calls
`xQueueSelectFromSet` to block until any queue has data, then
`xQueueReceive` on the selected queue.

```text
Queue Set (capacity = sum of all queue depths)
  ├── cmd_queue   (depth 4)
  ├── data_queue  (depth 32)
  └── notify_sem  (depth 1, optional)

xQueueSelectFromSet -> returns the handle that has data
xQueueReceive(handle, &event) -> gets the event
```

Static queue set allocation uses `xQueueCreateSetStatic`, which requires
FreeRTOS 11.2.0 or later.

## Queue Statistics

Each queue in a multi-queue AO tracks the same statistics as a single
queue:

```c
/* Get stats for queue index 1 (data queue in the example above) */
activert_queue_print_stats(my_ao, 1);

/* Utilization as a percentage (0–100) */
uint32_t pct = activert_queue_get_utilization(my_ao, 1);
```
