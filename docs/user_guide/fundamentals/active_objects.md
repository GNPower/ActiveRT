# Basic Active Objects

## The DEFINE / INIT Pattern

ActiveRT separates storage declaration from initialization. This is
necessary because static storage must be declared at file scope (before
`main`), but calls to FreeRTOS functions cannot be made before the code
executes.

**Step 1: Declare storage (file scope, before `main`)**

```c
ACTIVERT_ACTIVE_DEFINE_SIMPLE(
    my_ao,        /* handle name: creates  activert_active_t *my_ao    */
    my_dispatch,  /* dispatch function (forward declaration OK)        */
    5,            /* FreeRTOS task priority                            */
    2048,         /* task stack size in bytes                          */
    8             /* event queue depth (max events in flight)          */
);
```

This macro expands to:

- `static activert_active_t  _my_ao_obj;` <- the control block
- `static StackType_t        _my_ao_stack[2048 / sizeof(StackType_t)];`
- `static StaticTask_t       _my_ao_task_cb;`
- `static void*              _my_ao_queue_storage[8];`
- `static StaticQueue_t      _my_ao_queue_cb;`
- `activert_active_t        *my_ao = NULL;` <- the public handle

**Step 2: Initialise (after scheduler is running)**

```c
ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
```

This calls `activert_active_create_static` internally, which:

1. Creates the FreeRTOS queue (static)
2. Creates the FreeRTOS task (static)
3. Registers the AO with the stats system (if enabled)
4. Posts `ACTIVERT_INIT_SIG` to the queue

---

## The Dispatch Handler

The dispatch handler is a plain C function with this signature:

```c
void my_dispatch(activert_active_t *me, const activert_event_t *e);
```

| Parameter | Description |
| --- | --- |
| `me` | Pointer to the owning Active Object; cast to your subtype to access state |
| `e` | The current event; cast to your event type to access payload |

The handler runs to completion inside the AO's task. It must not block
(no `vTaskDelay`, no blocking semaphore takes). Long-running work should
be broken into a sequence of events.

### Carrying state

To give an Active Object private state, store it in the `void* context` field
of `activert_active_t`:

```c
typedef struct {
    uint32_t          counter;
    bool              armed;
} my_ao_context_t;

/* Create a static context */
static my_ao_context_t my_ao_context;

/* Declare the context when creating the Active Object type */
void ao_init()
{
    ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
    my_ao_context.counter = 0;
    my_ao_context.armed = false;
    my_ao.context = (void*)&my_ao_context;
}

/* Context can be read and/or modified in the dispatch handler */
void my_dispatch(activert_active_t *me, const activert_event_t *e)
{
    my_ao_context_t *context = (my_ao_context_t *)me->context;

    switch (e->sig) {
        case ACTIVERT_INIT_SIG:
            context->counter = 0;
            context->armed   = true;
            break;
        /* ... */
    }
}
```

---

## Loop Active Objects

A *loop AO* additionally calls a periodic function on every scheduler tick
in which no event is waiting. Use this for tasks that must do background
work at a regular rate regardless of event traffic.

```c
void my_loop(activert_active_t *me)
{
    my_ao_context_t *context = (my_ao_context_t *)me->context;
    /* Poll sensor, update LEDs, run PID, or anything time-driven */
    run_control_step(context);
}

ACTIVERT_ACTIVE_DEFINE_LOOP(my_ao, my_dispatch, my_loop, 5, 4096, 8);
ACTIVERT_ACTIVE_INIT_LOOP(my_ao, my_dispatch, my_loop, 5);
```

The loop function runs from within the AO's task with the same
thread-safety guarantees as the dispatch handler. It should complete
quickly to avoid blocking event processing.

---

## Stopping an Active Object

```c
activert_active_stop(my_ao);
```

This posts `ACTIVERT_TERM_SIG` to the AO's queue. When the dispatch
handler returns from processing `ACTIVERT_TERM_SIG`, ActiveRT calls
`vTaskDelete` on the AO's task. The AO handle becomes invalid after this
point.

Use `ACTIVERT_TERM_SIG` to free any resources your AO acquired:

```c
case ACTIVERT_TERM_SIG:
    close_peripheral(&context->handle);
    break;
```

---

## Stack Monitoring

```c
/* Returns the high-water free bytes remaining on the task stack */
uint32_t free_stack = activert_active_get_stack_high_water(my_ao);
```

A return value close to zero indicates a stack overflow risk. Increase
the stack size in the `DEFINE` macro if needed.
