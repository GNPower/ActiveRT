# Event Routing

When an Active Object has multiple queues, ActiveRT must route each
incoming event to the correct queue. Routing is based on **signal ranges**.

## Signal Ranges

Each queue in a multi-queue AO is configured with:

```c
activert_queue_config_t cfg = {
    .signal_base  = 20,   /* first signal routed to this queue */
    .signal_count = 8,    /* signals 20..27 go here            */
    .queue_length = 16,
    .pool         = data_pool,
};
```

When `activert_active_post(my_ao, evt)` is called, ActiveRT checks which
queue's signal range contains `evt->sig` and posts to that queue:

```c
evt->sig = 22  // 20 <= 22 < 28  routes to queue 1 (data_queue)
evt->sig = 17  // 16 <= 17 < 20  routes to queue 0 (cmd_queue)
```

## Design Rules for Signal Ranges

1. **Ranges must not overlap.** If a signal matches two queues, the
   event will go to the first matching queue.
2. **All signals you post must fall within a declared range.** Signals
   outside all ranges cause the post to fail and increment the
   `posts_failed` counter.
3. **Signals below `ACTIVERT_USER_SIG` (16)**, `ACTIVERT_INIT_SIG` and
   `ACTIVERT_TERM_SIG`, are always routed to queue 0.

## Choosing Signal Allocations

A clean pattern is to define signal ranges as named enum blocks:

```c
/* cmd.h - command events (routed to high-priority queue) */
typedef enum {
    CMD_START_SIG = ACTIVERT_USER_SIG,   /* 16 */
    CMD_STOP_SIG,                        /* 17 */
    CMD_RESET_SIG                        /* 18 */
} cmd_signal_t;
#define CMD_SIG_COUNT 3
#define CMD_SIG_BASE  ACTIVERT_USER_SIG

/* data.h - data events (routed to low-priority queue) */
typedef enum {
    DATA_SAMPLE_SIG = CMD_SIG_BASE + CMD_SIG_COUNT,  /* 19 */
    DATA_FLUSH_SIG                                   /* 20 */
} data_signal_t;
#define DATA_SIG_COUNT 2
#define DATA_SIG_BASE  (CMD_SIG_BASE + CMD_SIG_COUNT)
```

## Routing from ISR

`activert_active_post_from_isr` performs the same signal-range routing as
the task-context version:

```c
void DMA_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    data_event_t *evt = (data_event_t *)
        activert_event_pool_alloc_from_isr(data_pool, &woken);
    if (evt) {
        evt->base.sig = DATA_SAMPLE_SIG;   /* routed to data_queue */
        evt->sample   = DMA->BUFFER[0];
        activert_active_post_from_isr(my_ao, &evt->base, &woken);
    }
    portYIELD_FROM_ISR(woken);
}
```

## What Happens When No Queue Matches?

If the signal does not fall within any configured range, the post function
returns `-1` and the stats counter `stats.events_dropped` is incremented.
No assertion is raised. In development builds, enable
`ACTIVERT_ENABLE_DEBUG 1` to see a printf diagnostic.
