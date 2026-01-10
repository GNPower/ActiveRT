/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert.h
*   @brief      Master include — single header for all ActiveRT functionality
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    0.6.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial master include
*   0.4.0   gnp     2025-12-13  Added activert_queue.h
*   0.5.0   gnp     2025-12-27  Added activert_stats.h
*   0.6.0   gnp     2026-01-10  Added activert_cli.h (guarded by ACTIVERT_ENABLE_CLI)
*
*******************************************************************************/

#ifndef ACTIVERT_H
#define ACTIVERT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration - must be first */
#include "activert_config.h"

/* Type definitions */
#include "activert_types.h"

/* Event Pool API */
#include "activert_event.h"

/* Active Object API (includes posting functions) */
#include "activert_active.h"

/* Optional: CLI Commands */
#if ACTIVERT_ENABLE_CLI
#include "activert_cli.h"
#endif

/* Optional: Statistics API */
#if ACTIVERT_ENABLE_STATS
#include "activert_stats.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* ACTIVERT_H */
