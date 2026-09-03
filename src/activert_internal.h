/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_internal.h
*   @brief      Library-private declarations (not public API)
*   @author     Graham N. Power
*   @date       2026-08-30
*   @version    1.2.0
*
*   This header is under src/ and is deliberately not installed with the
*   public headers in include/. It carries declarations shared between library
*   translation units that are not part of the public API.
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   1.2.0   gnp     2026-08-30  Initial private header for post_wait completion helpers
*
*******************************************************************************/

#ifndef ACTIVERT_INTERNAL_H
#define ACTIVERT_INTERNAL_H

#include "activert_types.h"

#if ACTIVERT_ENABLE_POST_WAIT

/**
 * Take ownership of any completion attached to an event
 *
 * Must be called before the event is dispatched or freed. Clears
 * event->completion so the same waiter can't be signalled twice.
 *
 * @param event         Event about to be dispatched or discarded
 * @return              Claimed completion block, or NULL if the event has no
 *                      waiter or the poster already detached on a timeout
 */
activert_completion_t* activert_completion_claim(activert_event_t* event);

/**
 * Release a claimed completion and wake the posting task
 *
 * Safe to call with a NULL completion.
 *
 * @param completion    Block returned by activert_completion_claim()
 * @param dispatched    true if the dispatch handler ran, false if the event was
 *                      discarded without dispatch
 */
void activert_completion_release(activert_completion_t* completion, bool dispatched);

#endif /* ACTIVERT_ENABLE_POST_WAIT */

#endif /* ACTIVERT_INTERNAL_H */
