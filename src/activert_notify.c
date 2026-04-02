/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_notify.c
*   @brief      Task Notification Implementation
*   @author     Graham N. Power
*   @date       2026-01-24
*   @version    1.0.0
*
*   Task notifications provide ultra-fast ISR-to-task communication with
*   minimal overhead. Notifications are checked before queue events in the
*   event loop, ensuring lowest latency for ISR-driven events.
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.7.0   gnp     2026-01-24  Initial task notification (semaphore and xTaskNotify)
*   1.0.0   gnp     2026-02-28  ISR-safe notify; ACTIVERT_ENTER_CRITICAL macros
*
*******************************************************************************/

#include "activert_active.h"
#include <stdio.h>

/*******************************************************************************
* Notification API
*******************************************************************************/

void activert_active_notify(activert_active_t* me, uint32_t notify_bits)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(me->notification.handler != NULL);

    // Hybrid notification strategy:
    // - If semaphore exists (queue+notification task): use semaphore in queue set
    // - Else (notification-only task): use xTaskNotify directly
    if (me->notification.semaphore != NULL)
    {
        // Accumulate notification bits (OR them together) - use critical section for thread safety
        ACTIVERT_ENTER_CRITICAL();
        me->notification.pending_value |= notify_bits;
        ACTIVERT_EXIT_CRITICAL();

        // Give semaphore to wake up task
        xSemaphoreGive(me->notification.semaphore);
    }
    else
    {
        // Use xTaskNotify for notification-only tasks (ultra-lightweight)
        xTaskNotify(me->thread, notify_bits, eSetBits);
    }

#if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
    printf(
        "activert_active_notify: Notified task '%s' with bits 0x%08X\n",
        me->name ? me->name : "unnamed",
        (unsigned int)notify_bits
    );
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_DEBUG */
}

void activert_active_notify_from_isr(
    activert_active_t* me, uint32_t notify_bits, BaseType_t* pxHigherPriorityTaskWoken
)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(me->notification.handler != NULL);

    // Hybrid notification strategy (ISR version):
    // - If semaphore exists (queue+notification task): use semaphore in queue set
    // - Else (notification-only task): use xTaskNotifyFromISR directly
    if (me->notification.semaphore != NULL)
    {
        // Accumulate notification bits (OR them together) - use ISR-safe critical section
        UBaseType_t ux_saved_interrupt_status = taskENTER_CRITICAL_FROM_ISR();
        me->notification.pending_value |= notify_bits;
        taskEXIT_CRITICAL_FROM_ISR(ux_saved_interrupt_status);

        // Give semaphore to wake up task
        xSemaphoreGiveFromISR(me->notification.semaphore, pxHigherPriorityTaskWoken);
    }
    else
    {
        // Use xTaskNotifyFromISR for notification-only tasks (ultra-lightweight)
        xTaskNotifyFromISR(me->thread, notify_bits, eSetBits, pxHigherPriorityTaskWoken);
    }
}

/*******************************************************************************
* Notification Utilities
*******************************************************************************/

/**
 * Set notification mask
 * 
 * Configures which notification bits are valid for this Active Object.
 * Can be used to validate notification bits before posting.
 * 
 * @param me            Active Object
 * @param mask          Valid notification bits mask
 */
void activert_active_set_notify_mask(activert_active_t* me, uint32_t mask)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(me->notification.handler != NULL);

    me->notification.notify_mask = mask;
}

/**
 * Get notification mask
 * 
 * @param me            Active Object
 * @return              Current notification mask
 */
uint32_t activert_active_get_notify_mask(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    return me->notification.notify_mask;
}

/**
 * Check if Active Object has notification support
 * 
 * @param me            Active Object
 * @return              true if notification handler is configured
 */
bool activert_active_has_notification(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    return (me->notification.handler != NULL);
}

/*******************************************************************************
* Notification Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

/**
 * Get notification count
 * 
 * @param me            Active Object
 * @return              Total notifications received
 */
uint32_t activert_active_get_notification_count(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    return me->stats.notifications_received;
}

/**
 * Print notification statistics
 * 
 * @param me            Active Object
 */
void activert_active_print_notification_stats(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    if (!me->notification.handler)
    {
        printf("Task has no notification handler\n");
        return;
    }

    printf("================================================================\n");
    #if ACTIVERT_ENABLE_NAMES
    printf("Notification Statistics: %s\n", me->name ? me->name : "unnamed");
    #else  /* ACTIVERT_ENABLE_NAMES */
    printf("Notification Statistics\n");
    #endif /* ACTIVERT_ENABLE_NAMES */
    printf("================================================================\n");
    printf("Notifications received: %u\n", me->stats.notifications_received);
    printf("Notification mask:      0x%08X\n", (unsigned int)me->notification.notify_mask);

    if (me->stats.events_processed > 0U)
    {
        uint32_t total_events = me->stats.events_processed + me->stats.notifications_received;
        float notify_ratio = (float)me->stats.notifications_received * 100.0F / (float)total_events;
        printf("Notification ratio:     %.1f%% (vs queue events)\n", notify_ratio);
    }

    printf("================================================================\n");
}

#endif /* ACTIVERT_ENABLE_STATS */
