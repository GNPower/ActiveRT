/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_post.c
*   @brief      Event Posting Implementation
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial event posting with basic queue send
*   0.3.0   gnp     2025-11-29  ISR-safe post variant
*   0.4.0   gnp     2025-12-13  Multi-queue routing by signal range
*   1.0.0   gnp     2026-02-28  activert_active_post returns int (0=ok, -1=fail)
*
*******************************************************************************/

#include "activert_active.h"
#include <stdio.h>

/*******************************************************************************
* Internal Helper Functions
*******************************************************************************/

/**
 * Find queue index for signal-based routing
 * 
 * @param me            Active Object
 * @param signal        Event signal
 * @return              Queue index, or -1 if no matching queue
 */
static int find_queue_for_signal(activert_active_t* me, activert_signal_t signal)
{
    int catch_all_queue = -1;

    for (uint8_t i = 0; i < me->queue_count; i++)
    {
        // Check if this is a catch-all queue (signal_count == 0)
        if (me->queues[i].signal_count == 0U)
        {
            catch_all_queue = i;
            continue;
        }

        // Check if signal is in this queue's range
        if ((signal >= me->queues[i].signal_base) &&
            (signal < (me->queues[i].signal_base + me->queues[i].signal_count)))
        {
            return i;
        }
    }

    // No specific queue found, use catch-all if available
    return catch_all_queue;
}

/**
 * Update queue depth statistics
 *
 * @param queue         Queue to update
 */
#if ACTIVERT_ENABLE_STATS
static void update_queue_depth_stats(activert_queue_t* queue)
{
    UBaseType_t depth          = uxQueueMessagesWaiting(queue->handle);
    queue->stats.current_depth = depth;

    if (depth > queue->stats.peak_depth)
    {
        queue->stats.peak_depth = depth;
    }
}
#endif /* ACTIVERT_ENABLE_STATS */

/*******************************************************************************
* Event Posting - Normal Context
*******************************************************************************/

int activert_active_post(activert_active_t* me, activert_event_t* event)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(event != NULL);

    // Find queue based on signal
    int queue_idx = find_queue_for_signal(me, event->sig);

    if (queue_idx < 0)
    {
#if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
        printf(
            "activert_active_post: No queue for signal %u in task '%s'\n",
            event->sig,
            me->name ? me->name : "unnamed"
        );
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_DEBUG */

#if ACTIVERT_ENABLE_STATS
        me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */

        return -1;  // No queue handles this signal
    }

    return activert_active_post_to_queue(me, queue_idx, event);
}

int activert_active_post_to_queue(
    activert_active_t* me, uint8_t queue_index, activert_event_t* event
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(event != NULL);

    // Runtime bounds guard on the caller-supplied queue index. Unlike a bare
    // ACTIVERT_ASSERT (compiled out in release builds), this prevents indexing
    // past me->queues in every build. An out-of-range index is a failed post.
    if (queue_index >= me->queue_count)
    {
#if ACTIVERT_ENABLE_STATS
        me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */
        return -1;
    }

#if ACTIVERT_ENABLE_STATS
    me->queues[queue_index].stats.posts_attempted++;
#endif /* ACTIVERT_ENABLE_STATS */

    // Post event to queue (non-blocking)
    // Queue stores activert_event_t*, so send the pointer value, not its address
    BaseType_t status = xQueueSendToBack(me->queues[queue_index].handle, (const void*)&event, 0);
    ACTIVERT_COMPILER_BARRIER();  // Portable compiler barrier (GCC/Clang/MSVC)

    if (status == pdPASS)
    {
#if ACTIVERT_ENABLE_STATS
        me->queues[queue_index].stats.posts_succeeded++;
        update_queue_depth_stats(&me->queues[queue_index]);
#endif /* ACTIVERT_ENABLE_STATS */

        return 0;
    }

    // Queue full
#if ACTIVERT_ENABLE_STATS
    me->queues[queue_index].stats.posts_failed++;
    me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */

    return -1;
}

/*******************************************************************************
* Event Posting - ISR Context
*******************************************************************************/

int activert_active_post_from_isr(
    activert_active_t* me, activert_event_t* event, BaseType_t* pxHigherPriorityTaskWoken
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(event != NULL);

    // Find queue based on signal
    int queue_idx = find_queue_for_signal(me, event->sig);

    if (queue_idx < 0)
    {
#if ACTIVERT_ENABLE_STATS
        me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */
        return -1;
    }

    return activert_active_post_to_queue_from_isr(me, queue_idx, event, pxHigherPriorityTaskWoken);
}

int activert_active_post_to_queue_from_isr(
    activert_active_t* me,
    uint8_t queue_index,
    activert_event_t* event,
    BaseType_t* pxHigherPriorityTaskWoken
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(event != NULL);

    // Runtime bounds guard on the caller-supplied queue index.
    // An out-of-range index is a failed post, not out-of-bounds access.
    if (queue_index >= me->queue_count)
    {
#if ACTIVERT_ENABLE_STATS
        me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */
        return -1;
    }

#if ACTIVERT_ENABLE_STATS
    me->queues[queue_index].stats.posts_attempted++;
#endif /* ACTIVERT_ENABLE_STATS */

    // Post event to queue from ISR
    BaseType_t status = xQueueSendToBackFromISR(
        me->queues[queue_index].handle, (const void*)&event, pxHigherPriorityTaskWoken
    );

    if (status == pdPASS)
    {
#if ACTIVERT_ENABLE_STATS
        me->queues[queue_index].stats.posts_succeeded++;
// Note: Can't safely call uxQueueMessagesWaiting from ISR
// Queue depth stats won't be updated from ISR posts
#endif /* ACTIVERT_ENABLE_STATS */

        return 0;
    }

#if ACTIVERT_ENABLE_STATS
    me->queues[queue_index].stats.posts_failed++;
    me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */

    return -1;
}
