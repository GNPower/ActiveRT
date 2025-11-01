# Changelog

All notable changes to ActiveRT are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [0.1.0] — 2025-11-01

### Added

- Active Object implementation over FreeRTOS tasks and single-queue dispatch
- `activert_event_t` base event structure with signal and owning-pool pointer
- Event pool with mutex protection and basic bitmap allocation
- Dispatch handler pattern with `INIT_SIG` and `TERM_SIG` lifecycle signals
- `activert_active_post` — post event to Active Object from task context
