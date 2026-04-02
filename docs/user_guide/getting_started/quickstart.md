# ActiveRT QuickStart

This page walks through a complete, self-contained example: a single
Active Object that processes two signal types.

---

## Step 1: Define your event type

Every event starts with an `activert_event_t` base as its **first member**.
Extend it with your payload fields:

```c
#include "activert.h"

/* Signal definitions - start at ACTIVERT_USER_SIG to avoid
   collisions with the reserved INIT_SIG and TERM_SIG. */
typedef enum {
    CMD_SIG  = ACTIVERT_USER_SIG,
    DATA_SIG,
} my_signal_t;

/* Application event - base must be first */
typedef struct {
    activert_event_t base;   /* signal, pool pointer */
    uint32_t         value;
} my_event_t;
```

---

## Step 2: Declare static storage

Use the convenience macros at **file scope**. They expand to static arrays
and a handle pointer, meaning no heap allocation:

```c
/* Event pool: 8 events, drop new arrivals when full */
ACTIVERT_EVENT_POOL_DEFINE(my_pool,
                           my_event_t,
                           8,
                           ACTIVERT_POOL_OVERFLOW_DROP);

/* Active Object: priority 5, 2 KB stack, queue depth 8 */
ACTIVERT_ACTIVE_DEFINE_SIMPLE(my_ao,
                               my_dispatch,   /* forward-declared below */
                               5,
                               2048,
                               8);
```

---

## Step 3 — Write the dispatch handler

The dispatch handler is called once per event, from within the AO's task:

```c
void my_dispatch(activert_active_t *me, const activert_event_t *e)
{
    (void)me;  /* cast to your AO subclass if you carry state */

    switch (e->sig)
    {
        case ACTIVERT_INIT_SIG:
            /* First event. Run one-time startup code here */
            break;

        case ACTIVERT_TERM_SIG:
            /* Sent by activert_active_stop(); clean up resources */
            break;

        case CMD_SIG:
        {
            const my_event_t *evt = (const my_event_t *)e;
            process_command(evt->value);
            break;
        }

        case DATA_SIG:
        {
            const my_event_t *evt = (const my_event_t *)e;
            process_data(evt->value);
            break;
        }

        default:
            break;
    }
    /* ActiveRT frees the event back to the pool automatically */
}
```

---

## Step 4: Initialise at startup

Call the `INIT` macros **inside a function (like main) BEFORE the FreeRTOS scheduler starts**:

```c
void app_init(void)
{
    ACTIVERT_EVENT_POOL_INIT(my_pool,
                             my_event_t,
                             8,
                             ACTIVERT_POOL_OVERFLOW_DROP);

    ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
}
```

---

## Step 5: Post events

From any task context:

```c
void send_command(uint32_t cmd_value)
{
    my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
    if (evt == NULL) {
        return;  /* pool exhausted, handled by DROP policy */
    }
    evt->base.sig = CMD_SIG;
    evt->value    = cmd_value;
    activert_active_post(my_ao, &evt->base);
    /* do NOT free evt, ActiveRT frees it after dispatch */
}
```

From an ISR:

```c
void my_isr_handler(void)
{
    BaseType_t woken  = pdFALSE;
    my_event_t *evt   = (my_event_t *)activert_event_pool_alloc_from_isr(
                            my_pool, &woken);
    if (evt != NULL) {
        evt->base.sig = DATA_SIG;
        evt->value    = read_peripheral();
        activert_active_post_from_isr(my_ao, &evt->base, &woken);
    }
    portYIELD_FROM_ISR(woken);
}
```

---

## Complete file listing

```c
/* my_ao.h */
#ifndef MY_AO_H
#define MY_AO_H
#include "activert.h"

typedef enum { CMD_SIG = ACTIVERT_USER_SIG, DATA_SIG } my_signal_t;
typedef struct { activert_event_t base; uint32_t value; } my_event_t;

extern activert_event_pool_t *my_pool;
extern activert_active_t     *my_ao;

void app_init(void);
void send_command(uint32_t value);
#endif

/* my_ao.c */
#include "my_ao.h"

ACTIVERT_EVENT_POOL_DEFINE(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(my_ao, my_dispatch, 5, 2048, 8);

static void my_dispatch(activert_active_t *me, const activert_event_t *e)
{
    (void)me;
    switch (e->sig) {
        case ACTIVERT_INIT_SIG: /* startup */ break;
        case CMD_SIG: process_command(((my_event_t *)e)->value); break;
        case DATA_SIG: process_data(((my_event_t *)e)->value);   break;
        default: break;
    }
}

void app_init(void)
{
    ACTIVERT_EVENT_POOL_INIT(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
}

void send_command(uint32_t value)
{
    my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
    if (evt) { evt->base.sig = CMD_SIG; evt->value = value;
               activert_active_post(my_ao, &evt->base); }
}
```
