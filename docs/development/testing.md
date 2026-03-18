# Testing

ActiveRT uses a two-layer test strategy:

- **Host unit tests** - run on Linux via the FreeRTOS POSIX simulator;
  exercised in CI on every push.
- **Static analysis** - clang-format, clang-tidy, and cppcheck / MISRA-C
  checks; also run in CI on every push.

---

## Running Host Unit Tests

The host tests require a Linux environment (the FreeRTOS POSIX port uses
pthreads). On Ubuntu/Debian:

```bash
sudo apt-get install -y cmake ninja-build

cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test --output-on-failure
```

Expected output:

```text
Test #1: test_event_pool    ... Passed
Test #2: test_active_basic  ... Passed
Test #3: test_stats_registry ... Passed

100% tests passed, 0 tests failed out of 3
```

---

## Test Architecture

All host tests share
[test/common/freertos_test_main.c](../../test/common/freertos_test_main.c),
which starts the FreeRTOS POSIX scheduler, creates a Unity runner task,
and exits with Unity's failure count as the process exit code.

Each test file defines exactly one function:

```c
void run_tests(void);
```

`run_tests` is called from within a live FreeRTOS task, so all FreeRTOS
APIs are fully operational.

### File layout

```text
test/
├── CMakeLists.txt             - test build rules
├── common/
│   └── freertos_test_main.c   - shared scheduler + Unity entry point
├── posix_config/
│   └── FreeRTOSConfig.h       - FreeRTOS config for the POSIX port
├── platform_stubs/
│   └── embedded_cli.h         - stub for MISRA analysis only
└── unit/
    ├── test_event_pool.c
    ├── test_active_basic.c
    └── test_stats_registry.c
```

---

## Adding a New Unit Test

1. Create `test/unit/test_my_feature.c`:

```c
#include "unity.h"
#include "activert.h"

void setUp(void)    { /* reset state */ }
void tearDown(void) { /* clean up    */ }

void test_my_case(void)
{
    TEST_ASSERT_EQUAL(expected, actual);
}

void run_tests(void)
{
    RUN_TEST(test_my_case);
}
```

2. Register in `test/CMakeLists.txt`:

```cmake
add_activert_test(test_my_feature
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_my_feature.c")
```

3. Rebuild and run: `cmake --build --preset host-test && ctest --preset host-test`

---

## Static Analysis

### clang-format

```bash
# Check (dry-run, fails on violations)
find src include -name "*.c" -o -name "*.h" | \
    xargs clang-format --dry-run -Werror

# Auto-fix
find src include -name "*.c" -o -name "*.h" | \
    xargs clang-format -i
```

Style rules are in [`.clang-format`](../../.clang-format) — Allman braces,
4-space indent, 100-column limit.

### clang-tidy

```bash
cmake --preset host-test          # generates compile_commands.json
cmake --build --preset host-test

find src -name "*.c" | xargs clang-tidy -p build/host-test
```

Check configuration is in [`.clang-tidy`](../../.clang-tidy).

### cppcheck / MISRA-C 2012

```bash
python3 tools/misra/run_misra_check.py
```

All MISRA-C deviations are documented with rationale in
[misra](misra).
