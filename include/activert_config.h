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
#define ACTIVERT_ENABLE_STATS           1
#endif

/**
 * Enable timing statistics
 */
#ifndef ACTIVERT_ENABLE_TIMING_STATS
#define ACTIVERT_ENABLE_TIMING_STATS    1
#endif

/**
 * Enable names for Active Objects and Event Pools
 */
#ifndef ACTIVERT_ENABLE_NAMES
#define ACTIVERT_ENABLE_NAMES           1
#endif

/**
 * Enable debug features (DISABLE for production builds!)
 */
#ifndef ACTIVERT_ENABLE_DEBUG
#define ACTIVERT_ENABLE_DEBUG           0
#endif

/*******************************************************************************
* Memory Configuration
*******************************************************************************/

/**
 * Maximum queues per Active Object
 */
#ifndef ACTIVERT_MAX_QUEUES
#define ACTIVERT_MAX_QUEUES             8
#endif

/**
 * Maximum registered Active Objects (for statistics)
 */
#ifndef ACTIVERT_MAX_REGISTERED_ACTIVES
#define ACTIVERT_MAX_REGISTERED_ACTIVES 32
#endif

/**
 * Maximum registered Event Pools (for statistics)
 */
#ifndef ACTIVERT_MAX_REGISTERED_POOLS
#define ACTIVERT_MAX_REGISTERED_POOLS   32
#endif

/*******************************************************************************
* Assertion Configuration
*******************************************************************************/

/**
 * Assertion macro
 */
#define ACTIVERT_ASSERT(x)              configASSERT(x)

/*******************************************************************************
* Signal Definitions
*******************************************************************************/

/**
 * System reserved signals
 */
#define ACTIVERT_INIT_SIG               0   /* Initialization signal */
#define ACTIVERT_TERM_SIG               1   /* Termination signal */
#define ACTIVERT_USER_SIG               16  /* First user signal */

/*******************************************************************************
* Platform Configuration
*******************************************************************************/

/* Critical section macros */
#define ACTIVERT_ENTER_CRITICAL()       taskENTER_CRITICAL()
#define ACTIVERT_EXIT_CRITICAL()        taskEXIT_CRITICAL()

/* Memory allocation */
#define ACTIVERT_MALLOC(size)           pvPortMalloc(size)
#define ACTIVERT_FREE(ptr)              vPortFree(ptr)

/* Printf for debug output (stdout) */
#if ACTIVERT_ENABLE_DEBUG || ACTIVERT_ENABLE_STATS
#include <stdio.h>
#define ACTIVERT_PRINTF                 printf
#else
#define ACTIVERT_PRINTF(...)            ((void)0)
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
#define ACTIVERT_ENABLE_CLI             0
#endif

/* CLI output - maps to the host CLI system's printf.
 * Override in your project config to redirect to your CLI output function. */
#ifndef ACTIVERT_CLI_PRINTF
#include <stdio.h>
#define ACTIVERT_CLI_PRINTF             printf
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
 * StaticQueueSet_t was added in FreeRTOS 11.x
 * For older versions, we can't use static allocation for queue sets
 */
#if !defined(tskKERNEL_VERSION_MAJOR) || (tskKERNEL_VERSION_MAJOR < 11)
    #error "ActiveRT requires FreeRTOS 11.x or later for static queue set support. Please upgrade your FreeRTOS version."
#endif

#endif /* ACTIVERT_CONFIG_H */
