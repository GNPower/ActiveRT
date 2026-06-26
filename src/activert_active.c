/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_active.c
*   @brief      Active Object Implementation
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial Active Object with single-queue FreeRTOS task
*   0.4.0   gnp     2025-12-13  Multi-queue support with QueueSet routing
*   0.5.0   gnp     2025-12-27  Statistics integration and per-queue tracking
*   0.7.0   gnp     2026-01-24  Task notification support (semaphore and xTaskNotify)
*   1.0.0   gnp     2026-02-28  Loop task variant, ACTIVERT_MALLOC/ENTER_CRITICAL macros
*
*******************************************************************************/

#include "activert_active.h"
#include "activert_event.h"
#include "activert_stats.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
* Functions Declarations
*******************************************************************************/

/*******************************************************************************
* Internal Helper Functions
*******************************************************************************/

/**
 * Active Object event loop (FreeRTOS task function)
 * 
 * This is the heart of the Active Object pattern.
 * Priority order:
 * 1. Task notifications (if enabled) - checked first, non-blocking
 * 2. Queue events - fair scheduling via queue set
 */
static void activert_active_event_loop(void* pvParameters)
{
    /* MISRA-C 2012 Rule 11.5 deviation: FreeRTOS task API requires void* for
     * pvParameters. The cast is safe as this function is only registered as a
     * task with an activert_active_t* argument in activert_active_start(). */
    // cppcheck-suppress misra-c2012-11.5
    activert_active_t* me = (activert_active_t*)pvParameters;

    ACTIVERT_ASSERT(me != NULL);

#if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
    printf("activert_active_event_loop: Starting task '%s'\n", me->name ? me->name : "unnamed");
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_DEBUG */

    // Send INIT_SIG to dispatch handler (if it exists)
    if (me->dispatch != NULL)
    {
        activert_event_t init_event = {.sig = ACTIVERT_INIT_SIG, .pool = NULL};
        me->dispatch(me, &init_event);
    }

    // Main event loop
    while (1)
    {
        /*************************************************************
        * Task Notifications (if enabled)
        *************************************************************/
        if (me->notification.handler != NULL)
        {
            uint32_t notify_bits = 0U;

            // Non-blocking check for notifications
            if (xTaskNotifyWait(0U, 0xFFFFFFFFU, &notify_bits, 0U) == pdPASS)
            {
#if ACTIVERT_ENABLE_STATS
                me->stats.notifications_received++;
#endif /* ACTIVERT_ENABLE_STATS */

                // Call notification handler
                me->notification.handler(me, notify_bits);

                // Continue to check for more notifications before processing queue
                continue;
            }
        }

        /*************************************************************
        * Queue Events (fair scheduling)
        *************************************************************/

        // Queue-less task (notification-only): block on notifications regardless
        // of whether a dispatch handler is set. A dispatch handler on a queue-less
        // AO only receives the startup INIT_SIG and there is no queue to receive from,
        // so falling through to the queue-receive path below would dereference the
        // NULL me->queues. A queue-less AO MUST have a notification handler.
        if (me->queue_count == 0U)
        {
            ACTIVERT_ASSERT(me->notification.handler != NULL);

            uint32_t notify_bits;
            xTaskNotifyWait(0U, 0xFFFFFFFFU, &notify_bits, portMAX_DELAY);

#if ACTIVERT_ENABLE_STATS
            me->stats.notifications_received++;
#endif /* ACTIVERT_ENABLE_STATS */

            me->notification.handler(me, notify_bits);
            continue;
        }

        QueueSetMemberHandle_t active_queue;
        activert_event_t* event = NULL;

        // Timeout logic:
        // - If notification semaphore in queue set: use portMAX_DELAY (true blocking, no polling)
        // - Else if notification handler but no semaphore: use 10ms timeout (polling for xTaskNotify)
        // - Else: use portMAX_DELAY (no notifications)
        TickType_t queue_timeout = portMAX_DELAY;
        if (me->notification.handler && !me->notification.semaphore)
        {
            queue_timeout = pdMS_TO_TICKS(10);  // Polling needed for xTaskNotify
        }

        if (me->queue_set != NULL)
        {
            // Multiple queues - wait on queue set (may include notification semaphore)
            active_queue = xQueueSelectFromSet(me->queue_set, queue_timeout);

            // If timeout, continue to check notifications
            if (active_queue == NULL)
            {
                continue;
            }

            // Check if this is the notification semaphore
            if ((me->notification.semaphore) && (active_queue == me->notification.semaphore))
            {
                // Take semaphore (clear it)
                xSemaphoreTake(me->notification.semaphore, 0);

                // Get pending notification value (use critical section for thread safety).
                // 'was_pending' is cleared here so a leftover queue-set token (more
                // tokens than handler dispatches) does not invoke the handler, while a
                // real notify with value 0 still does.
                ACTIVERT_ENTER_CRITICAL();
                uint32_t notify_value          = me->notification.pending_value;
                bool was_pending               = me->notification.pending;
                me->notification.pending_value = 0;  // Clear pending value
                me->notification.pending       = false;
                ACTIVERT_EXIT_CRITICAL();

                // Call notification handler (also for a value of 0, when pending)
                if ((me->notification.handler != NULL) && was_pending)
                {
                    me->notification.handler(me, notify_value);

#if ACTIVERT_ENABLE_STATS
                    me->stats.notifications_received++;
#endif /* ACTIVERT_ENABLE_STATS */
                }

                // Continue to next iteration
                continue;
            }

            // It's a queue - receive from it
            if (xQueueReceive(active_queue, (void*)&event, 0) != pdPASS)
            {
                // Shouldn't happen - queue set said this queue had data
                continue;
            }
        }
        else
        {
            // Single queue - receive directly
            if (xQueueReceive(me->queues[0].handle, (void*)&event, queue_timeout) != pdPASS)
            {
                // Timeout or error - continue to check notifications
                continue;
            }
        }

        // Validate event pointer
        if (event == NULL)
        {
#if ACTIVERT_ENABLE_DEBUG
            printf("activert_active_event_loop: Received NULL event!\n");
#endif /* ACTIVERT_ENABLE_DEBUG */
            continue;
        }

#if ACTIVERT_ENABLE_TIMING_STATS
        TickType_t start_time = xTaskGetTickCount();
#endif /* ACTIVERT_ENABLE_TIMING_STATS */

        // Dispatch event
        me->dispatch(me, event);

#if ACTIVERT_ENABLE_STATS
        me->stats.events_processed++;

    #if ACTIVERT_ENABLE_TIMING_STATS
        TickType_t processing_time = xTaskGetTickCount() - start_time;
        me->stats.total_processing_time += processing_time;

        if (processing_time > me->stats.max_processing_time)
        {
            me->stats.max_processing_time = processing_time;
            me->stats.slowest_signal      = event->sig;
        }
    #endif /* ACTIVERT_ENABLE_TIMING_STATS */
#endif     /* ACTIVERT_ENABLE_STATS */

        // Auto-recycle the dispatched event. activert_event_pool_free handles
        // both pool events (returned to the pool) and ACTIVERT_POOL_OVERFLOW_DYNAMIC
        // events (event->pool == NULL, freed with vPortFree).
        activert_event_pool_free(event);
    }
}

/**
 * Create queue for Active Object
 * 
 * @param queue         Queue structure to initialize
 * @param config        Queue configuration
 * @param queue_storage Pre-allocated queue storage (static) or NULL (dynamic)
 * @param queue_cb      Pre-allocated queue control block (static) or NULL (dynamic)
 * @return              0 on success, -1 on error
 */
static int create_queue(
    activert_queue_t* queue,
    const activert_queue_config_t* config,
    activert_event_t** queue_storage,
    StaticQueue_t* queue_cb
)
{
    // Copy configuration
    queue->signal_base  = config->signal_base;
    queue->signal_count = config->signal_count;
    queue->queue_length = config->queue_length;
    queue->event_pool   = config->event_pool;

    // Create queue
    if (queue_storage && queue_cb)
    {
        // Static allocation
        queue->handle = xQueueCreateStatic(
            config->queue_length, sizeof(activert_event_t*), (uint8_t*)queue_storage, queue_cb
        );
    }
    else
    {
// Dynamic allocation
#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
        queue->handle = xQueueCreate(config->queue_length, sizeof(activert_event_t*));
#else  /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */
        return -1;  // Dynamic allocation disabled
#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */
    }

    if (queue->handle == NULL)
    {
        return -1;
    }

// Initialize statistics
#if ACTIVERT_ENABLE_STATS
    memset(&queue->stats, 0, sizeof(queue->stats));
#endif /* ACTIVERT_ENABLE_STATS */

    return 0;
}

/*******************************************************************************
* Active Object Creation - Static
*******************************************************************************/

/**
 * Internal helper for creating Active Objects with static allocation
 * This is called by both activert_active_create_static() and
 * activert_active_create_with_notification_static()
 *
 * All memory is caller-provided (truly static, zero heap allocation).
 *
 * @param active_storage        Caller-provided activert_active_t struct
 * @param queue_structs         Caller-provided array of activert_queue_t (num_queues elements)
 * @param notification_handler  Optional notification handler (NULL if not used)
 * @param notify_sem_cb         Optional semaphore storage for notifications (NULL if not used)
 */
static activert_active_t* activert_active_create_static_internal(
    activert_active_t* active_storage,
    activert_queue_t* queue_structs,
    const char* name,
    activert_dispatch_handler_t dispatch,
    activert_notify_handler_t notification_handler,
    StaticSemaphore_t* notify_sem_cb,
    UBaseType_t priority,
    StackType_t* stack,
    size_t stack_size,
    StaticTask_t* task_cb,
    activert_queue_config_t* queue_configs,
    uint8_t num_queues,
    StaticQueue_t* queue_cbs,
    /* cppcheck-suppress misra-c2012-18.5
                                        * Deviation: activert_queue_storage_t* is a pointer to a
                                        * caller-provided array of event-pointer arrays. The three
                                        * levels of indirection (event_t**, per-queue array*, outer
                                        * array*) are inherent to the static-allocation API design
                                        * and cannot be reduced without losing type safety. */
    activert_queue_storage_t* queue_storages,
    StaticQueue_t* queue_set_cb,
    uint8_t* queue_set_storage
)
{
    // Validate parameters
    ACTIVERT_ASSERT(active_storage != NULL);
    ACTIVERT_ASSERT(queue_structs != NULL);
    ACTIVERT_ASSERT(dispatch != NULL);
    ACTIVERT_ASSERT(stack != NULL);
    ACTIVERT_ASSERT(stack_size > 0U);
    ACTIVERT_ASSERT(task_cb != NULL);
    ACTIVERT_ASSERT(queue_configs != NULL);
    ACTIVERT_ASSERT((num_queues > 0U) && (num_queues <= ACTIVERT_MAX_QUEUES));
    ACTIVERT_ASSERT(queue_cbs != NULL);
    ACTIVERT_ASSERT(queue_storages != NULL);
    ACTIVERT_ASSERT((num_queues == 1U) || (queue_set_cb != NULL));

    // Use caller-provided storage (zero heap allocation)
    activert_active_t* me = active_storage;
    memset(me, 0, sizeof(activert_active_t));

    me->queues = queue_structs;
    memset(me->queues, 0, num_queues * sizeof(activert_queue_t));

#if ACTIVERT_ENABLE_NAMES
    me->name = name;
#endif /* ACTIVERT_ENABLE_NAMES */

    me->dispatch                     = dispatch;
    me->priority                     = priority;
    me->queue_count                  = num_queues;
    me->is_static                    = true;
    me->static_mem.thread_cb         = task_cb;
    me->static_mem.queue_cbs         = queue_cbs;
    me->static_mem.queue_set_cb      = queue_set_cb;
    me->static_mem.queue_set_storage = queue_set_storage;

    // Set notification handler before creating task so it's ready when task starts
    me->notification.handler = notification_handler;

    // Create notification semaphore if handler exists and semaphore storage provided
    // (This is for queue+notification tasks - notification-only tasks use xTaskNotify)
    if (notification_handler && notify_sem_cb)
    {
        me->notification.semaphore     = xSemaphoreCreateBinaryStatic(notify_sem_cb);
        me->notification.semaphore_cb  = notify_sem_cb;
        me->notification.pending_value = 0;
        if (me->notification.semaphore == NULL)
        {
            return NULL;
        }

        // Queue set is required when using notification semaphore with queues
        // (even for single queue - we need to wait on both queue and semaphore)
        ACTIVERT_ASSERT(queue_set_cb != NULL);
        ACTIVERT_ASSERT(queue_set_storage != NULL);
    }
    else
    {
        me->notification.semaphore     = NULL;
        me->notification.semaphore_cb  = NULL;
        me->notification.pending_value = 0;
    }

    // Create queues
    for (uint8_t i = 0; i < num_queues; i++)
    {
        if (create_queue(&me->queues[i], &queue_configs[i], queue_storages[i], &queue_cbs[i]) != 0)
        {
            // Cleanup on failure
            for (uint8_t j = 0; j < i; j++)
            {
                vQueueDelete(me->queues[j].handle);
            }
            return NULL;
        }
    }

    // Create queue set if multiple queues OR if we have notification semaphore
    // (Semaphore needs to be in queue set to wait on both queue and notifications simultaneously)
    if ((num_queues > 1U) || (me->notification.semaphore != NULL))
    {
        // Calculate total queue set size (queue lengths + 1 for semaphore if present)
        size_t queue_set_size = 0;
        for (uint8_t i = 0; i < num_queues; i++)
        {
            queue_set_size += queue_configs[i].queue_length;
        }
        if (me->notification.semaphore != NULL)
        {
            queue_set_size += 1U;  // Binary semaphore counts as 1 slot
        }

        me->queue_set = xQueueCreateSetStatic(queue_set_size, queue_set_storage, queue_set_cb);
        if (me->queue_set == NULL)
        {
            // Cleanup
            for (uint8_t i = 0; i < num_queues; i++)
            {
                vQueueDelete(me->queues[i].handle);
            }
            return NULL;
        }

        // Add queues to set
        for (uint8_t i = 0; i < num_queues; i++)
        {
            if (xQueueAddToSet(me->queues[i].handle, me->queue_set) != pdPASS)
            {
                // Cleanup
                vQueueDelete(me->queue_set);
                for (uint8_t j = 0; j < num_queues; j++)
                {
                    vQueueDelete(me->queues[j].handle);
                }
                return NULL;
            }
        }

        // Add notification semaphore to queue set if it exists
        // This allows the event loop to wait on both queues and notifications simultaneously
        if (me->notification.semaphore != NULL)
        {
            if (xQueueAddToSet(me->notification.semaphore, me->queue_set) != pdPASS)
            {
                // Cleanup
                vQueueDelete(me->queue_set);
                for (uint8_t j = 0; j < num_queues; j++)
                {
                    vQueueDelete(me->queues[j].handle);
                }
                return NULL;
            }
        }
    }
    else
    {
        me->queue_set = NULL;
    }

    // Create task
    size_t stack_depth = stack_size / sizeof(StackType_t);

    me->thread = xTaskCreateStatic(
        activert_active_event_loop,
#if ACTIVERT_ENABLE_NAMES
        name ? name : "ActiveRT",
#else  /* ACTIVERT_ENABLE_NAMES */
        "ActiveRT",
#endif /* ACTIVERT_ENABLE_NAMES */
        stack_depth,
        me,  // Pass Active Object as parameter
        priority + tskIDLE_PRIORITY,
        stack,
        task_cb
    );

    if (me->thread == NULL)
    {
        // Cleanup
        if (me->queue_set != NULL)
        {
            vQueueDelete(me->queue_set);
        }
        for (uint8_t i = 0; i < num_queues; i++)
        {
            vQueueDelete(me->queues[i].handle);
        }
        return NULL;
    }

#if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
    printf(
        "activert_active_create_static_internal: Created Active Object '%s'\n",
        name ? name : "unnamed"
    );
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_DEBUG */

#if ACTIVERT_ENABLE_STATS
    activert_stats_register_active(me);
#endif /* ACTIVERT_ENABLE_STATS */

    return me;
}

/**
 * Public API: Create Active Object without notifications
 */
activert_active_t* activert_active_create_static(
    const char* name,
    activert_dispatch_handler_t dispatch,
    UBaseType_t priority,
    StackType_t* stack,
    size_t stack_size,
    StaticTask_t* task_cb,
    activert_queue_config_t* queue_configs,
    uint8_t num_queues,
    StaticQueue_t* queue_cbs,
    /* cppcheck-suppress misra-c2012-18.5
                                                  * Deviation: see activert_active_create_static. */
    activert_queue_storage_t* queue_storages,
    StaticQueue_t* queue_set_cb,
    uint8_t* queue_set_storage,
    activert_active_t* active_storage,
    activert_queue_t* queue_structs
)
{
    return activert_active_create_static_internal(
        active_storage,
        queue_structs,
        name,
        dispatch,
        NULL,  // No notification handler
        NULL,  // No semaphore
        priority,
        stack,
        stack_size,
        task_cb,
        queue_configs,
        num_queues,
        queue_cbs,
        queue_storages,
        queue_set_cb,
        queue_set_storage
    );
}

activert_active_t* activert_active_create_with_notification_static(
    const char* name,
    activert_dispatch_handler_t dispatch,
    activert_notify_handler_t notification_handler,
    UBaseType_t priority,
    StackType_t* stack,
    size_t stack_size,
    StaticTask_t* task_cb,
    activert_queue_config_t* queue_configs,
    uint8_t num_queues,
    StaticQueue_t* queue_cbs,
    /* cppcheck-suppress misra-c2012-18.5
                                                 * Deviation: see activert_active_create_static. */
    activert_queue_storage_t* queue_storages,
    StaticQueue_t* queue_set_cb,
    uint8_t* queue_set_storage,
    StaticSemaphore_t* notify_sem_cb,
    activert_active_t* active_storage,
    activert_queue_t* queue_structs
)
{
    // If no queues, create minimal Active Object (notification-only)
    // These use xTaskNotify directly, not semaphores
    if (num_queues == 0U)
    {
        ACTIVERT_ASSERT(active_storage != NULL);
        ACTIVERT_ASSERT(notification_handler != NULL);

        // Use caller-provided storage (zero heap allocation)
        activert_active_t* me = active_storage;
        memset(me, 0, sizeof(activert_active_t));

#if ACTIVERT_ENABLE_NAMES
        me->name = name;
#endif /* ACTIVERT_ENABLE_NAMES */

        me->dispatch             = dispatch;  // Can be NULL for notification-only
        me->priority             = priority;
        me->queue_count          = 0;
        me->is_static            = true;
        me->static_mem.thread_cb = task_cb;
        me->notification.handler = notification_handler;

        // Create task
        size_t stack_depth = stack_size / sizeof(StackType_t);

        me->thread = xTaskCreateStatic(
            activert_active_event_loop,
#if ACTIVERT_ENABLE_NAMES
            name ? name : "ActiveRT",
#else  /* ACTIVERT_ENABLE_NAMES */
            "ActiveRT",
#endif /* ACTIVERT_ENABLE_NAMES */
            stack_depth,
            me,
            priority + tskIDLE_PRIORITY,
            stack,
            task_cb
        );

        if (me->thread == NULL)
        {
            return NULL;
        }

#if ACTIVERT_ENABLE_STATS
        activert_stats_register_active(me);
#endif /* ACTIVERT_ENABLE_STATS */

        return me;
    }

    // Create with queues and notification handler
    // Call internal function to set notification handler BEFORE task starts
    return activert_active_create_static_internal(
        active_storage,
        queue_structs,
        name,
        dispatch,
        notification_handler,
        notify_sem_cb,
        priority,
        stack,
        stack_size,
        task_cb,
        queue_configs,
        num_queues,
        queue_cbs,
        queue_storages,
        queue_set_cb,
        queue_set_storage
    );
}

/*******************************************************************************
* Loop Task - Static
*******************************************************************************/

/**
 * Loop task runner (FreeRTOS task function)
 *
 * Sends INIT_SIG to dispatch handler (if provided), then loops calling the
 * user's loop function forever. No queues or notifications.
 */
static void activert_active_loop_runner(void* pvParameters)
{
    /* MISRA-C 2012 Rule 11.5 deviation: FreeRTOS task API requires void* for
     * pvParameters. The cast is safe as this function is only registered as a
     * task with an activert_active_t* argument in activert_active_start(). */
    // cppcheck-suppress misra-c2012-11.5
    activert_active_t* me = (activert_active_t*)pvParameters;

    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(me->loop_fn != NULL);

    // Send INIT_SIG to dispatch handler (if provided)
    if (me->dispatch != NULL)
    {
        activert_event_t init_event = {.sig = ACTIVERT_INIT_SIG, .pool = NULL};
        me->dispatch(me, &init_event);
    }

    // Infinite loop calling user function
    while (1)
    {
        me->loop_fn(me);

#if ACTIVERT_ENABLE_STATS
        me->stats.events_processed++;
#endif /* ACTIVERT_ENABLE_STATS */
    }
}

/**
 * Create a loop task with static allocation (zero heap)
 */
activert_active_t* activert_active_create_loop_static(
    const char* name,
    activert_dispatch_handler_t dispatch,
    activert_loop_fn_t loop_fn,
    UBaseType_t priority,
    StackType_t* stack,
    size_t stack_size,
    StaticTask_t* task_cb,
    activert_active_t* active_storage
)
{
    ACTIVERT_ASSERT(active_storage != NULL);
    ACTIVERT_ASSERT(loop_fn != NULL);
    ACTIVERT_ASSERT(stack != NULL);
    ACTIVERT_ASSERT(stack_size > 0U);
    ACTIVERT_ASSERT(task_cb != NULL);

    activert_active_t* me = active_storage;
    memset(me, 0, sizeof(activert_active_t));

#if ACTIVERT_ENABLE_NAMES
    me->name = name;
#else  /* ACTIVERT_ENABLE_NAMES */
    (void)name;
#endif /* ACTIVERT_ENABLE_NAMES */

    me->dispatch             = dispatch;
    me->loop_fn              = loop_fn;
    me->priority             = priority;
    me->queue_count          = 0;
    me->queues               = NULL;
    me->is_static            = true;
    me->static_mem.thread_cb = task_cb;

    size_t stack_depth = stack_size / sizeof(StackType_t);

    me->thread = xTaskCreateStatic(
        activert_active_loop_runner,
#if ACTIVERT_ENABLE_NAMES
        name ? name : "ActiveRT",
#else  /* ACTIVERT_ENABLE_NAMES */
        "ActiveRT",
#endif /* ACTIVERT_ENABLE_NAMES */
        stack_depth,
        me,
        priority + tskIDLE_PRIORITY,
        stack,
        task_cb
    );

    if (me->thread == NULL)
    {
        return NULL;
    }

#if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
    printf("activert_active_create_loop_static: Created loop task '%s'\n", name ? name : "unnamed");
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_DEBUG */

#if ACTIVERT_ENABLE_STATS
    activert_stats_register_active(me);
#endif /* ACTIVERT_ENABLE_STATS */

    return me;
}

/*******************************************************************************
* Active Object Creation - Dynamic
*******************************************************************************/

#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION

activert_active_t* activert_active_create_dynamic(
    const char* name,
    activert_dispatch_handler_t dispatch,
    UBaseType_t priority,
    size_t stack_size,
    activert_queue_config_t* queue_configs,
    uint8_t num_queues
)
{
    ACTIVERT_ASSERT(dispatch != NULL);
    ACTIVERT_ASSERT(stack_size > 0);
    ACTIVERT_ASSERT(queue_configs != NULL);
    ACTIVERT_ASSERT(num_queues > 0 && num_queues <= ACTIVERT_MAX_QUEUES);

    // Allocate Active Object structure
    activert_active_t* me = (activert_active_t*)ACTIVERT_MALLOC(sizeof(activert_active_t));
    if (me == NULL)
    {
        return NULL;
    }

    memset(me, 0, sizeof(activert_active_t));

    #if ACTIVERT_ENABLE_NAMES
    me->name = name;
    #endif

    me->dispatch    = dispatch;
    me->priority    = priority;
    me->queue_count = num_queues;
    me->is_static   = false;

    // Allocate the queue array (the static path receives it from the caller.
    // The dynamic path must allocate it before writing through me->queues[i]).
    me->queues = (activert_queue_t*)ACTIVERT_MALLOC((size_t)num_queues * sizeof(activert_queue_t));
    if (me->queues == NULL)
    {
        ACTIVERT_FREE(me);
        return NULL;
    }
    memset(me->queues, 0, (size_t)num_queues * sizeof(activert_queue_t));

    // Create queues
    for (uint8_t i = 0; i < num_queues; i++)
    {
        if (create_queue(&me->queues[i], &queue_configs[i], NULL, NULL) != 0)
        {
            // Cleanup
            for (uint8_t j = 0; j < i; j++)
            {
                vQueueDelete(me->queues[j].handle);
            }
            ACTIVERT_FREE(me->queues);
            ACTIVERT_FREE(me);
            return NULL;
        }
    }

    // Create queue set if needed
    if (num_queues > 1)
    {
        size_t queue_set_size = 0;
        for (uint8_t i = 0; i < num_queues; i++)
        {
            queue_set_size += queue_configs[i].queue_length;
        }

        me->queue_set = xQueueCreateSet(queue_set_size);
        if (me->queue_set == NULL)
        {
            for (uint8_t i = 0; i < num_queues; i++)
            {
                vQueueDelete(me->queues[i].handle);
            }
            ACTIVERT_FREE(me->queues);
            ACTIVERT_FREE(me);
            return NULL;
        }

        for (uint8_t i = 0; i < num_queues; i++)
        {
            xQueueAddToSet(me->queues[i].handle, me->queue_set);
        }
    }

    // Create task
    if (xTaskCreate(
            activert_active_event_loop,
    #if ACTIVERT_ENABLE_NAMES
            name ? name : "ActiveRT",
    #else  /* ACTIVERT_ENABLE_NAMES */
            "ActiveRT",
    #endif /* ACTIVERT_ENABLE_NAMES */
            stack_size / sizeof(StackType_t),
            me,
            priority + tskIDLE_PRIORITY,
            &me->thread
        ) != pdPASS)
    {
        if (me->queue_set)
        {
            vQueueDelete(me->queue_set);
        }
        for (uint8_t i = 0; i < num_queues; i++)
        {
            vQueueDelete(me->queues[i].handle);
        }
        ACTIVERT_FREE(me->queues);
        ACTIVERT_FREE(me);
        return NULL;
    }

    #if ACTIVERT_ENABLE_STATS
    activert_stats_register_active(me);
    #endif /* ACTIVERT_ENABLE_STATS */

    return me;
}

activert_active_t* activert_active_create_with_notification_dynamic(
    const char* name,
    activert_dispatch_handler_t dispatch,
    activert_notify_handler_t notification_handler,
    UBaseType_t priority,
    size_t stack_size,
    activert_queue_config_t* queue_configs,
    uint8_t num_queues
)
{
    activert_active_t* me;

    if (num_queues == 0)
    {
        ACTIVERT_ASSERT(notification_handler != NULL);

        me = (activert_active_t*)ACTIVERT_MALLOC(sizeof(activert_active_t));
        if (me == NULL)
        {
            return NULL;
        }

        memset(me, 0, sizeof(activert_active_t));

    #if ACTIVERT_ENABLE_NAMES
        me->name = name;
    #endif /* ACTIVERT_ENABLE_NAMES */

        me->dispatch             = dispatch;
        me->priority             = priority;
        me->queue_count          = 0;
        me->is_static            = false;
        me->notification.handler = notification_handler;

        if (xTaskCreate(
                activert_active_event_loop,
    #if ACTIVERT_ENABLE_NAMES
                name ? name : "ActiveRT",
    #else  /* ACTIVERT_ENABLE_NAMES */
                "ActiveRT",
    #endif /* ACTIVERT_ENABLE_NAMES */
                stack_size / sizeof(StackType_t),
                me,
                priority + tskIDLE_PRIORITY,
                &me->thread
            ) != pdPASS)
        {
            ACTIVERT_FREE(me);
            return NULL;
        }

    #if ACTIVERT_ENABLE_STATS
        activert_stats_register_active(me);
    #endif /* ACTIVERT_ENABLE_STATS */

        return me;
    }

    me = activert_active_create_dynamic(
        name, dispatch, priority, stack_size, queue_configs, num_queues
    );

    if (me)
    {
        me->notification.handler = notification_handler;
    }

    return me;
}

void activert_active_destroy(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);
    ACTIVERT_ASSERT(!me->is_static);

    // Delete task
    if (me->thread)
    {
        vTaskDelete(me->thread);
    }

    // Delete queue set
    if (me->queue_set)
    {
        vQueueDelete(me->queue_set);
    }

    // Delete queues
    for (uint8_t i = 0; i < me->queue_count; i++)
    {
        vQueueDelete(me->queues[i].handle);
    }

    // Free queues array
    if (me->queues)
    {
        ACTIVERT_FREE(me->queues);
    }

    // Remove from the global stats registry before freeing, otherwise the
    // registry retains a dangling pointer (use-after-free on the next walk).
    #if ACTIVERT_ENABLE_STATS
    activert_stats_unregister_active(me);
    #endif /* ACTIVERT_ENABLE_STATS */

    // Free structure
    ACTIVERT_FREE(me);
}

#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */

/*******************************************************************************
* Active Object Control
*******************************************************************************/

void activert_active_stop(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    if (me->dispatch != NULL)
    {
        // Send TERM_SIG event
        activert_event_t term_event = {.sig = ACTIVERT_TERM_SIG, .pool = NULL};
        me->dispatch(me, &term_event);
    }

    // Delete task
    if (me->thread != NULL)
    {
        vTaskDelete(me->thread);
        me->thread = NULL;
    }
}

TaskHandle_t activert_active_get_task_handle(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);
    return me->thread;
}

#if ACTIVERT_ENABLE_NAMES
const char* activert_active_get_name(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);
    return me->name;
}
#endif /* ACTIVERT_ENABLE_NAMES */

UBaseType_t activert_active_get_priority(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);
    return me->priority;
}

/*******************************************************************************
* Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

uint32_t activert_active_get_stack_high_water(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    if (me->thread == NULL)
    {
        return 0;
    }

    UBaseType_t words = uxTaskGetStackHighWaterMark(me->thread);
    return words * sizeof(StackType_t);
}

void activert_active_reset_stats(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    // Reset event stats
    me->stats.events_processed       = 0;
    me->stats.events_dropped         = 0;
    me->stats.notifications_received = 0;

    #if ACTIVERT_ENABLE_TIMING_STATS
    me->stats.total_processing_time = 0;
    me->stats.max_processing_time   = 0;
    me->stats.slowest_signal        = 0;
    #endif /* ACTIVERT_ENABLE_TIMING_STATS */

    // Reset queue stats
    for (uint8_t i = 0; i < me->queue_count; i++)
    {
        memset(&me->queues[i].stats, 0, sizeof(me->queues[i].stats));
    }
}

void activert_active_print_stats(activert_active_t* me)
{
    ACTIVERT_ASSERT(me != NULL);

    printf("================================================================\n");
    #if ACTIVERT_ENABLE_NAMES
    printf("Active Object: %s\n", me->name ? me->name : "unnamed");
    #else  /* ACTIVERT_ENABLE_NAMES */
    printf("Active Object\n");
    #endif /* ACTIVERT_ENABLE_NAMES */
    printf("================================================================\n");
    printf("Priority:        %u\n", (unsigned int)me->priority);
    printf("Queue count:     %u\n", (unsigned int)me->queue_count);

    uint32_t free_stack = activert_active_get_stack_high_water(me);
    printf("Stack free:      %u bytes\n", (unsigned int)free_stack);

    printf("\nEvent Statistics:\n");
    printf("  Processed:     %u\n", (unsigned int)me->stats.events_processed);
    printf("  Dropped:       %u\n", (unsigned int)me->stats.events_dropped);
    printf("  Notifications: %u\n", (unsigned int)me->stats.notifications_received);

    #if ACTIVERT_ENABLE_TIMING_STATS
    if (me->stats.events_processed > 0U)
    {
        TickType_t avg_time = me->stats.total_processing_time / me->stats.events_processed;
        printf("  Avg time:  %u ticks\n", (unsigned int)avg_time);
        printf(
            "  Max time:     %u ticks (sig %u)\n",
            (unsigned int)me->stats.max_processing_time,
            (unsigned int)me->stats.slowest_signal
        );
    }
    #endif /* ACTIVERT_ENABLE_TIMING_STATS */

    // Print queue stats
    for (uint8_t i = 0; i < me->queue_count; i++)
    {
        printf("\nQueue %u:\n", (unsigned int)i);
        printf("  Length:    %u\n", (unsigned int)me->queues[i].queue_length);
        printf(
            "  Posts:        %u (%u OK, %u failed)\n",
            (unsigned int)me->queues[i].stats.posts_attempted,
            (unsigned int)me->queues[i].stats.posts_succeeded,
            (unsigned int)me->queues[i].stats.posts_failed
        );
        printf(
            "  Depth:        %u / %u (current / max)\n",
            (unsigned int)me->queues[i].stats.current_depth,
            (unsigned int)me->queues[i].queue_length
        );
        printf(
            "  Peak:         %u / %u (%u%%)\n",
            (unsigned int)me->queues[i].stats.peak_depth,
            (unsigned int)me->queues[i].queue_length,
            (unsigned int)((me->queues[i].stats.peak_depth * 100U) / me->queues[i].queue_length)
        );
    }

    printf("================================================================\n");
}

#endif /* ACTIVERT_ENABLE_STATS */
