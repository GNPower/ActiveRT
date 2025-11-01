/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert.h
*   @brief      Master include — single header for all ActiveRT functionality
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    0.1.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial master include
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

#ifdef __cplusplus
}
#endif

#endif /* ACTIVERT_H */
