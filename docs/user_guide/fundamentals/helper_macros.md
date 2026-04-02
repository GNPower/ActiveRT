# Helper Macros

ActiveRT provides convenience macros that declare all the static storage
needed for a pool or Active Object in a single statement. This page
explains what each macro expands to so you know exactly what memory is
being reserved and why the macros must be used the way they are.

---

## Event Pool Macros

### `ACTIVERT_EVENT_POOL_DEFINE`

Declares static storage and the pool handle. Must appear at **file scope**
(not inside a function).

```c
ACTIVERT_EVENT_POOL_DEFINE(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
```

Expands to approximately:

```c
static uint8_t              _my_pool_storage[8 * sizeof(my_event_t)];
static uint8_t              _my_pool_bitmap[1];          /* ceil(8/8) bytes */
static StaticSemaphore_t    _my_pool_mutex_cb;
static activert_event_pool_t _my_pool_obj;
activert_event_pool_t       *my_pool = NULL;             /* the public handle */
```

The `my_pool` identifier is the handle used in all subsequent API calls.
It is `NULL` until `ACTIVERT_EVENT_POOL_INIT` is called.

### `ACTIVERT_EVENT_POOL_INIT`

Creates the FreeRTOS mutex and initialises the pool control block.
Must be called **inside a function** (eg. from a startup task).
The arguments must match those given to `DEFINE` exactly.

```c
ACTIVERT_EVENT_POOL_INIT(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
```

After this call, `my_pool` is non-NULL and ready for `alloc`/`free`.

---

## Active Object Macros

### `ACTIVERT_ACTIVE_DEFINE_SIMPLE`

Declares storage for a single-queue Active Object. Must appear at
**file scope**.

```c
ACTIVERT_ACTIVE_DEFINE_SIMPLE(
    my_ao,        /* handle name                    */
    my_dispatch,  /* dispatch function (forward decl OK) */
    5,            /* FreeRTOS task priority          */
    2048,         /* stack size in bytes             */
    8             /* event queue depth               */
);
```

Expands to approximately:

```c
static activert_active_t  _my_ao_obj;
static StackType_t        _my_ao_stack[2048 / sizeof(StackType_t)];
static StaticTask_t       _my_ao_task_cb;
static void              *_my_ao_queue_storage[8];
static StaticQueue_t      _my_ao_queue_cb;
activert_active_t        *my_ao = NULL;                  /* the public handle */
```

### `ACTIVERT_ACTIVE_INIT_SIMPLE`

Creates the FreeRTOS task and queue, registers the AO with the stats
system, and posts `ACTIVERT_INIT_SIG`. Must be called **inside a function**.

```c
ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
```

### `ACTIVERT_ACTIVE_DEFINE_LOOP` / `ACTIVERT_ACTIVE_INIT_LOOP`

Identical to the `SIMPLE` variants but additionally stores a loop function
pointer that is called on every tick in which no event is waiting.

```c
ACTIVERT_ACTIVE_DEFINE_LOOP(my_ao, my_dispatch, my_loop, 5, 2048, 8);
ACTIVERT_ACTIVE_INIT_LOOP(my_ao, my_dispatch, my_loop, 5);
```

---

## Important Constraints

**`DEFINE` must be at file scope.** The macros declare `static` arrays.
Placing them inside a function body would make the arrays local and
potentially stack-allocated, so the storage would be destroyed when the
function returns, while the FreeRTOS objects that reference it live on.

**`INIT` must be called inside a function.** FreeRTOS queue and task
creation APIs (`xQueueCreateStatic`, `xTaskCreateStatic`) cannot be called
outside of a function. Call `INIT` from your first FreeRTOS task or from a
pre-scheduler hook.

**Arguments to `DEFINE` and `INIT` must match.** The macro pair shares
no compile-time linkage. A mismatch (eg. different queue depths) will
silently produce undefined behaviour at runtime. Ideally you should
define the pool and Active Oject parameters as named constants:

```c
#define MY_POOL_CAPACITY  8
#define MY_AO_STACK_SIZE  2048
#define MY_AO_QUEUE_DEPTH 8
#define MY_AO_PRIORITY    5

ACTIVERT_EVENT_POOL_DEFINE(my_pool, my_event_t,
                           MY_POOL_CAPACITY, ACTIVERT_POOL_OVERFLOW_DROP);

ACTIVERT_ACTIVE_DEFINE_SIMPLE(my_ao, my_dispatch,
                               MY_AO_PRIORITY, MY_AO_STACK_SIZE, MY_AO_QUEUE_DEPTH);

/* In startup: */
ACTIVERT_EVENT_POOL_INIT(my_pool, my_event_t,
                         MY_POOL_CAPACITY, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, MY_AO_PRIORITY);
```

---

## When Not to Use the Helper Macros

The helper macros cover the most common cases. For advanced scenarios such as
multi-queue AOs, notification AOs, or AOs with application-specific
extended control blocks, use the lower-level `activert_active_create_static`
and `activert_event_pool_init_static` functions directly. These give you
full control over storage layout at the cost of more verbose setup code.
