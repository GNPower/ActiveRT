# Synchronous Posting

`activert_active_post` is fire and forget. It returns as soon as the event is
queued, which is what the Active Object pattern is for: the poster does not
block on the handler, and neither task can see the other's state.

However, sometimes you need synchronous posting. For example, applying a 
configuration change before reading back a result, running a self-test step by 
step, or shutting a subsystem down in order all require knowing that a handler 
has finished. `activert_active_post_wait` posts an event and blocks the calling 
task until the target Active Object's dispatch handler has returned for that event.

```c
cfg_event_t *evt = (cfg_event_t *)activert_event_pool_alloc(cfg_pool);
if (evt != NULL) {
    evt->base.sig = CFG_APPLY_SIG;
    evt->baud     = 115200;

    int rc = activert_active_post_wait(radio_ao, &evt->base, pdMS_TO_TICKS(100));

    if (rc == ACTIVERT_POST_WAIT_OK) {
        /* The handler has returned. Safe to continue. */
    }
}
```

The calling task blocks on a binary semaphore, so it consumes no CPU while it
waits. There is no busy loop and no polling.

**What it reports:** only that `dispatch` ran and returned. It says nothing
about whether the handler succeeded. If you need a result, put it in shared
state the handler writes and the caller can read after the call returns, or have
the handler post a reply event.

---

## Return Codes and Event Ownership

Return codes must be handled properly, as mishandling can lead to leaks in pools or double frees.

| Return | Meaning | Who owns the event afterwards |
| --- | --- | --- |
| `ACTIVERT_POST_WAIT_OK` | Dispatch ran and returned | The Active Object. It has been freed |
| `ACTIVERT_POST_WAIT_DROPPED` | Consumed without dispatch because the Active Object was disabled | The Active Object. It has been freed |
| `ACTIVERT_POST_WAIT_FAILED` | The post never succeeded | **The caller.** Free it or retry |
| `ACTIVERT_POST_WAIT_TIMEOUT` | Timed out before dispatch began | The Active Object. Still queued, **do not free it** |

Only `ACTIVERT_POST_WAIT_FAILED` hands the event back:

```c
int rc = activert_active_post_wait(ao, &evt->base, pdMS_TO_TICKS(100));

if (rc == ACTIVERT_POST_WAIT_FAILED) {
    activert_event_pool_free(&evt->base);
}
```

Freeing after `ACTIVERT_POST_WAIT_TIMEOUT` is a double free. The event is still
sitting in the queue and the Active Object will free it when it eventually
dispatches it.

`ACTIVERT_POST_WAIT_FAILED` covers every case where the event never entered the
queue: a full queue, a signal with no matching queue, a disabled Active Object,
an Active Object with no queues, a call from the target's own task, and a call
made before the scheduler is running.

---

## Two-Phase Timeout

`timeout` bounds how long you wait for dispatch to **begin**. Once the Active
Object has taken the event, the call waits for the handler to return however
long that takes.

```
post_wait(ao, evt, pdMS_TO_TICKS(100))
   |
   |-- event still queued at 100 ms  --> ACTIVERT_POST_WAIT_TIMEOUT
   |
   '-- handler already started       --> wait for it, then OK
```

The
completion object lives on the calling task's stack and is freed the when
the function returns. While the event is still queued, the library can free it
atomically, and the Active Object then finds nothing to signal. Once the handler
is running, that free is no longer possible, so abandoning the wait would
leave the Active Object signalling a stack frame that no longer exists.

In practice the timeout protects you against a backed-up queue, a stopped Active
Object, or a deadlock. It does not protect you against a slow handler. Keep
handlers bounded, as the Active Object pattern already requires.

Pass `portMAX_DELAY` to wait indefinitely.

---

## Rules and Costs

**Task context only.** There is no `_from_isr` variant because an ISR cannot 
block. Post from an ISR with `activert_active_post_from_isr` and wait on the 
result some other way.

**Never post to yourself.** A dispatch handler calling `post_wait` on its own
Active Object would wait for a handler that cannot run until it returns. The
library checks for this and returns `ACTIVERT_POST_WAIT_FAILED` rather than
deadlocking.

**Cycles still deadlock.** Two Active Objects that `post_wait` on each other
will both block until their timeouts expire. The library cannot detect this,
so design your call graph so synchronous posts flow in one direction.

**A stopped Active Object never signals.** `activert_active_stop` deletes the
task wherever it happens to be. Anything waiting on an event still in that
Active Object's queue unblocks only on its timeout.

**Stack cost.** The completion object holds a `StaticSemaphore_t` and lives in
the calling task's stack frame for the duration of the call. Budget for
`sizeof(StaticSemaphore_t)` on top of whatever the calling function already
uses. No heap allocation is involved.

**Priority.** The poster blocks and the target runs. If the poster is the higher
priority task, it is descheduled until the handler finishes, so a long handler
can delay a higher priority task with synchronous posting.

---

## Posting to a Specific Queue

`activert_active_post_wait` routes by signal exactly as `activert_active_post`
does. To bypass routing on a multi-queue Active Object, use the indexed form:

```c
activert_active_post_to_queue_wait(ao, 1, &evt->base, pdMS_TO_TICKS(50));
```

---

## Turning the Feature Off

`ACTIVERT_ENABLE_POST_WAIT` defaults to `1`. It enables both the API and the
completion pointer the feature adds to `activert_event_t`, which costs one
pointer per event in every pool. Set it to `0` in your
`activert_user_config.h` to remove both:

```c
#define ACTIVERT_ENABLE_POST_WAIT 0
```

With it enabled, your `FreeRTOSConfig.h` must make two kernel APIs available:

```c
#define INCLUDE_xTaskGetSchedulerState    1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
```

FreeRTOS also compiles `xTaskGetSchedulerState` in when `configUSE_TIMERS` is
`1`, and `xTaskGetCurrentTaskHandle` when `configUSE_RECURSIVE_MUTEXES` is `1`
or the build is multicore, so many configurations already satisfy this. Builds
with either missing raise an `#error`.
