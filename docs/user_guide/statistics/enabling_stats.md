# Enabling ActiveRT Statistics

ActiveRT's statistics subsystem is controlled by compile-time macros in
`include/activert_config.h`. All statistics are **enabled by default** and
can be disabled individually to reduce code size and RAM usage in
production builds.

## Configuration Macros

```c
/* include/activert_config.h defaults - override before including */

/* Master switch: global registry and per-component counters */
#define ACTIVERT_ENABLE_STATS        1

/* Per-event processing time measurement (requires ENABLE_STATS=1) */
#define ACTIVERT_ENABLE_TIMING_STATS 1

/* Named AOs and pools (stores const char* pointer per component) */
#define ACTIVERT_ENABLE_NAMES        1
```

Override in your project-level config file before including any ActiveRT
header:

```c
/* myproject_config.h - included before activert.h */
#define ACTIVERT_ENABLE_STATS        1
#define ACTIVERT_ENABLE_TIMING_STATS 0   /* save ~8 bytes per AO */
#define ACTIVERT_ENABLE_NAMES        0   /* save one pointer per component */
#include "activert.h"
```

## Registry Capacity

The stats registry holds up to `ACTIVERT_MAX_REGISTERED_ACTIVES` AOs and
`ACTIVERT_MAX_REGISTERED_POOLS` pools. Default is 32 each. Reduce for
memory-constrained targets:

```c
#define ACTIVERT_MAX_REGISTERED_ACTIVES  8
#define ACTIVERT_MAX_REGISTERED_POOLS    8
```

Each registered AO consumes one pointer slot in a static array
(`sizeof(activert_active_t*) * MAX_REGISTERED_ACTIVES`).

## RAM Cost

When `ACTIVERT_ENABLE_STATS=1`, the additional RAM per component is:

| Per AO | Size |
| --- | --- |
| `stats.events_processed` | 4 bytes |
| `stats.events_dropped` | 4 bytes |
| `stats.notifications_received` | 4 bytes |
| `stats.min/max/total_dispatch_us` | 12 bytes (timing stats only) |

| Per queue (per AO) | Size |
| --- | --- |
| `posts_attempted/succeeded/failed` | 12 bytes |
| `current_depth / peak_depth` | 8 bytes |

| Per pool | Size |
| --- | --- |
| `allocs_attempted/succeeded/failed` | 12 bytes |
| `current_allocated / peak_allocated` | 8 bytes |

For a typical system with 8 AOs (2 queues each) and 4 pools, the total
stats overhead is roughly **400 bytes** which is negligible on most targets.

## Disabling Stats Entirely

Setting `ACTIVERT_ENABLE_STATS 0` removes all stat fields from structs and
replaces all stat-incrementing code with no-ops at compile time. The
`activert_stats_*` API functions are still compiled but become stubs that
return immediately. The CLI layer can still be compiled but will show zero
values for all counters.
