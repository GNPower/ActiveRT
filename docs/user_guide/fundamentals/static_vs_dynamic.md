# Static vs. Dynamic Active Objects

All examples so far have used the *static allocation* API via the
`DEFINE`/`INIT` macro pair. ActiveRT also supports *dynamic allocation*
for situations where AOs need to be created and destroyed at runtime.

Dynamic allocation is an opt-in feature and is **disabled by default**.

---

## Static Allocation (Default)

Static allocation means all memory is declared at compile time using the
`DEFINE` macro. No heap is used after scheduler start.

```c
/* All storage allocated at compile time */
ACTIVERT_ACTIVE_DEFINE_SIMPLE(my_ao, my_dispatch, 5, 2048, 8);

/* Initialise (creates FreeRTOS objects referencing the static arrays) */
ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
```

**Advantages:**

- Zero heap use means no fragmentation risk.
- Deterministic startup means memory errors appear at link time, not runtime.
- Required by many safety standards (MISRA-C, DO-178C, IEC 61508).
- Works with `configSUPPORT_DYNAMIC_ALLOCATION 0`.

**Limitation:** The set of Active Objects is fixed at compile time. You
cannot create a new AO in response to a runtime event.

---

## Dynamic Allocation

When `ACTIVERT_ENABLE_DYNAMIC_ALLOCATION 1` is set, the library exposes
`activert_active_create` and `activert_active_destroy`:

```c
/* In activert_config.h or before including activert.h */
#define ACTIVERT_ENABLE_DYNAMIC_ALLOCATION 1
#include "activert.h"
```

### Creating a dynamic AO

```c
activert_active_t *ao = activert_active_create(
    my_dispatch,      /* dispatch function       */
    5,                /* priority                */
    2048,             /* stack size in bytes     */
    8                 /* queue depth             */
);

if (ao == NULL) {
    /* pvPortMalloc failed — heap exhausted */
}
```

`activert_active_create` calls `pvPortMalloc` for:

- The `activert_active_t` control block
- The task stack
- The queue storage and control block

### Destroying a dynamic AO

```c
/* Sends TERM_SIG and waits for the task to self-delete */
activert_active_destroy(ao);
ao = NULL;   /* handle is invalid after this point */
```

`activert_active_destroy` posts `ACTIVERT_TERM_SIG`, waits for the
task to acknowledge by calling `vTaskDelete`, then frees the heap
memory. Do not access the handle after calling `destroy`.

---

## Comparison

| Property | Static (`DEFINE`/`INIT`) | Dynamic (`create`/`destroy`) |
| --- | --- | --- |
| Heap used | None | Yes (stack + queues + control block) |
| AOs defined at | Compile time | Runtime |
| Fragmentation risk | None | Yes, over many create/destroy cycles |
| Works without heap | Yes | No |
| MISRA-C / safety-critical | Fully compliant | Deviates from Rule 21.3 |
| Typical use | Production code | Test harnesses, plugin systems |

---

## Recommended Practice

Use **static allocation** for all production code. The static API
covers every use case that the dynamic API does; the only reason to reach
for dynamic allocation is when the application genuinely needs to create
or destroy AOs at runtime (eg. a communication channel AO created per
accepted network connection and destroyed when the connection closes).

For unit testing and host-side tooling where strict no-heap policies do
not apply, dynamic allocation can reduce boilerplate.
