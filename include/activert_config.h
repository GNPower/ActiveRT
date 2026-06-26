/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_config.h
*   @brief      Configuration and Platform Abstraction Layer
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial configuration and platform abstraction
*   0.2.0   gnp     2025-11-15  Overflow policy definitions
*   0.5.0   gnp     2025-12-27  Statistics configuration macros
*   0.6.0   gnp     2026-01-10  CLI macros (ACTIVERT_CLI_PRINTF, ACTIVERT_CLI_GET_TOKEN)
*   1.0.0   gnp     2026-02-28  FreeRTOS version compat; ACTIVERT_ENABLE_CLI; stdio.h fix
*
*******************************************************************************/

#ifndef ACTIVERT_CONFIG_H
#define ACTIVERT_CONFIG_H

/* Include project-level overrides before defaults are set.
 * Create activert_user_config.h in your include path to override any macro.
 * This mirrors the FreeRTOSConfig.h pattern. */
#if __has_include("activert_user_config.h")
    #include "activert_user_config.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/*******************************************************************************
* Feature Configuration
*******************************************************************************/

/**
 * Enable global statistics tracking
 */
#ifndef ACTIVERT_ENABLE_STATS
    #define ACTIVERT_ENABLE_STATS 1
#endif

/**
 * Enable timing statistics
 */
#ifndef ACTIVERT_ENABLE_TIMING_STATS
    #define ACTIVERT_ENABLE_TIMING_STATS 1
#endif

/**
 * Enable names for Active Objects and Event Pools
 */
#ifndef ACTIVERT_ENABLE_NAMES
    #define ACTIVERT_ENABLE_NAMES 1
#endif

/**
 * Enable debug features (DISABLE for production builds!)
 */
#ifndef ACTIVERT_ENABLE_DEBUG
    #define ACTIVERT_ENABLE_DEBUG 0
#endif

/**
 * Enable dynamic allocation API (activert_active_create, activert_active_destroy).
 * Disabled by default — embedded targets typically use static allocation only.
 */
#ifndef ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
    #define ACTIVERT_ENABLE_DYNAMIC_ALLOCATION 0
#endif

/**
 * Enable runtime warning when an event pool is exhausted.
 * Disabled by default — production builds should handle exhaustion silently.
 */
#ifndef ACTIVERT_ENABLE_POOL_OVERFLOW_DETECTION
    #define ACTIVERT_ENABLE_POOL_OVERFLOW_DETECTION 0
#endif

/*******************************************************************************
* Memory Configuration
*******************************************************************************/

/**
 * Maximum queues per Active Object
 */
#ifndef ACTIVERT_MAX_QUEUES
    #define ACTIVERT_MAX_QUEUES 8U
#endif

/**
 * Maximum registered Active Objects (for statistics)
 */
#ifndef ACTIVERT_MAX_REGISTERED_ACTIVES
    #define ACTIVERT_MAX_REGISTERED_ACTIVES 32U
#endif

/**
 * Maximum registered Event Pools (for statistics)
 */
#ifndef ACTIVERT_MAX_REGISTERED_POOLS
    #define ACTIVERT_MAX_REGISTERED_POOLS 32U
#endif

/*******************************************************************************
* Assertion Configuration
*******************************************************************************/

/**
 * Assertion macro
 */
#define ACTIVERT_ASSERT(x) configASSERT(x)

/*******************************************************************************
* Signal Definitions
*******************************************************************************/

/**
 * System reserved signals
 */
#define ACTIVERT_INIT_SIG 0  /* Initialization signal */
#define ACTIVERT_TERM_SIG 1  /* Termination signal */
#define ACTIVERT_USER_SIG 16 /* First user signal */

/*******************************************************************************
* Platform Configuration
*******************************************************************************/

/* Critical section macros */
#define ACTIVERT_ENTER_CRITICAL() taskENTER_CRITICAL()
#define ACTIVERT_EXIT_CRITICAL()  taskEXIT_CRITICAL()

/* Compiler memory barrier (prevents the compiler from reordering memory
 * accesses across the barrier). Portable across GCC/Clang and MSVC, expands to
 * nothing on unknown compilers. This is a compiler-only fence, not a CPU fence. */
#if defined(__GNUC__) || defined(__clang__)
    #define ACTIVERT_COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#elif defined(_MSC_VER)
    #include <intrin.h>
    #define ACTIVERT_COMPILER_BARRIER() _ReadWriteBarrier()
#else
    #define ACTIVERT_COMPILER_BARRIER() ((void)0)
#endif

/* Memory allocation */
#define ACTIVERT_MALLOC(size) pvPortMalloc(size)
#define ACTIVERT_FREE(ptr)    vPortFree(ptr)

/* Printf for debug output (stdout) */
#if ACTIVERT_ENABLE_DEBUG || ACTIVERT_ENABLE_STATS
    #ifndef ACTIVERT_PRINTF
        #include <stdio.h>
        #define ACTIVERT_PRINTF printf
    #endif
#else
    #define ACTIVERT_PRINTF(...) ((void)0)
#endif

/**
 * Enable CLI command layer (activert_cli_cmd_* functions).
 *
 * When set to 1 you MUST also define ACTIVERT_CLI_GET_TOKEN(args, n) to
 * match your embedded CLI library's argument extraction API.  Define it
 * before including this header (e.g. in your project-level config file)
 * or in this header below.
 *
 * When set to 0 (default) all CLI commands still compile but argument
 * parsing always returns NULL, which is safe for stats-only builds.
 */
#ifndef ACTIVERT_ENABLE_CLI
    #define ACTIVERT_ENABLE_CLI 0
#endif

/* CLI output - maps to the host CLI system's printf.
 * Override in your project config to redirect to your CLI output function. */
#ifndef ACTIVERT_CLI_PRINTF
    #include <stdio.h>
    #define ACTIVERT_CLI_PRINTF printf
#endif

/* CLI token extraction - maps to the host CLI system's argument parser.
 * When ACTIVERT_ENABLE_CLI=1 this MUST be overridden; a build error is
 * issued if it is not.  When ACTIVERT_ENABLE_CLI=0 it defaults to NULL
 * (CLI commands that require arguments will print their usage string). */
#ifndef ACTIVERT_CLI_GET_TOKEN
    #if ACTIVERT_ENABLE_CLI
        #error "ACTIVERT_ENABLE_CLI=1 requires ACTIVERT_CLI_GET_TOKEN(args, n) to be defined. " \
       "Define it to match your embedded CLI token-extraction API before including activert_config.h."
    #else
        #define ACTIVERT_CLI_GET_TOKEN(args, n) NULL
    #endif
#endif

/*******************************************************************************
* FreeRTOS Version Compatibility
*******************************************************************************/

/*
 * xQueueCreateSetStatic was added in FreeRTOS 11.2.0.
 * For older versions, static queue set allocation is unavailable.
 */
#if !defined(tskKERNEL_VERSION_MAJOR) || (tskKERNEL_VERSION_MAJOR < 11) || \
    (tskKERNEL_VERSION_MAJOR == 11 && tskKERNEL_VERSION_MINOR < 2)
    #error "ActiveRT requires FreeRTOS 11.2.0 or later. Please upgrade your FreeRTOS version."
#endif

#endif /* ACTIVERT_CONFIG_H */
