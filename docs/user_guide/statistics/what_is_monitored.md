# What Gets Monitored

When `ACTIVERT_ENABLE_STATS=1`, ActiveRT tracks the following metrics.

---

## Active Object Metrics

Accessible via `activert_active_print_stats(ao)` or the individual getter
functions:

| Metric | Getter | Description |
| --- | --- | --- |
| Events processed | `activert_stats_get_events_processed(ao)` | Total dispatch handler invocations |
| Events dropped | `activert_stats_get_events_dropped(ao)` | Posts that found all queues full |
| Notifications received | `activert_stats_get_notifications_received(ao)` | Notification handler invocations |
| Min dispatch time | `activert_stats_get_min_dispatch_us(ao)` | Fastest single dispatch (µs) |
| Max dispatch time | `activert_stats_get_max_dispatch_us(ao)` | Slowest single dispatch (µs) |
| Total dispatch time | `activert_stats_get_total_dispatch_us(ao)` | Cumulative handler runtime (µs) |
| Stack high-water | `activert_active_get_stack_high_water(ao)` | Free stack bytes remaining |
| Priority | *(in print output)* | FreeRTOS task priority |

---

## Queue Metrics

Per queue index `i` within an AO:

| Metric | Description |
| --- | --- |
| `posts_attempted` | Total calls to `activert_active_post` for this queue |
| `posts_succeeded` | Events successfully enqueued |
| `posts_failed` | Posts that found the queue full |
| `current_depth` | Events currently waiting in the queue |
| `peak_depth` | Highest recorded depth since last reset |

```c
activert_queue_print_stats(my_ao, 0);          /* queue 0 */
uint32_t pct = activert_queue_get_utilization(my_ao, 0); /* 0–100 */
```

---

## Event Pool Metrics

| Metric | Getter | Description |
| --- | --- | --- |
| Allocs attempted | `activert_event_pool_get_alloc_attempts(pool)` | Total `alloc` calls |
| Allocs failed | `activert_event_pool_get_alloc_failures(pool)` | `alloc` calls that returned `NULL` |
| Current allocated | `activert_event_pool_get_free_count` : pool_size - result | Events currently checked out |
| Peak allocated | `activert_event_pool_get_peak_usage(pool)` | Highest simultaneous allocation |

```c
activert_event_pool_print_stats(my_pool);
```

---

## System-Wide Summary

```c
activert_stats_print_summary();
```

Prints a one-line entry per registered AO and pool with the most
important counters:

```text
=== ActiveRT Statistics ===
Active Objects (2):
  [0] cmd_ao   prio=5  processed=1024  dropped=0   max_us=42
  [1] data_ao  prio=3  processed=8192  dropped=12  max_us=18
Event Pools (2):
  [0] cmd_pool    8/ 8 free  peak=6  failures=0
  [1] data_pool  14/32 free  peak=29 failures=3
```

---

## Health Checks

`activert_stats_check_health(ao)` evaluates the following thresholds:

| Condition | Level |
| --- | --- |
| Queue utilisation > 80 % | `WARNING` |
| Pool failure rate > 5 % | `WARNING` |
| Stack free < 10 % of total | `WARNING` |
| Queue overflow recorded (any `posts_failed > 0`) | `CRITICAL` |
| Pool failure rate > 50 % | `CRITICAL` |
| Stack free < 5 % of total | `CRITICAL` |

```c
activert_health_status_t h = activert_stats_check_health(my_ao);
if (h != ACTIVERT_HEALTH_OK) {
    log_warning("AO health degraded: %d", h);
}
```

---

## Resetting Statistics

```c
activert_stats_reset_active(my_ao);    /* single AO */
activert_stats_reset_pool(my_pool);    /* single pool */
activert_stats_reset_all();            /* all registered components */
```

Note: `peak` counters are reset to the current value, not zero, when
events are still allocated. A pool with 4 events currently checked out
will have `peak_allocated = 4` immediately after reset.

---

## Telemetry Export

For logging to non-volatile storage or transmitting over a network:

```c
size_t  size = activert_stats_get_export_size();
uint8_t buf[size];    /* or pvPortMalloc, must be 4-byte aligned */
activert_stats_export(buf, size);
/* buf now contains a compact binary snapshot of all stats */
```

---

## Monitoring Callbacks

Register real-time alert callbacks instead of polling:

```c
/* Called when queue depth changes for AO index 0, queue 0 */
activert_stats_monitor_queue_depth(my_ao, 0, on_depth_change_cb);

/* Called when the pool allocation failure count increments */
activert_stats_monitor_pool_exhaustion(my_pool, on_pool_exhaustion_cb);

/* Called when stack free bytes fall below a threshold */
activert_stats_monitor_stack_usage(my_ao, on_stack_low_cb);
```
