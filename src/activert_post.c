/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_post.c
*   @brief      Event Posting Implementation
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.2.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial event posting with basic queue send
*   0.3.0   gnp     2025-11-29  ISR-safe post variant
*   0.4.0   gnp     2025-12-13  Multi-queue routing by signal range
*   1.0.0   gnp     2026-02-28  activert_active_post returns int (0=ok, -1=fail)
*   1.2.0   gnp     2026-08-30  Disabled-AO post protection, synchronous 
*                               activert_active_post_wait
*
*******************************************************************************/

#include "activert_active.h"
#include "activert_internal.h"
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

/**
 * Queue send shared by the plain and the synchronous posts
 *
 * @param me            Active Object
 * @param queue_index   Queue index
 * @param event         Event to post
 * @return              0 on success, -1 on failure
 */
static int post_to_queue_inner(activert_active_t* me, uint8_t queue_index, activert_event_t* event)
{
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

    // A disabled Active Object cannot accept posts.
    if (!me->enabled)
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

int activert_active_post_to_queue(
    activert_active_t* me, uint8_t queue_index, activert_event_t* event
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(event != NULL);

#if ACTIVERT_ENABLE_POST_WAIT
    event->completion = NULL;
#endif /* ACTIVERT_ENABLE_POST_WAIT */

    return post_to_queue_inner(me, queue_index, event);
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

#if ACTIVERT_ENABLE_POST_WAIT
    event->completion = NULL;
#endif /* ACTIVERT_ENABLE_POST_WAIT */

    // Runtime bounds guard on the caller-supplied queue index.
    // An out-of-range index is a failed post, not out-of-bounds access.
    if (queue_index >= me->queue_count)
    {
#if ACTIVERT_ENABLE_STATS
        me->stats.events_dropped++;
#endif /* ACTIVERT_ENABLE_STATS */
        return -1;
    }

    // A disabled Active Object accepts nothing, even from an ISR.
    if (!me->enabled)
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

/*******************************************************************************
* Synchronous Event Posting
*******************************************************************************/

#if ACTIVERT_ENABLE_POST_WAIT

activert_completion_t* activert_completion_claim(activert_event_t* event)
{
    activert_completion_t* completion;

    ACTIVERT_ENTER_CRITICAL();
    completion = event->completion;
    if (completion != NULL)
    {
        completion->state = ACTIVERT_COMPLETION_CLAIMED;
        event->completion = NULL;
    }
    ACTIVERT_EXIT_CRITICAL();

    return completion;
}

void activert_completion_release(activert_completion_t* completion, bool dispatched)
{
    if (completion == NULL)
    {
        return;
    }

    ACTIVERT_ENTER_CRITICAL();
    completion->state = dispatched ? ACTIVERT_COMPLETION_DONE : ACTIVERT_COMPLETION_DISCARDED;
    ACTIVERT_EXIT_CRITICAL();

    // Given outside the critical section: the block is already CLAIMED, so the
    // poster cannot return and cannot delete the semaphore before this runs.
    (void)xSemaphoreGive(completion->sem);
}

/**
 * Map a signalled completion block to a public return code
 *
 * @param completion    Completion block that has already been signalled
 * @return              ACTIVERT_POST_WAIT_OK or ACTIVERT_POST_WAIT_DROPPED
 */
static int completion_result(const activert_completion_t* completion)
{
    return (completion->state == ACTIVERT_COMPLETION_DISCARDED) ? ACTIVERT_POST_WAIT_DROPPED
                                                                : ACTIVERT_POST_WAIT_OK;
}

/**
 * Post an event and block until the Active Object is finished with it
 *
 * @param me            Target Active Object
 * @param queue_index   Queue index to post to
 * @param event         Event to post
 * @param timeout       Ticks to wait for dispatch to begin
 * @return              One of the ACTIVERT_POST_WAIT_* codes
 */
static int post_wait_common(
    activert_active_t* me, uint8_t queue_index, activert_event_t* event, TickType_t timeout
)
{
    activert_completion_t completion;
    int rc;

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        return ACTIVERT_POST_WAIT_FAILED;
    }
    if (me->thread == xTaskGetCurrentTaskHandle())
    {
        return ACTIVERT_POST_WAIT_FAILED;
    }

    completion.sem = xSemaphoreCreateBinaryStatic(&completion.sem_buf);
    if (completion.sem == NULL)
    {
        return ACTIVERT_POST_WAIT_FAILED;
    }
    completion.state = ACTIVERT_COMPLETION_PENDING;

    // Attach before queueing. A higher priority Active Object can dequeue and
    // dispatch the event before this function reaches the wait below.
    //
    // cppcheck flags storing the address of a local into a longer-lived object.
    // But here, either the Active Object claims the block, or the timeout path
    // removes it under a critical section.
    // See the invariants in src/activert_internal.h.
    // cppcheck-suppress autoVariables
    event->completion = &completion;

    if (post_to_queue_inner(me, queue_index, event) != 0)
    {
        event->completion = NULL;
        vSemaphoreDelete(completion.sem);
        return ACTIVERT_POST_WAIT_FAILED;  // caller still owns the event
    }

    if (xSemaphoreTake(completion.sem, timeout) == pdTRUE)
    {
        rc = completion_result(&completion);
    }
    else
    {
        bool claimed;

        ACTIVERT_ENTER_CRITICAL();
        claimed = (completion.state != ACTIVERT_COMPLETION_PENDING);
        if (!claimed)
        {
            // Still PENDING, so the Active Object has not claimed the event,
            // and thus has not dispatched or freed it so this pointer is
            // still valid. Detach so it never signals a block that is about to
            // go out of scope.
            event->completion = NULL;
        }
        ACTIVERT_EXIT_CRITICAL();

        if (claimed)
        {
            // Dispatch is under way and the release is already committed to
            // giving the semaphore. Deleting the block now would be a
            // use-after-free, so wait it out.
            (void)xSemaphoreTake(completion.sem, portMAX_DELAY);
            rc = completion_result(&completion);
        }
        else
        {
            rc = ACTIVERT_POST_WAIT_TIMEOUT;  // event still queued, do not free it
        }
    }

    vSemaphoreDelete(completion.sem);
    return rc;
}

int activert_active_post_wait(activert_active_t* me, activert_event_t* event, TickType_t timeout)
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

        return ACTIVERT_POST_WAIT_FAILED;  // No queue handles this signal
    }

    return post_wait_common(me, (uint8_t)queue_idx, event, timeout);
}

int activert_active_post_to_queue_wait(
    activert_active_t* me, uint8_t queue_index, activert_event_t* event, TickType_t timeout
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(event != NULL);

    return post_wait_common(me, queue_index, event, timeout);
}

#endif /* ACTIVERT_ENABLE_POST_WAIT */
