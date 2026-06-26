# Changelog

All notable changes to ActiveRT are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.1.0] - 2026-06-26

### Added

- Windows / MSVC test support using the FreeRTOS Windows simulator (`MSVC_MINGW` port): platform-aware `test/CMakeLists.txt`, `test/windows_config/FreeRTOSConfig.h`, a `windows-test` CMake preset, and a `windows-latest` CI job. The unit tests now run on Windows in addition to the Linux POSIX simulator.
- `ACTIVERT_QUEUE_SET_STORAGE_BYTES` and `ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES` macros for sizing the queue-set storage buffer. The notification variant includes the extra slot the notification semaphore occupies.
- `ACTIVERT_COMPILER_BARRIER` portable compiler-barrier macro (GCC/Clang/MSVC).
- Regression, dynamic-allocation-path, and multi-Active-Object integration/stress tests.

### Changed

- Event pool thread-safety now uses interrupt-masking critical sections instead of a FreeRTOS mutex, so task-context and ISR-context pool operations mutually exclude. The per-pool mutex storage was removed from `activert_event_pool_t`.
- Documentation corrected: event allocation is non-blocking and the notification value is OR-accumulated, not overwritten. Queue-set storage sizing includes the notification slot, signal-routing rules (catch-all queue, INIT/TERM not routed), ISR alloc/free example signatures, and the statistics getter, monitoring, and health-check function signatures and thresholds.

### Fixed

- Event pool corruption when a pool was shared between task and ISR context: the task path used a mutex and the ISR path a critical section, which did not mutually exclude. Both now use a critical section.
- Notification-only Active Object (`num_queues == 0`) with a non-NULL dispatch handler dereferenced a NULL queue array in the event loop. It now routes to the notification path and asserts a handler is present.
- Notification queue-set storage was one pointer too small, the documented size and the new sizing macros include the `+1` semaphore slot.
- Dynamic Active Object creation (`activert_active_create_dynamic`) wrote through an unallocated queue array. The array is now allocated and freed on every error path.
- Active Objects and pools are unregistered from the statistics registry on destroy, removing a dangling pointer / use-after-free in registry iteration.
- The GNU `__asm__` memory barrier in `activert_post.c`, which broke the MSVC build, was replaced with `ACTIVERT_COMPILER_BARRIER`.
- A double free of a pool event is rejected instead of underflowing `current_allocated`.
- Re-initializing a static pool or Active Object no longer creates duplicate statistics-registry entries.
- `ACTIVERT_POOL_OVERFLOW_DYNAMIC` events are freed after dispatch instead of leaking.
- Health-check stack thresholds are evaluated in bytes (warning < 512, critical < 256) to match the documentation, and queue overflow is flagged only when a post is actually dropped (`posts_failed > 0`). The stack check skips stopped Active Objects (NULL task handle).
- `pool->bitmap_size` is initialized.
- `activert_active_notify(ao, 0)` is delivered on the semaphore path.
- Pool statistics counters are updated inside the critical section.
- `activert_stats_export` writes its header with `memcpy` (no misaligned 32-bit store) and the export buffer no longer requires 4-byte alignment.
- `printf` format specifiers match their argument types, `%zu` was replaced with portable `%u` casts because the MSVCRT `printf` does not support `%zu`.
- `activert_active_post_to_queue` and `activert_active_post_to_queue_from_isr` bounds-check the caller-supplied queue index at runtime and return `-1` for an out-of-range index, rather than relying only on an assert.

---

## [1.0.0] — 2026-02-28

### Added

- Loop task variant (`activert_active_create_loop_static`, `ACTIVERT_ACTIVE_DEFINE_LOOP` / `ACTIVERT_ACTIVE_INIT_LOOP`)
- Platform abstraction macros: `ACTIVERT_MALLOC`, `ACTIVERT_FREE`, `ACTIVERT_ENTER_CRITICAL`, `ACTIVERT_EXIT_CRITICAL`
- `ACTIVERT_ENABLE_CLI` configuration guard (CLI guarded separately from stats)
- FreeRTOS 10.x and 11.x compatibility (`StaticQueueSet_t` detected via version check)
- `activert_active_stop()` — clean AO shutdown via `TERM_SIG`
- `activert_active_get_stack_high_water()` — stack monitoring
- Statistics reset API: `activert_stats_reset_active`, `activert_stats_reset_pool`, `activert_stats_reset_all`
- Telemetry export: `activert_stats_export` / `activert_stats_get_export_size`
- Real-time monitoring callbacks: `activert_stats_monitor_queue_depth`, `activert_stats_monitor_pool_exhaustion`, `activert_stats_monitor_stack_usage`
- `is_static` flag and `activert_static_mem_t` tracking in Active Object struct
- Version macros (`ACTIVERT_VERSION`, `ACTIVERT_VERSION_CHECK`) and convenience aliases (`activert_post`, `activert_notify`) in `activert.h`

---

## [0.7.0] — 2026-01-24

### Added

- Task notification support: semaphore-based and `xTaskNotify`-based hybrid modes
- `activert_active_create_with_notification_static` — AO with queue + notification
- `activert_active_notify` and `activert_active_notify_from_isr` — send notifications from task or ISR
- `activert_notify_handler_t` callback type and `activert_notification_t` struct

---

## [0.6.0] — 2026-01-10

### Added

- CLI command layer (`activert_cli`): `summary`, `list`, `show`, `pool`, `health`, `reset`, `perf`, `report`, `help`
- Health check API (`activert_stats_health_check`) with `ACTIVERT_HEALTH_OK` / `WARNING` / `CRITICAL` levels
- Warning thresholds: queue utilisation >80%, pool exhaustion, drop rate >5%, low stack
- Critical thresholds: queue overflow, pool failure rate >50%, stack overflow risk
- Performance profiling summary: `activert_stats_get_perf_summary`, `find_slowest_active`, `find_busiest_active`
- Formatting helpers: `activert_cli_format_bytes`, `activert_cli_format_percent`

---

## [0.5.0] — 2025-12-27

### Added

- Global statistics registry: `activert_stats_register_active`, `activert_stats_register_pool`
- Per-AO statistics: events processed, events dropped, notifications received
- Per-queue statistics: posts attempted/succeeded/failed, current depth, peak depth
- Per-pool statistics: allocs attempted/succeeded/failed, current allocated, peak allocated
- System-wide totals: `activert_stats_get_total_events_processed`, `get_total_events_dropped`, etc.
- Report generation: `activert_stats_print_summary`, `print_all_actives`, `print_all_pools`, `print_full_report`
- Queue statistics helpers: `activert_queue_get_utilization`, `activert_queue_get_peak_utilization`, `activert_queue_print_stats`

---

## [0.4.0] — 2025-12-13

### Added

- Multi-queue Active Objects with `QueueSetHandle_t` routing
- `activert_queue_config_t` — per-queue signal base, signal count, depth, and pool
- Queue management utilities: `activert_queue_get_depth`, `activert_queue_get_free_space`, `activert_queue_is_full`, `activert_queue_is_empty`, `activert_queue_flush`, `activert_queue_get_config`
- Signal-based routing: events dispatched to queue matching `signal_base` + `signal_count` range

---

## [0.3.0] — 2025-11-29

### Added

- ISR-safe event posting: `activert_active_post_from_isr`
- ISR-safe event pool: `activert_event_pool_alloc_from_isr`, `activert_event_pool_free_from_isr`

---

## [0.2.0] — 2025-11-15

### Added

- Packed bit-per-slot bitmap for event pool allocation (replaced byte-per-slot)
- Overflow policy enforcement: `ACTIVERT_POOL_OVERFLOW_DROP`, `ACTIVERT_POOL_OVERFLOW_ASSERT`, `ACTIVERT_POOL_OVERFLOW_DYNAMIC`
- `activert_event_pool_init_static()` — fully static (zero-heap) pool initialisation

---

## [0.1.0] — 2025-11-01

### Added

- Active Object implementation over FreeRTOS tasks and single-queue dispatch
- `activert_event_t` base event structure with signal and owning-pool pointer
- Event pool with mutex protection and basic bitmap allocation
- Dispatch handler pattern with `INIT_SIG` and `TERM_SIG` lifecycle signals
- `activert_active_post` — post event to Active Object from task context
