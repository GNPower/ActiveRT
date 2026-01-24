# Changelog

All notable changes to ActiveRT are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
