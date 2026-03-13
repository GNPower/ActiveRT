/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_queue.c
*   @brief      Multi-Queue Utilities Implementation
*   @author     Graham N. Power
*   @date       2025-12-13
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.4.0   gnp     2025-12-13  Initial multi-queue utility functions
*   1.0.0   gnp     2026-02-28  Queue depth, free space, utilisation, and flush
*
*******************************************************************************/

#include "activert_queue.h"
#include "activert_active.h"
#include "activert_event.h"
#include <stdio.h>

/*******************************************************************************
* Queue Management
*******************************************************************************/

UBaseType_t activert_queue_get_depth(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    return uxQueueMessagesWaiting(me->queues[queue_index].handle);
}

UBaseType_t activert_queue_get_free_space(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    return uxQueueSpacesAvailable(me->queues[queue_index].handle);
}

bool activert_queue_is_full(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    return (uxQueueSpacesAvailable(me->queues[queue_index].handle) == 0U);
}

bool activert_queue_is_empty(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    return (uxQueueMessagesWaiting(me->queues[queue_index].handle) == 0U);
}

uint32_t activert_queue_flush(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    uint32_t flushed = 0;
    activert_event_t* event;

    // Receive and discard all events
    while (xQueueReceive(me->queues[queue_index].handle, &event, 0) == pdPASS)
    {
        // Recycle event if it came from a pool
        if (event && event->pool)
        {
            activert_event_pool_free(event);
        }
        flushed++;
    }

#if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
    printf(
        "activert_queue_flush: Flushed %u events from queue %u in task '%s'\n",
        flushed,
        queue_index,
        me->name ? me->name : "unnamed"
    );
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_DEBUG */

    return flushed;
}

int activert_queue_get_config(
    activert_active_t* me, uint8_t queue_index, activert_queue_config_t* config
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(config != NULL);

    if (queue_index >= me->queue_count)
    {
        return -1;
    }

    config->signal_base  = me->queues[queue_index].signal_base;
    config->signal_count = me->queues[queue_index].signal_count;
    config->queue_length = me->queues[queue_index].queue_length;
    config->event_pool   = me->queues[queue_index].event_pool;

    return 0;
}

/*******************************************************************************
* Queue Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

void activert_queue_print_all_stats(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    printf("\n");
    printf("================================================================\n");
    #if ACTIVERT_ENABLE_NAMES
    printf("Queue Statistics: %s\n", me->name ? me->name : "unnamed");
    #else  /* ACTIVERT_ENABLE_NAMES */
    printf("Queue Statistics\n");
    #endif /* ACTIVERT_ENABLE_NAMES */
    printf("================================================================\n");

    for (uint8_t i = 0; i < me->queue_count; i++)
    {
        activert_queue_print_stats(me, i);
    }

    printf("================================================================\n");
}

void activert_queue_print_stats(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    activert_queue_t* queue = &me->queues[queue_index];

    printf("\nQueue %u:\n", queue_index);

    // Signal range
    if (queue->signal_count == 0U)
    {
        printf("  Signals:      Catch-all (any signal)\n");
    }
    else
    {
        printf(
            "  Signals:      %u - %u (%u signals)\n",
            (unsigned)queue->signal_base,
            (unsigned)(queue->signal_base + queue->signal_count - 1U),
            (unsigned)queue->signal_count
        );
    }

    printf("  Length:       %zu events\n", queue->queue_length);

    // Current state
    UBaseType_t current_depth = uxQueueMessagesWaiting(queue->handle);
    UBaseType_t free_space    = uxQueueSpacesAvailable(queue->handle);

    printf(
        "  Current:      %u / %zu (%u%%)\n",
        current_depth,
        queue->queue_length,
        (uint32_t)((current_depth * 100U) / queue->queue_length)
    );

    printf("  Free:         %u\n", free_space);

    // Statistics
    printf(
        "  Posts:        %u (%u OK, %u failed)\n",
        queue->stats.posts_attempted,
        queue->stats.posts_succeeded,
        queue->stats.posts_failed
    );

    if (queue->stats.posts_attempted > 0U)
    {
        float success_rate =
            (float)queue->stats.posts_succeeded * 100.0f / (float)queue->stats.posts_attempted;
        printf("  Success rate: %.2f%%\n", success_rate);
    }

    printf(
        "  Peak:         %u / %zu (%u%%)\n",
        queue->stats.peak_depth,
        queue->queue_length,
        (uint32_t)((queue->stats.peak_depth * 100U) / queue->queue_length)
    );
}

uint8_t activert_queue_get_utilization(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    uint32_t depth  = (uint32_t)uxQueueMessagesWaiting(me->queues[queue_index].handle);
    uint32_t length = (uint32_t)me->queues[queue_index].queue_length;
    uint32_t pct    = (depth * 100U) / length;
    return (uint8_t)pct;
}

uint8_t activert_queue_get_peak_utilization(activert_active_t* me, uint8_t queue_index)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(queue_index < me->queue_count);

    uint32_t peak   = me->queues[queue_index].stats.peak_depth;
    uint32_t length = (uint32_t)me->queues[queue_index].queue_length;
    uint32_t pct    = (peak * 100U) / length;
    return (uint8_t)pct;
}

#endif /* ACTIVERT_ENABLE_STATS */
