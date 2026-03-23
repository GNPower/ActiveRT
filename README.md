# ActiveRT

[![CI](https://github.com/GNPower/ActiveRT/actions/workflows/ci.yml/badge.svg)](https://github.com/GNPower/ActiveRT/actions/workflows/ci.yml)
[![Static Analysis](https://github.com/GNPower/ActiveRT/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/GNPower/ActiveRT/actions/workflows/static-analysis.yml)
[![Docs](https://readthedocs.org/projects/activert/badge/?version=dev)](https://activert.readthedocs.io/en/dev/)

**ActiveRT** is a lightweight C framework that implements the
[Active Object design pattern](https://en.wikipedia.org/wiki/Active_object)
for FreeRTOS-based embedded systems.

Each Active Object is a self-contained FreeRTOS task that processes events
from one or more queues. The pattern eliminates shared-state concurrency bugs
by ensuring that all state data belonging to an object is only accessed by its own
task; no mutexes, no race conditions.

Detailed documentation can be found at [ReadTheDocs](https://activert.readthedocs.io).

---

## Features

| Feature | Description |
| --- | --- |
| **Static allocation** | Zero-heap operation — all buffers provided by the caller |
| **Event pools** | Bitmap-based allocation with `DROP`, `ASSERT`, or `DYNAMIC` overflow policies |
| **Multi-queue AOs** | Up to 8 queues per Active Object with signal-based routing |
| **Task notifications** | Lightweight ISR -> task signalling without event allocation |
| **Statistics** | Per-component counters, peak usage, processing time, health checks |
| **CLI layer** | Runtime diagnostics via any embedded CLI system |
| **ISR-safe APIs** | `_from_isr` variants for all post/free/notify operations |
| **FreeRTOS 11** | Compatible with FreeRTOS 11.2+ |

---

## Architecture Overview

```text
ISR / Task
    |
    v  activert_event_pool_alloc()
 Event Pool ---- bitmap allocation, mutex-protected
    |
    v  activert_active_post()
 Queue Set ------ fair priority scheduling across queues
    |
    v  FreeRTOS task (one per Active Object)
 Dispatch Handler -- user-defined switch/state machine
    |
    v  activert_event_pool_free()  (automatic after dispatch)
 Event Pool ---- event returned to pool
```

---

## Quick Start

```c
#include "activert.h"

/* Signal definitions */
enum { CMD_SIG = ACTIVERT_USER_SIG, DATA_SIG };

/* Application event type */
typedef struct {
    activert_event_t base;   /* Must be first */
    uint32_t         value;
} my_event_t;

/* Static event pool (8 events, drop on overflow) */
ACTIVERT_EVENT_POOL_DEFINE(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);

/* Static Active Object (priority 5, 2 KB stack, 8-deep queue) */
ACTIVERT_ACTIVE_DEFINE_SIMPLE(my_ao, my_dispatch, 5, 2048, 8);

/* Dispatch handler */
void my_dispatch(activert_active_t *me, const activert_event_t *e)
{
    (void)me;
    my_event_t *evt = (my_event_t *)e;
    switch (e->sig) {
        case ACTIVERT_INIT_SIG: /* startup init */            break;
        case CMD_SIG:           process_command(evt->value); break;
        case DATA_SIG:          process_data(evt->value);    break;
    }
}

/* Initialise in main / RTOS startup hook */
void app_init(void)
{
    ACTIVERT_EVENT_POOL_INIT(my_pool, my_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(my_ao, my_dispatch, 5);
}

/* Post an event from anywhere */
void send_command(uint32_t cmd)
{
    my_event_t *evt = (my_event_t *)activert_event_pool_alloc(my_pool);
    if (evt) {
        evt->base.sig = CMD_SIG;
        evt->value    = cmd;
        activert_active_post(my_ao, &evt->base);
    }
}
```

---

## Building

### Embedded integration (your CMake project)

```cmake
# Option A: add_subdirectory
add_subdirectory(third_party/ActiveRT)
target_link_libraries(my_firmware PRIVATE ActiveRT::activert)

# Option B: find_package (after cmake --install)
find_package(ActiveRT CONFIG REQUIRED)
target_link_libraries(my_firmware PRIVATE ActiveRT::activert)
```

> **Note:** ActiveRT headers directly include FreeRTOS headers (`FreeRTOS.h`,
> `task.h`, `queue.h`, `semphr.h`). Your embedded toolchain or BSP must supply
> these — ActiveRT does not bundle FreeRTOS. The library target will not compile
> standalone; it is always built as part of an embedded project that provides
> FreeRTOS in its include path.

### Host unit tests (Linux / macOS)

Uses the FreeRTOS POSIX simulator, no hardware required.

```bash
cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test
```

### Documentation

```bash
pip install -r requirements.txt
cmake --preset docs
cmake --build --preset docs
# Open build/docs/html/index.html
```

### Static analysis (local)

```bash
# clang-format check
find src include -name "*.c" -o -name "*.h" | \
    xargs clang-format --dry-run -Werror

# cppcheck
cppcheck --enable=warning,style,performance,portability \
         --std=c11 --inline-suppr -I include/ src/

# clang-tidy (requires cmake --preset host-test first)
find src -name "*.c" | xargs clang-tidy -p build/host-test
```

---

## Configuration

Key options in `include/activert_config.h`:

| Macro | Default | Description |
| --- | --- | --- |
| `ACTIVERT_ENABLE_STATS` | `1` | Global statistics registry |
| `ACTIVERT_ENABLE_TIMING_STATS` | `1` | Per-event processing time tracking |
| `ACTIVERT_ENABLE_NAMES` | `1` | Named AOs and pools for debugging |
| `ACTIVERT_ENABLE_DEBUG` | `0` | Verbose debug printf output |
| `ACTIVERT_ENABLE_CLI` | `0` | CLI command layer (requires `ACTIVERT_CLI_GET_TOKEN` override) |
| `ACTIVERT_MAX_QUEUES` | `8` | Max queues per Active Object |
| `ACTIVERT_MAX_REGISTERED_ACTIVES` | `32` | Global AO registry capacity |
| `ACTIVERT_MAX_REGISTERED_POOLS` | `32` | Global pool registry capacity |

---

## Additional Resources

Full API reference is published at the project's GitHub Pages site.

- [CHANGELOG.md](CHANGELOG.md)

---

## License

Copyright 2025-2026 Graham N. Power. All rights reserved.
