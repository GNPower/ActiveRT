Changelog
=========

v1.0.0 (2026-02-28)
--------------------

- Loop task variant (``activert_active_create_loop_static``, ``ACTIVERT_ACTIVE_DEFINE_LOOP`` / ``ACTIVERT_ACTIVE_INIT_LOOP``)
- Platform abstraction macros: ``ACTIVERT_MALLOC``, ``ACTIVERT_FREE``, ``ACTIVERT_ENTER_CRITICAL``, ``ACTIVERT_EXIT_CRITICAL``
- ``ACTIVERT_ENABLE_CLI`` configuration guard (CLI guarded separately from stats)
- FreeRTOS 10.x and 11.x compatibility (``StaticQueueSet_t`` detected via version check)
- ``activert_active_stop()`` — clean AO shutdown via ``TERM_SIG``
- ``activert_active_get_stack_high_water()`` — stack monitoring
- Statistics reset API: ``activert_stats_reset_active``, ``activert_stats_reset_pool``, ``activert_stats_reset_all``
- Telemetry export: ``activert_stats_export`` / ``activert_stats_get_export_size``
- Real-time monitoring callbacks: ``activert_stats_monitor_queue_depth``, ``activert_stats_monitor_pool_exhaustion``, ``activert_stats_monitor_stack_usage``
- ``is_static`` flag and ``activert_static_mem_t`` tracking in Active Object struct
- Version macros (``ACTIVERT_VERSION``, ``ACTIVERT_VERSION_CHECK``) and convenience aliases in ``activert.h``

v0.7.0 (2026-01-24)
--------------------

- Task notification support: semaphore-based and ``xTaskNotify``-based hybrid modes
- ``activert_active_create_with_notification_static`` — AO with queue + notification
- ``activert_active_notify`` and ``activert_active_notify_from_isr``
- ``activert_notify_handler_t`` callback type and ``activert_notification_t`` struct

v0.6.0 (2026-01-10)
--------------------

- CLI command layer: ``summary``, ``list``, ``show``, ``pool``, ``health``, ``reset``, ``perf``, ``report``, ``help``
- Health check API with ``ACTIVERT_HEALTH_OK`` / ``WARNING`` / ``CRITICAL`` levels
- Warning thresholds: queue utilisation >80%, pool exhaustion, drop rate >5%, low stack
- Critical thresholds: queue overflow, pool failure rate >50%, stack overflow risk
- Performance profiling summary: ``activert_stats_get_perf_summary``, ``find_slowest_active``, ``find_busiest_active``
- Formatting helpers: ``activert_cli_format_bytes``, ``activert_cli_format_percent``

v0.5.0 (2025-12-27)
--------------------

- Global statistics registry: ``activert_stats_register_active``, ``activert_stats_register_pool``
- Per-AO statistics: events processed, events dropped, notifications received
- Per-queue statistics: posts attempted/succeeded/failed, current depth, peak depth
- Per-pool statistics: allocs attempted/succeeded/failed, current allocated, peak allocated
- System-wide totals and report generation functions
- Queue statistics helpers: ``activert_queue_get_utilization``, ``activert_queue_print_stats``

v0.4.0 (2025-12-13)
--------------------

- Multi-queue Active Objects with ``QueueSetHandle_t`` routing
- ``activert_queue_config_t`` — per-queue signal base, signal count, depth, and pool
- Queue management utilities: ``activert_queue_get_depth``, ``activert_queue_get_free_space``, ``activert_queue_is_full``, ``activert_queue_is_empty``, ``activert_queue_flush``, ``activert_queue_get_config``

v0.3.0 (2025-11-29)
--------------------

- ISR-safe event posting: ``activert_active_post_from_isr``
- ISR-safe event pool: ``activert_event_pool_alloc_from_isr``, ``activert_event_pool_free_from_isr``

v0.2.0 (2025-11-15)
--------------------

- Packed bit-per-slot bitmap for event pool allocation
- Overflow policy enforcement: ``ACTIVERT_POOL_OVERFLOW_DROP``, ``ACTIVERT_POOL_OVERFLOW_ASSERT``, ``ACTIVERT_POOL_OVERFLOW_DYNAMIC``
- ``activert_event_pool_init_static()`` — fully static (zero-heap) pool initialisation

v0.1.0 (2025-11-01)
--------------------

- Initial Active Object implementation over FreeRTOS tasks and single-queue dispatch
- ``activert_event_t`` base event structure with signal and owning-pool pointer
- Event pool with mutex protection and basic bitmap allocation
- Dispatch handler pattern with ``INIT_SIG`` and ``TERM_SIG`` lifecycle signals
- ``activert_active_post`` — post event to Active Object from task context
