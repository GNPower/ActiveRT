/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_active.h
*   @brief      Active Object API
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial Active Object API with single-queue creation
*   0.4.0   gnp     2025-12-13  Multi-queue create; ACTIVERT_ACTIVE_DEFINE_SIMPLE updated
*   0.7.0   gnp     2026-01-24  Notification create variants; notify/notify_from_isr
*   1.0.0   gnp     2026-02-28  Loop task API; ACTIVERT_ACTIVE_DEFINE_LOOP macro
*
*******************************************************************************/

#ifndef ACTIVERT_ACTIVE_H
#define ACTIVERT_ACTIVE_H

#include "activert_types.h"

/*******************************************************************************
* Active Object Creation - Static Allocation
*******************************************************************************/

/**
 * Create an Active Object with static allocation
 * 
 * Creates a FreeRTOS task that processes events via a dispatch handler.
 * All memory must be pre-allocated by the caller.
 * Thread-safe. Can be called before or after scheduler starts.
 * 
 * @param name          Task name (for debugging, NULL if names disabled)
 * @param dispatch      Event dispatch handler function
 * @param priority      FreeRTOS priority (1-based, 0 = idle)
 * @param stack         Pre-allocated stack memory
 * @param stack_size    Stack size in bytes
 * @param task_cb       Pre-allocated StaticTask_t
 * @param queue_configs Array of queue configurations
 * @param num_queues    Number of queues (1 to ACTIVERT_MAX_QUEUES)
 * @param queue_cbs     Pre-allocated StaticQueue_t array
 * @param queue_storages Array of per-queue storage buffers (activert_queue_storage_t[num_queues])
 *                      Each element is a caller-provided activert_event_t* array of queue_length
 * @param queue_set_cb  Pre-allocated StaticQueue_t (NULL if num_queues==1)
 * @param queue_set_storage Pre-allocated storage buffer (NULL if num_queues==1)
 *                      Size: sum(queue_lengths) * sizeof(void*) bytes
 * @param active_storage Pre-allocated activert_active_t struct
 * @param queue_structs Pre-allocated activert_queue_t array (num_queues elements)
 * @return              Pointer to Active Object, or NULL on error
 *
 * Note: Task starts automatically. Sends INIT_SIG event to dispatch handler.
 *       All memory is caller-provided (zero heap allocation).
 */
activert_active_t* activert_active_create_static(const char* name,
                                                 activert_dispatch_handler_t dispatch,
                                                 UBaseType_t priority,
                                                 StackType_t* stack,
                                                 size_t stack_size,
                                                 StaticTask_t* task_cb,
                                                 activert_queue_config_t* queue_configs,
                                                 uint8_t num_queues,
                                                 StaticQueue_t* queue_cbs,
                                                 /* cppcheck-suppress misra-c2012-18.5
                                                  * Deviation: see activert_active_create_static
                                                  * in src/activert_active.c. */
                                                 activert_queue_storage_t* queue_storages,
                                                 StaticQueue_t* queue_set_cb,
                                                 uint8_t* queue_set_storage,
                                                 activert_active_t* active_storage,
                                                 activert_queue_t* queue_structs);

/**
 * Create Active Object with notification support (static allocation)
 * 
 * Creates an Active Object that can receive both events and task notifications.
 * Notifications are checked with higher priority than queue events.
 * 
 * @param name                  Task name
 * @param dispatch              Event dispatch handler (NULL for notify-only)
 * @param notification_handler  Notification callback function
 * @param priority              FreeRTOS priority (1-based, 0 = idle)
 * @param stack                 Pre-allocated stack
 * @param stack_size            Stack size in bytes
 * @param task_cb               Pre-allocated StaticTask_t
 * @param queue_configs         Queue configurations (NULL if notify-only)
 * @param num_queues            Number of queues  (1 to ACTIVERT_MAX_QUEUES, 0 if notify-only)
 * @param queue_cbs             Pre-allocated StaticQueue_t array (NULL if notify-only)
 * @param queue_storages        Array of per-queue storage buffers (activert_queue_storage_t[num_queues], NULL if notify-only)
 * @param queue_set_cb          Pre-allocated StaticQueue_t (NULL if single/no queue)
 * @param queue_set_storage     Pre-allocated storage buffer (NULL if single/no queue)
 *                              Size: sum(queue_lengths) * sizeof(void*) bytes
 * @param notify_sem_cb         Pre-allocated StaticSemaphore_t for notification semaphore
 *                              (NULL if notification-only task, required if queues + notifications)
 * @param active_storage        Pre-allocated activert_active_t struct
 * @param queue_structs         Pre-allocated activert_queue_t array (num_queues elements, NULL if notify-only)
 * @return                      Pointer to Active Object, or NULL on error
 *
 * Note: All memory is caller-provided (zero heap allocation).
 */
activert_active_t*
activert_active_create_with_notification_static(const char* name,
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
                                                 * Deviation: see activert_active_create_static
                                                 * in src/activert_active.c. */
                                                activert_queue_storage_t* queue_storages,
                                                StaticQueue_t* queue_set_cb,
                                                uint8_t* queue_set_storage,
                                                StaticSemaphore_t* notify_sem_cb,
                                                activert_active_t* active_storage,
                                                activert_queue_t* queue_structs);

/*******************************************************************************
* Loop Task Creation - Static Allocation
*******************************************************************************/

/**
 * Create a loop task with static allocation (zero heap)
 *
 * Creates a minimal ActiveRT task that calls a loop function in while(1).
 * No queues, no notifications. Optionally receives INIT_SIG via dispatch.
 * Registered in the stats registry for CLI visibility.
 *
 * @param name            Task name (for debugging)
 * @param dispatch        Dispatch handler for INIT_SIG (can be NULL)
 * @param loop_fn         Loop function called repeatedly (REQUIRED)
 * @param priority        FreeRTOS priority (1-based, 0 = idle)
 * @param stack           Pre-allocated stack memory
 * @param stack_size      Stack size in bytes
 * @param task_cb         Pre-allocated StaticTask_t
 * @param active_storage  Pre-allocated activert_active_t struct
 * @return                Pointer to Active Object, or NULL on error
 */
activert_active_t* activert_active_create_loop_static(const char* name,
                                                      activert_dispatch_handler_t dispatch,
                                                      activert_loop_fn_t loop_fn,
                                                      UBaseType_t priority,
                                                      StackType_t* stack,
                                                      size_t stack_size,
                                                      StaticTask_t* task_cb,
                                                      activert_active_t* active_storage);

/*******************************************************************************
* Active Object Creation - Dynamic Allocation
*******************************************************************************/

#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION

/**
 * Create an Active Object with dynamic allocation
 * 
 * All memory allocated with malloc. Can be destroyed with activert_active_destroy().
 * 
 * @param name          Task name
 * @param dispatch      Event dispatch handler
 * @param priority      FreeRTOS priority (1-based, 0 = idle)
 * @param stack_size    Stack size in bytes
 * @param queue_configs Queue configurations
 * @param num_queues    Number of queues
 * @return              Pointer to Active Object, or NULL on error
 */
activert_active_t* activert_active_create_dynamic(const char* name,
                                                  activert_dispatch_handler_t dispatch,
                                                  UBaseType_t priority,
                                                  size_t stack_size,
                                                  activert_queue_config_t* queue_configs,
                                                  uint8_t num_queues);

/**
 * Create Active Object with notification support (dynamic allocation)
 * 
 * @param name                  Task name
 * @param dispatch              Event dispatch handler (NULL for notify-only)
 * @param notification_handler  Notification callback
 * @param priority              FreeRTOS priority (1-based, 0 = idle)
 * @param stack_size            Stack size in bytes
 * @param queue_configs         Queue configurations (NULL if notify-only)
 * @param num_queues            Number of queues (0 if notify-only)
 * @return                      Pointer to Active Object, or NULL on error
 */
activert_active_t*
activert_active_create_with_notification_dynamic(const char* name,
                                                 activert_dispatch_handler_t dispatch,
                                                 activert_notify_handler_t notification_handler,
                                                 UBaseType_t priority,
                                                 size_t stack_size,
                                                 activert_queue_config_t* queue_configs,
                                                 uint8_t num_queues);

/**
 * Destroy a dynamically allocated Active Object
 * 
 * Stops the task, frees all queues, and frees the Active Object structure.
 * Must not be called on statically allocated Active Objects!
 * 
 * @param me            Active Object to destroy (must be dynamic)
 * 
 * Warning: Does not check if events are still in queues.
 *          Ensure all events are processed before destroying.
 */
void activert_active_destroy(activert_active_t* me);

#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */

/*******************************************************************************
* Active Object Control
*******************************************************************************/

/**
 * Stop an Active Object
 * 
 * Sends TERM_SIG event to dispatch handler, then deletes the FreeRTOS task.
 * The Active Object structure remains valid and can be restarted.
 * 
 * @param me            Active Object to stop
 * 
 * Note: For statically allocated Active Objects, memory is not freed.
 *       For dynamically allocated, use activert_active_destroy() instead.
 */
void activert_active_stop(activert_active_t* me);

/**
 * Get FreeRTOS task handle
 * 
 * @param me            Active Object
 * @return              FreeRTOS task handle
 */
TaskHandle_t activert_active_get_task_handle(activert_active_t* me);

/**
 * Get Active Object name
 * 
 * @param me            Active Object
 * @return              Task name, or NULL if names disabled
 */
#if ACTIVERT_ENABLE_NAMES
const char* activert_active_get_name(activert_active_t* me);
#endif

/**
 * Get Active Object priority
 * 
 * @param me            Active Object
 * @return              FreeRTOS priority
 */
UBaseType_t activert_active_get_priority(activert_active_t* me);

/*******************************************************************************
* Event Posting
*******************************************************************************/

/**
 * Post event to Active Object (signal-based routing)
 * 
 * Routes event to appropriate queue based on event signal.
 * Thread-safe. Non-blocking - returns immediately if queue is full.
 * 
 * @param me            Target Active Object
 * @param event         Event to post (must not be NULL)
 * @return              0 on success, -1 on failure (queue full or routing error)
 * 
 * Example:
 *   my_event_t* evt = (my_event_t*)activert_event_pool_alloc(&my_pool);
 *   if (evt) {
 *       evt->base.sig = MY_SIGNAL;
 *       evt->data = 123;
 *       
 *       if (activert_active_post(my_task, &evt->base) != 0) {
 *           // Post failed - return event to pool
 *           activert_event_pool_free(&evt->base);
 *       }
 *   }
 * 
 * Note: Event is automatically freed after dispatch. Don't free manually!
 */
int activert_active_post(activert_active_t* me, activert_event_t* event);

/**
 * Post event to specific queue
 * 
 * Posts directly to the specified queue index, bypassing signal routing.
 * 
 * @param me            Target Active Object
 * @param queue_index   Queue index (0 to num_queues-1)
 * @param event         Event to post
 * @return              0 on success, -1 on failure
 * 
 * Example:
 *   // Post to queue 0 regardless of signal
 *   activert_active_post_to_queue(task, 0, &evt->base);
 */
int activert_active_post_to_queue(activert_active_t* me,
                                  uint8_t queue_index,
                                  activert_event_t* event);

/**
 * Post event from ISR context
 * 
 * ISR-safe version of activert_active_post().
 * 
 * @param me                            Target Active Object
 * @param event                         Event to post
 * @param pxHigherPriorityTaskWoken     FreeRTOS context switch flag
 * @return                              0 on success, -1 on failure
 * 
 * Example:
 *   void MY_IRQHandler(void) {
 *       BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *       
 *       my_event_t* evt = (my_event_t*)activert_event_pool_alloc_from_isr(&pool);
 *       if (evt) {
 *           evt->base.sig = IRQ_SIG;
 *           activert_active_post_from_isr(task, &evt->base, &xHigherPriorityTaskWoken);
 *       }
 *       
 *       portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
 *   }
 */
int activert_active_post_from_isr(activert_active_t* me,
                                  activert_event_t* event,
                                  BaseType_t* pxHigherPriorityTaskWoken);

/**
 * Post event to specific queue from ISR context
 * 
 * @param me                            Target Active Object
 * @param queue_index                   Queue index
 * @param event                         Event to post
 * @param pxHigherPriorityTaskWoken     FreeRTOS context switch flag
 * @return                              0 on success, -1 on failure
 */
int activert_active_post_to_queue_from_isr(activert_active_t* me,
                                           uint8_t queue_index,
                                           activert_event_t* event,
                                           BaseType_t* pxHigherPriorityTaskWoken);

/*******************************************************************************
* Notification Support
*******************************************************************************/

/**
 * Notify Active Object (wake via task notification)
 * 
 * Sets notification bits to wake the task. Handler is called with priority
 * over queue events.
 * 
 * @param me            Target Active Object
 * @param notify_bits   Bits to set in notification
 * 
 * Example:
 *   #define NOTIFY_BIT (1 << 0)
 *   activert_active_notify(some_task, NOTIFY_BIT);
 */
void activert_active_notify(activert_active_t* me, uint32_t notify_bits);

/**
 * Notify Active Object from ISR context
 * 
 * @param me                            Target Active Object
 * @param notify_bits                   Bits to set
 * @param pxHigherPriorityTaskWoken     FreeRTOS context switch flag
 * 
 * Example:
 *   void CAN_RX_IRQHandler(void) {
 *       BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *       activert_active_notify_from_isr(some_task, NOTIFY_BIT, &xHigherPriorityTaskWoken);
 *       portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
 *   }
 */
void activert_active_notify_from_isr(activert_active_t* me,
                                     uint32_t notify_bits,
                                     BaseType_t* pxHigherPriorityTaskWoken);

/**
 * Set the valid notification bits mask for an Active Object.
 *
 * @param me    Active Object (must have a notification handler configured)
 * @param mask  Bitmask of valid notification bits
 */
void activert_active_set_notify_mask(activert_active_t* me, uint32_t mask);

/**
 * Get the current notification bits mask.
 *
 * @param me    Active Object
 * @return      Current notification mask
 */
uint32_t activert_active_get_notify_mask(activert_active_t* me);

/**
 * Check whether an Active Object has a notification handler configured.
 *
 * @param me    Active Object
 * @return      true if a notification handler is set, false otherwise
 */
bool activert_active_has_notification(activert_active_t* me);

#if ACTIVERT_ENABLE_STATS

/**
 * Get the total number of notifications received by an Active Object.
 *
 * @param me    Active Object
 * @return      Notification count
 */
uint32_t activert_active_get_notification_count(activert_active_t* me);

/**
 * Print notification statistics for an Active Object to stdout.
 *
 * @param me    Active Object
 */
void activert_active_print_notification_stats(activert_active_t* me);

#endif /* ACTIVERT_ENABLE_STATS */

/*******************************************************************************
* Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

/**
 * Get stack high water mark
 * 
 * Returns the minimum free stack space that has ever existed.
 * Lower values indicate the task is using more stack.
 * 
 * @param me            Active Object
 * @return              Minimum free stack in bytes
 * 
 * Example:
 *   uint32_t free_stack = activert_active_get_stack_high_water(my_task);
 *   if (free_stack < 512) {
 *       printf("WARNING: Low stack space!\n");
 *   }
 */
uint32_t activert_active_get_stack_high_water(activert_active_t* me);

/**
 * Reset Active Object statistics
 * 
 * Resets event counters and timing stats to zero.
 * Does not reset stack high water mark.
 * 
 * @param me            Active Object
 */
void activert_active_reset_stats(activert_active_t* me);

/**
 * Print Active Object statistics
 * 
 * Prints formatted statistics including queue depths, event counts,
 * processing times, and stack usage.
 * 
 * @param me            Active Object
 */
void activert_active_print_stats(activert_active_t* me);

#endif /* ACTIVERT_ENABLE_STATS */

/*******************************************************************************
* Helper Macros
*******************************************************************************/

/**
 * Define a simple Active Object with single queue (static allocation)
 * 
 * Creates all necessary static storage for:
 * - Stack
 * - Task control block
 * - Queue storage
 * - Queue control block
 * 
 * Must be followed by ACTIVERT_ACTIVE_INIT_SIMPLE() in initialization code.
 * 
 * @param task_name     Name of the Active Object variable
 * @param dispatch_fn   Dispatch handler function name (not used here, just for reference)
 * @param prio          FreeRTOS priority (not used here, just for reference)
 * @param stack_sz      Stack size in bytes
 * @param queue_len     Queue depth (number of event pointers)
 * 
 * Example:
 *   // In header or top of file:
 *   ACTIVERT_ACTIVE_DEFINE_SIMPLE(my_task, my_dispatch, 5, 1024, 16);
 * 
 *   // In initialization function:
 *   ACTIVERT_ACTIVE_INIT_SIMPLE(my_task, my_dispatch, 5);
 */
/* cppcheck-suppress misra-c2012-20.7
 * Deviation: task_name, dispatch_fn, and prio are used only as token-paste
 * prefixes (##) or declaration identifiers — not as value expressions —
 * so Rule 20.7 parenthesisation is inapplicable. stack_sz and queue_len
 * are parenthesised at every expression use site within the macro body. */
#define ACTIVERT_ACTIVE_DEFINE_SIMPLE(task_name, dispatch_fn, prio, stack_sz, queue_len) \
    static StackType_t task_name##_stack[(stack_sz) / sizeof(StackType_t)];              \
    static StaticTask_t task_name##_task_cb;                                             \
    static activert_event_t* task_name##_queue_storage[(queue_len)];                    \
    static StaticQueue_t task_name##_queue_cb;                                           \
    static activert_active_t task_name##_ao_storage;                                     \
    static activert_queue_t task_name##_queue_struct;                                    \
    static activert_active_t* task_name = NULL;

/**
 * Initialize a simple Active Object defined with ACTIVERT_ACTIVE_DEFINE_SIMPLE()
 * 
 * @param task_name     Name of the Active Object variable
 * @param dispatch_fn   Dispatch handler function
 * @param prio          FreeRTOS priority (0 = idle, higher = higher priority)
 * 
 * Example:
 *   void system_init(void) {
 *       ACTIVERT_ACTIVE_INIT_SIMPLE(my_task, my_dispatch, 5);
 *       
 *       // Task is now running
 *   }
 */
/* cppcheck-suppress misra-c2012-20.7
 * Deviation: task_name is used as a declaration identifier and assignment
 * target (not a value expression); dispatch_fn and prio are used as direct
 * function-call arguments where precedence is unambiguous. Parenthesising
 * a declaration name or a lone identifier passed as a function argument
 * provides no safety benefit and is not required by Rule 20.7. */
#define ACTIVERT_ACTIVE_INIT_SIMPLE(task_name, dispatch_fn, prio)                            \
    do                                                                                       \
    {                                                                                        \
        activert_queue_config_t task_name##_queue_config = {                                 \
            .signal_base  = 0,                                                               \
            .signal_count = 0,                                                               \
            .queue_length = sizeof(task_name##_queue_storage) / sizeof(activert_event_t*),   \
            .event_pool   = NULL};                                                             \
        activert_event_t** task_name##_queue_storage_array[1] = {task_name##_queue_storage}; \
        task_name = activert_active_create_static(#task_name,                                \
                                                  (dispatch_fn),                             \
                                                  (prio),                                    \
                                                  task_name##_stack,                         \
                                                  sizeof(task_name##_stack),                 \
                                                  &task_name##_task_cb,                      \
                                                  &task_name##_queue_config,                 \
                                                  1,                                         \
                                                  &task_name##_queue_cb,                     \
                                                  task_name##_queue_storage_array,           \
                                                  NULL,                                      \
                                                  NULL,                                      \
                                                  &task_name##_ao_storage,                   \
                                                  &task_name##_queue_struct);                \
        ACTIVERT_ASSERT(task_name != NULL);                                                  \
    } while (0)

/**
 * Define a loop task (static allocation)
 *
 * Creates static storage for stack, task control block, and AO struct.
 * Must be followed by ACTIVERT_ACTIVE_INIT_LOOP() in initialization code.
 *
 * @param task_name     Name of the Active Object variable
 * @param stack_sz      Stack size in bytes
 * 
 * Example:
 *   // In header or top of file:
 *   ACTIVERT_ACTIVE_DEFINE_LOOP(my_task, 1024);
 * 
 *   // In initialization function:
 *   ACTIVERT_ACTIVE_INIT_LOOP(my_task, my_dispatch, my_loop, 5);
 */
/* cppcheck-suppress misra-c2012-20.7
 * Deviation: task_name is used only as a token-paste prefix (##) or
 * declaration identifier, not as a value expression. stack_sz is
 * parenthesised at its expression use site. */
#define ACTIVERT_ACTIVE_DEFINE_LOOP(task_name, stack_sz)                      \
    static StackType_t task_name##_stack[(stack_sz) / sizeof(StackType_t)]; \
    static StaticTask_t task_name##_task_cb;                              \
    static activert_active_t task_name##_ao_storage;                      \
    static activert_active_t* task_name = NULL;

/**
 * Initialize a loop task defined with ACTIVERT_ACTIVE_DEFINE_LOOP()
 *
 * @param task_name     Name of the Active Object variable
 * @param dispatch_fn   Dispatch handler for INIT_SIG (can be NULL)
 * @param loop_fn       Loop function called repeatedly in while(1)
 * @param prio          FreeRTOS priority
 * 
 * Example:
 *   void system_init(void) {
 *       ACTIVERT_ACTIVE_INIT_LOOP(my_task, my_dispatch, my_loop, 5);
 *       
 *       // Task is now running
 *   }
 */
/* cppcheck-suppress misra-c2012-20.7
 * Deviation: see ACTIVERT_ACTIVE_INIT_SIMPLE — same rationale applies to
 * task_name, dispatch_fn, loop_fn, and prio. */
#define ACTIVERT_ACTIVE_INIT_LOOP(task_name, dispatch_fn, loop_fn, prio)          \
    do                                                                            \
    {                                                                             \
        task_name = activert_active_create_loop_static(#task_name,                \
                                                       (dispatch_fn),             \
                                                       (loop_fn),                 \
                                                       (prio),                    \
                                                       task_name##_stack,         \
                                                       sizeof(task_name##_stack), \
                                                       &task_name##_task_cb,      \
                                                       &task_name##_ao_storage);  \
        ACTIVERT_ASSERT(task_name != NULL);                                       \
    } while (0)

#endif /* ACTIVERT_ACTIVE_H */
