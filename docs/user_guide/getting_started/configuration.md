# Configuration

ActiveRT is configured entirely through C preprocessor macros defined in
`activert_config.h`. Every macro is guarded with `#ifndef`, so you can
override any of them **without modifying the library source**.

---

## `activert_user_config.h`

The recommended way to customise ActiveRT is to create a file called
`activert_user_config.h` anywhere on your compiler's include search path.
ActiveRT detects it automatically:

```c
/* activert_config.h */
#if __has_include("activert_user_config.h")
    #include "activert_user_config.h"
#endif
```

This mirrors the `FreeRTOSConfig.h` pattern, your overrides are applied
before any default is set so there is no conflict and no patching required.

**Typical placement:**

```text
my_firmware/
├── include/
│   ├── FreeRTOSConfig.h
│   └── activert_user_config.h   <- add this file
├── src/
└── CMakeLists.txt
```

As long as `include/` is on your compiler's include path (which it must be
for `FreeRTOSConfig.h` anyway) ActiveRT will find `activert_user_config.h`
automatically.

---

## Feature Flags

| Macro | Default | Description |
| --- | --- | --- |
| `ACTIVERT_ENABLE_STATS` | `1` | Global statistics tracking. Disable to reduce RAM and code size in production. |
| `ACTIVERT_ENABLE_TIMING_STATS` | `1` | Per-event dispatch timing. Requires `ACTIVERT_ENABLE_STATS`. |
| `ACTIVERT_ENABLE_NAMES` | `1` | String names for Active Objects and Event Pools. Disable to save memory. |
| `ACTIVERT_ENABLE_DEBUG` | `0` | Verbose debug output via `ACTIVERT_PRINTF`. **Disable for production builds.** |
| `ACTIVERT_ENABLE_DYNAMIC_ALLOCATION` | `0` | Enables `activert_active_create` / `activert_active_destroy`. Off by default. Embedded targets should prefer static allocation. |
| `ACTIVERT_ENABLE_POOL_OVERFLOW_DETECTION` | `0` | Prints a warning via `ACTIVERT_PRINTF` when an event pool is exhausted. Useful during development. |
| `ACTIVERT_ENABLE_CLI` | `0` | Enables the CLI command layer (`activert_cli_cmd_*` functions). When set to `1`, `ACTIVERT_CLI_GET_TOKEN` **must** also be defined. See [CLI Setup](../cli/setup). |

---

## Memory Limits

These control the sizes of internal static arrays used by the statistics
registry. Lower them if RAM is tight; raise them only if your application
genuinely exceeds the defaults.

| Macro | Default | Description |
| --- | --- | --- |
| `ACTIVERT_MAX_QUEUES` | `8` | Maximum number of queues per Active Object. |
| `ACTIVERT_MAX_REGISTERED_ACTIVES` | `32` | Maximum Active Objects tracked by the statistics registry. |
| `ACTIVERT_MAX_REGISTERED_POOLS` | `32` | Maximum Event Pools tracked by the statistics registry. |

---

## Platform Overrides

These macros map ActiveRT's internal I/O and memory operations to your
platform's equivalents. The defaults are FreeRTOS's standard functions,
which are correct for most targets.

| Macro | Default | Description |
| --- | --- | --- |
| `ACTIVERT_PRINTF` | `printf` | Output function used for debug and statistics printing. Redirect to your UART logger if needed. |
| `ACTIVERT_CLI_PRINTF` | `printf` | Output function used by CLI commands. Typically your embedded CLI's print function. |
| `ACTIVERT_CLI_GET_TOKEN(args, n)` | `NULL` | Argument extractor for your embedded CLI library. **Required** when `ACTIVERT_ENABLE_CLI=1`. |

---

## Minimal Example

A typical `activert_user_config.h` for a production firmware build:

```c
#ifndef ACTIVERT_USER_CONFIG_H
#define ACTIVERT_USER_CONFIG_H

/* Trim unused features to reduce code and RAM footprint */
#define ACTIVERT_ENABLE_TIMING_STATS            0
#define ACTIVERT_ENABLE_NAMES                   0
#define ACTIVERT_ENABLE_DEBUG                   0

/* Tighten registry sizes to match actual AO/pool counts */
#define ACTIVERT_MAX_REGISTERED_ACTIVES         8
#define ACTIVERT_MAX_REGISTERED_POOLS           4

/* Route output to the firmware's UART logger */
#define ACTIVERT_PRINTF                         uart_log_printf

#endif /* ACTIVERT_USER_CONFIG_H */
```

```{note}
`activert_user_config.h` is optional. If the file is absent ActiveRT
compiles with the defaults shown above, which are safe and functional
on any FreeRTOS target.
```
