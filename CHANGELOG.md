# Changelog

All notable changes to ActiveRT are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
