# Installation

## Requirements

| Requirement | Version |
| --- | --- |
| FreeRTOS-Kernel | **11.2.0 or later** |
| CMake | 3.16 or later |
| C compiler | C11 (GCC 9+ or Clang 11+) |

The following FreeRTOS configuration options must be enabled in your
`FreeRTOSConfig.h`:

```c
#define configSUPPORT_STATIC_ALLOCATION  1   /* always required */
#define configUSE_MUTEXES                1   /* event pool thread-safety */
#define configUSE_QUEUE_SETS             1   /* multi-queue and notification AOs */
#define configUSE_TASK_NOTIFICATIONS     1   /* notification AOs */
```

---

## Option A: `add_subdirectory` (recommended for embedded projects)

Copy or clone ActiveRT into your project tree, then add it to CMake:

```cmake
# CMakeLists.txt of your firmware project
add_subdirectory(third_party/ActiveRT)

target_link_libraries(my_firmware PRIVATE ActiveRT::activert)
```

Your toolchain's FreeRTOS include paths must already be on the compiler search
path. ActiveRT's `include/` headers include `FreeRTOS.h`, `task.h`, `queue.h`,
and `semphr.h` directly, so these must resolve to your BSP's copies.

---

## Option B: `find_package` (after installing the library)

Build and install ActiveRT once, then consume it from any project:

```bash
cmake -B build/release --preset host-release \
      -DCMAKE_INSTALL_PREFIX=/opt/activert
cmake --build  build/release
cmake --install build/release
```

In your firmware project:

```cmake
find_package(ActiveRT 1.0 CONFIG REQUIRED
             PATHS /opt/activert/lib/cmake/ActiveRT)

target_link_libraries(my_firmware PRIVATE ActiveRT::activert)
```

---

## Option C: Copy the source files directly

For projects that do not use CMake, copy the contents of `include/` and
`src/` into your source tree and add them to your build system. The library
has no code-generation step and no external C dependencies beyond FreeRTOS.
Then, add `include/` to your compiler's include search path.

---

## Verifying the Installation

If you have a Linux build environment available, run the host unit tests
to confirm the build is working:

```bash
cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test --output-on-failure
```

All three test suites should pass. See the
[Testing](../development/testing) page for details.
