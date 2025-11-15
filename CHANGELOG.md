# Changelog

All notable changes to ActiveRT are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
