# Absolute Basics for Beginners

This page explains the core concepts you need before writing your first
Active Object application. No prior knowledge of the Active Object pattern
is assumed.

---

## Events

An *event* is a small message that carries a **signal** (what happened) and
an optional **payload** (associated data). Every event in ActiveRT is a
C struct whose first member is `activert_event_t`:

```c
typedef struct {
    activert_event_t base;   /* MUST be first, carries the signal */
    uint32_t         value;  /* your payload */
    uint8_t          flags;  /* also your payload */
} my_event_t;
```

The `activert_event_t` base stores:

- `sig` - the signal number (a `uint32_t`)
- `pool` - a back-pointer to the pool this event belongs to (set automatically)

---

## Signals

A signal is just an integer that identifies *what happened*. ActiveRT
reserves the first 16 signal values for internal use:

| Signal | Value | Meaning |
| --- | --- | --- |
| `ACTIVERT_INIT_SIG` | 0 | Sent to every AO when it starts |
| `ACTIVERT_TERM_SIG` | 1 | Sent by `activert_active_stop()` |
| `ACTIVERT_USER_SIG` | 16 | First signal available for user code |

Define your application signals starting at `ACTIVERT_USER_SIG`:

```c
typedef enum {
    SENSOR_SIG  = ACTIVERT_USER_SIG,
    CONTROL_SIG,
    TIMEOUT_SIG,
    /* ... */
} app_signal_t;
```

---

## Event Pools

Because events are allocated and freed very frequently, and because
`pvPortMalloc` can be too slow and fragmentation-prone for high-rate embedded
use, ActiveRT uses *event pools*: pre-allocated arrays of fixed-size
event slots managed with a bitmap.

```text
Pool (capacity = 4):

 slot 0  │ ████ │  allocated
 slot 1  │      │  free
 slot 2  │ ████ │  allocated
 slot 3  │      │  free

bitmap: 0b0101  (bit set = allocated)
```

Allocation is O(1) - find the first zero bit, set it, return the
pointer. Free is also O(1) - clear the bit.

Each pool holds events of **one type** (fixed `event_size`). If you
have multiple event types with different payload sizes, create one pool
per type.

---

## Active Objects

An Active Object is a FreeRTOS task bundled with:

- One or more FIFO event queues
- A dispatch handler function you write
- Optionally: statistics counters, a name string, a notification channel

The task loop looks like this (simplified):

```c
/* Inside ActiveRT - you do not write this */
for (;;) {
    event = receive_from_queue_or_set();
    dispatch_handler(me, event);
    activert_event_pool_free(event);   /* automatic */
}
```

Your dispatch handler receives one event at a time. It runs to completion
before the next event is dequeued. This is the key guarantee that makes
the pattern safe: your state variables are only touched from within the
dispatch handler, which never runs concurrently with itself.

---

## The Lifecycle

Every Active Object goes through the same lifecycle:

```{figure} ../../img/ao_life.png
:alt: Active Object lifecycle - allocate storage, create queue and mutex, dispatch ACTIVERT_INIT_SIG, normal operation, call activert_active_stop(), dispatch ACTIVERT_TERM_SIG, task self-deletes
:align: center
:figclass: diagram-invertible
```

---

## Memory Model

All memory is declared statically at file scope using the `DEFINE` macro, unless
the user opts-in to dynamic allocation via `ACTIVERT_ENABLE_DYNAMIC_ALLOCATION`.
The layout for a simple AO looks like:

```text
Static memory (declared at file scope by DEFINE macro):
┌─────────────────────────────┐
│  activert_active_t object   │  AO control block
├─────────────────────────────┤
│  StackType_t stack[N]       │  FreeRTOS task stack
├─────────────────────────────┤
│  StaticTask_t task_cb       │  FreeRTOS TCB
├─────────────────────────────┤
│  void* queue_storage[depth] │  Queue item storage
├─────────────────────────────┤
│  StaticQueue_t queue_cb     │  FreeRTOS queue control block
└─────────────────────────────┘

Heap used at runtime: ZERO (when using static-allocation API)
```

The event pool similarly stores its bitmap and event array as static arrays.
