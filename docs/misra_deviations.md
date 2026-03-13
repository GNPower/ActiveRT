# MISRA-C 2012 Deviation Record - ActiveRT

This document records all deviations from MISRA-C 2012 rules in the
ActiveRT source code, along with the rationale for each deviation.

Deviations fall into two categories:

- **Inline** - suppressed at the specific source location with a
  `/* cppcheck-suppress misra-c2012-X.Y */` comment.
- **Global** - suppressed project-wide in `tools/misra/run_misra_check.py`
  because the rule conflicts with a deliberate project-wide design choice or
  produces only false positives on this codebase.

---

## Inline Deviations

### Rule 11.5 - Cast from `void*` to a pointer-to-object type

| Field | Detail |
| --- | --- |
| **Locations** | `src/activert_active.c` - `activert_task_fn()` and `activert_loop_task_fn()` (`void* pvParameters` -> `activert_active_t*`); `src/activert_event.c` - `get_event_at_index()` (`void*` pool storage -> `activert_event_t*`); `activert_event_pool_create()` (`ACTIVERT_MALLOC` return -> typed pointers); `get_event_index()` (`void*` pool storage -> `uint8_t*`) |
| **Category** | Advisory |
| **Rationale** | Two contexts require `void*`-to-object-pointer casts: (1) The FreeRTOS task API passes the task argument as `void* pvParameters`; ActiveRT always registers task functions with an `activert_active_t*` argument so the cast is type-safe. No alternative exists without modifying the FreeRTOS API contract. (2) The event pool stores raw bytes as `void*` to remain type-agnostic; indexing into the pool requires byte-level arithmetic via `uint8_t*`, then casting the result back through `void*` to the typed event pointer. This is the standard MISRA-C pattern for pool allocators and eliminates the more serious Rule 11.3 (Required) violation that would arise from casting directly between two object pointer types. |

---

### Rule 11.3 - Cast between pointer to different object types

| Field | Detail |
| --- | --- |
| **Locations** | `src/activert_event.c` - `get_event_index()` (`const activert_event_t*` -> `const uint8_t*`); `src/activert_stats.c` - `activert_stats_export()` (`uint8_t*` buffer -> `uint32_t*` for direct field serialisation) |
| **Category** | Required |
| **Rationale** | Two contexts: (1) Pool range comparison - determining whether an event pointer falls within a pool's memory range requires a `const uint8_t*` view of the typed event pointer for byte-level comparison. The `const` qualifier is preserved. No standard C alternative exists without casting to a character-type pointer. (2) Stats serialisation - writing `uint32_t` fields directly into a caller-supplied `uint8_t*` buffer via a `uint32_t*` cast. The buffer is documented to require 4-byte alignment; the cast avoids per-field `memcpy` overhead. A union or byte-by-byte approach would introduce byte-order sensitivity or verbosity without a safety benefit in this non-safety-critical diagnostic path. |

---

### Rule 18.4 - Pointer arithmetic with `+`, `-`, `+=`, `-=`

| Field | Detail |
| --- | --- |
| **Locations** | `src/activert_event.c` - `get_event_index()` (pointer subtraction `evt_ptr - base`) |
| **Category** | Advisory |
| **Rationale** | Computing the byte distance between two pointers into the same array is the only standard C mechanism for determining an event's index within a pool. Array-subscript notation (`&base[n]`) is used everywhere an address is computed from a base pointer plus offset, but pointer subtraction has no array-subscript equivalent and cannot be avoided here. |

---

### Rule 18.5 - More than two levels of pointer nesting

| Field | Detail |
| --- | --- |
| **Locations** | `include/activert_active.h` - declarations of `activert_active_create_static()` and `activert_active_create_with_notification_static()`; `src/activert_active.c` - corresponding definitions |
| **Category** | Advisory |
| **Rationale** | The static-allocation multi-queue API requires the caller to supply an array of per-queue event-pointer buffers. This is represented as `activert_queue_storage_t*` (typedef for `activert_event_t**`), yielding three logical levels of indirection: `event_t` -> per-event pointer array -> per-queue array -> outer array pointer. The typedef reduces the syntactic depth to two levels, but cppcheck resolves through typedefs and still flags it. The three levels of indirection are inherent to the static-allocation design and cannot be reduced without sacrificing type safety or requiring dynamic allocation. |

---

### Rule 20.7 - Macro parameters not enclosed in parentheses

| Field | Detail |
| --- | --- |
| **Locations** | `include/activert_active.h` - `ACTIVERT_ACTIVE_DEFINE_SIMPLE()`, `ACTIVERT_ACTIVE_DEFINE_LOOP()`, `ACTIVERT_ACTIVE_INIT_SIMPLE()`, `ACTIVERT_ACTIVE_INIT_LOOP()`; `include/activert_event.h` - `ACTIVERT_EVENT_POOL_DEFINE()`, `ACTIVERT_EVENT_POOL_INIT()` |
| **Category** | Required |
| **Rationale** | Three distinct cases arise across these macros where Rule 20.7 parenthesisation is either inapplicable or not possible: (1) **Declaration identifiers** - parameters such as `task_name` and `pool_name` are used as C declarator names (`static activert_active_t* task_name = NULL`) or assignment targets. A declarator name is not an expression in the C grammar; enclosing it in parentheses would either be a syntax error or change the declaration's meaning. (2) **Type names in `sizeof`** - `event_type` is a type name passed to `sizeof(event_type)`. The `sizeof(type)` form requires the type to appear directly inside parentheses; an additional outer pair would cause a parse error on some compilers. (3) **Function-pointer / value arguments** - parameters such as `dispatch_fn`, `loop_fn`, and `prio` are parenthesised at their call-site uses where they appear as function arguments. All arithmetic parameters (`stack_sz`, `queue_len`, `count`, `policy`) are parenthesised at every expression use site within the macro bodies. cppcheck flags the entire macro definition regardless of partial compliance. |

---

---

## Global Deviations

### Rule 2.5 - Unused macro declarations

| Field | Detail |
| --- | --- |
| **Scope** | All public headers |
| **Category** | Advisory |
| **Rationale** | ActiveRT is a library. Public convenience macros such as `ACTIVERT_ACTIVE_DEFINE_SIMPLE()`, `ACTIVERT_ACTIVE_INIT_SIMPLE()`, and the event pool equivalents are part of the public API provided for downstream user code. cppcheck analyses only the library's own source files and has no visibility into user applications, so it incorrectly reports these macros as unused. Suppressing this rule globally is the standard approach for library projects. |

---

### Rule 8.7 - External linkage only referenced in one translation unit

| Field | Detail |
| --- | --- |
| **Scope** | All source files |
| **Category** | Required |
| **Rationale** | ActiveRT is a library. All public API functions (e.g., `activert_active_post`, `activert_stats_print_summary`, `activert_event_pool_alloc`) are declared in public headers and intentionally carry external linkage so that downstream user applications can call them. cppcheck analyses only the library's own `.c` files and has no visibility into user application code; it therefore incorrectly concludes that each public function is "only referenced in one translation unit." Every flagged function is declared in a public header and forms part of the documented API surface. Suppressing this rule globally is the standard approach for library projects, consistent with the Rule 2.5 rationale. |

---

### Rule 15.5 - A function shall have a single point of exit

| Field | Detail |
| --- | --- |
| **Scope** | All source files |
| **Category** | Advisory |
| **Rationale** | Early-return guard clauses are used at the top of several functions to validate preconditions. Restructuring these as nested `if`/`else` chains would reduce readability without improving correctness. All early returns are assertion-equivalent checks for invalid state that cannot occur in a correctly integrated system. |

---

### Rule 17.7 - The value of a function call shall be used

| Field | Detail |
| --- | --- |
| **Scope** | All source files |
| **Category** | Required |
| **Rationale** | Several FreeRTOS API calls (e.g., `xQueueSend`, `xSemaphoreTake`) are invoked in contexts where failure is either impossible given runtime invariants or is already guarded by a prior `ACTIVERT_ASSERT`. Casting every return value to `(void)` would add noise without improving safety on an RTOS target that does not recover from queue errors. |

---

### Rule 20.10 - The `#` and `##` preprocessor operators should not be used

| Field | Detail |
| --- | --- |
| **Scope** | `include/activert_active.h`, `include/activert_event.h` - all `DEFINE` and `INIT` convenience macros |
| **Category** | Advisory |
| **Rationale** | The `##` token-paste operator is the only standard C mechanism for generating unique per-object variable names within a single macro expansion (e.g., `task_name##_stack`, `pool_name##_bitmap`). Without it, the static-allocation convenience macros cannot exist and each user would have to declare all storage variables manually. The `#` stringification operator is used to pass the object name as a `const char*` to FreeRTOS `xTaskCreate`, which requires it for debug visibility. Both uses are deliberate and well-isolated to the public convenience macro layer; they do not appear in the library implementation sources. |

---

### Rule 21.6 - Standard library `<stdio.h>` shall not be used

| Field | Detail |
| --- | --- |
| **Scope** | All source files |
| **Category** | Required |
| **Rationale** | `<stdio.h>` functions (`printf`, `fprintf`) are used only when `ACTIVERT_ENABLE_DEBUG=1` or `ACTIVERT_ENABLE_CLI=1`, both of which can be disabled in production builds if desired. The CLI output macro `cli_printf` is remapped to the platform's serial output in the embedded BSP; the `printf` expansion is used only for host-side testing. |

---

### Rule 21.15 - `memcmp`, `memcpy`, `memmove` pointer types shall match

| Field | Detail |
| --- | --- |
| **Scope** | All source files |
| **Category** | Required |
| **Rationale** | This rule is suppressed globally because cppcheck raises false positives on FreeRTOS internal headers included transitively during analysis. No calls to `memcmp`/`memcpy`/`memmove` in ActiveRT source violate this rule; the suppression exists solely to prevent noise from the FreeRTOS include chain. |

---

### Rules 22.8 / 22.9 - `errno` not zeroed / not checked around `strtoul`

| Field | Detail |
| --- | --- |
| **Scope** | `src/activert_cli.c` - `find_active()` and `find_pool()` |
| **Category** | Advisory |
| **Rationale** | `strtoul` is classified as an errno-setting function; MISRA requires `errno` to be zeroed before the call (22.8) and tested for zero after (22.9). In both call sites validity is fully determined by `*endptr == '\0'`, which is the standard embedded pattern for checking whether the entire input string was consumed as a number. Introducing `errno = 0` / `errno != 0` checks would require `<errno.h>` on a bare-metal target and adds no safety benefit since the `endptr` test already distinguishes a valid parse from an invalid CLI argument. `strtoul` is the only errno-setting function used anywhere in ActiveRT; the global suppression covers no other call sites. |
