# Enabling and Disabling Active Objects

An Active Object runs from the moment it is created until `activert_active_stop`
deletes its task. Stopping is destructive: it dispatches `TERM_SIG`, calls
`vTaskDelete`, and leaves the queues and anything still in them behind. When you
only want a subsystem to stop temporarily, disable it instead.

```c
activert_active_set_enabled(sensor_ao, false);

/* ... sometime later during execution ... */

activert_active_set_enabled(sensor_ao, true);
```

The task, its queues, its event pool and its statistics all stay allocated. There is
no teardown and no re-creation.

---

## What Disabling Does

**Posts are rejected.** `activert_active_post` and every other post function
return `-1` while the Active Object is disabled. That is the same return value as a
full queue, so the caller keeps ownership of the event and is responsible for
freeing it:

```c
my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
if (evt != NULL) {
    evt->base.sig = MY_SIG;

    if (activert_active_post(my_ao, &evt->base) != 0) {
        activert_event_pool_free(&evt->base);   /* still ours */
    }
}
```

Code that already handles a failed post correctly needs no change to work with a
disabled Active Object.

**Queued events are discarded.** An event posted before the Active Object is disabled 
is dequeued, freed back to its pool, and counted in `stats.events_dropped` if it is 
still in the pool when the Active Object is disabled. Its dispatch handler is never 
called.

A disabled Active Object uses no CPU. Nothing can be posted to it, so its task
stays blocked on its queue as it would when idle.

---

## What Disabling Does Not Do

| | Behaviour while disabled |
| --- | --- |
| Queue events | Rejected on post, discarded if already queued |
| Notification handlers | Still run |
| `ACTIVERT_INIT_SIG` | Unaffected, dispatched at task start |
| `ACTIVERT_TERM_SIG` | Unaffected, dispatched by `activert_active_stop` |
| Statistics | Still collected, still registered for the CLI |
| A handler already running | Runs to completion |

Notifications keep flowing on purpose. They are the low-latency path from an
ISR, and an Active Object that has been asked to stop processing events often
still needs to acknowledge interrupts. `INIT_SIG` and `TERM_SIG` are built on the
stack and handed straight to the dispatch handler rather than routed through a
queue, so get dispatched no matter the state of the Active Object.

Disabling is asynchronous. It just sets a flag, so if the dispatch
handler is mid-call when you disable it, that call finishes normally.

---

## Which Active Objects Can Be Disabled

Only ones that dispatch queue events. `activert_active_set_enabled` returns `-1`
and changes nothing for:

- **Loop tasks** created with `activert_active_create_loop_static`, which call
  their loop function directly and own no queue
- **Notification-only Active Objects** created with `num_queues == 0`

Both cases are `me->queue_count == 0`. Check the return value to see if you have
successfully disabled the Active Object:

```c
if (activert_active_set_enabled(ao, false) != 0) {
    /* No event dispatch function, a loop task or a notification-only AO */
}
```

New Active Objects start enabled. To start one in a disabled state, just disable
it immediately after creation. If nothing has been posted to it yet, if has the 
same effect. Its `INIT_SIG` is still dispatched.

---

## Reading the Current State

```c
if (!activert_active_is_enabled(radio_ao)) {
    printf("radio is standing down\n");
}
```

`activert_active_is_enabled` returns `true` for a loop task or a
notification-only Active Object, because both are running normally even though
the flag cannot be cleared.

---

## Convenience Aliases

`activert.h` defines two shorthands alongside `activert_post` and
`activert_notify`:

```c
#define activert_enable(ao)  activert_active_set_enabled((ao), true)
#define activert_disable(ao) activert_active_set_enabled((ao), false)
```

---

## Interaction With Synchronous Posting

`activert_active_post_wait` fails immediately with `ACTIVERT_POST_WAIT_FAILED`
when the target is disabled, since that Active Object that will not dispatch 
the event while disabled.

If the Active Object is disabled after the event is queued but before it is
dispatched, the waiting task is still released, with
`ACTIVERT_POST_WAIT_DROPPED`. See [Synchronous Posting](synchronous_posting).
