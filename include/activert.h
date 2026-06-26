/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert.h
*   @brief      Master include — single header for all ActiveRT functionality
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Include this file to access all ActiveRT APIs. Includes are ordered by
*   dependency: configuration, types, event pool, active object. Optional
*   CLI and statistics headers are guarded by their respective feature macros.
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial master include
*   0.4.0   gnp     2025-12-13  Added activert_queue.h
*   0.5.0   gnp     2025-12-27  Added activert_stats.h
*   0.6.0   gnp     2026-01-10  Added activert_cli.h (guarded by ACTIVERT_ENABLE_CLI)
*   1.0.0   gnp     2026-02-28  Version macros; convenience aliases; quick-reference patterns
*
*******************************************************************************/

#ifndef ACTIVERT_H
#define ACTIVERT_H

#ifdef __cplusplus
extern "C"
{
#endif

    /*******************************************************************************
* Version Information
*******************************************************************************/

#define ACTIVERT_VERSION_MAJOR 1
#define ACTIVERT_VERSION_MINOR 1
#define ACTIVERT_VERSION_PATCH 0

#define ACTIVERT_VERSION_STRING "1.1.0"

/*******************************************************************************
* Core Includes (Order matters - dependencies)
*******************************************************************************/

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

/*******************************************************************************
* Convenience Macros
*******************************************************************************/

/**
 * Get ActiveRT version as a 32-bit number
 * Format: 0xMMmmpppp (Major, minor, patch)
 */
#define ACTIVERT_VERSION \
    ((ACTIVERT_VERSION_MAJOR << 24) | (ACTIVERT_VERSION_MINOR << 16) | (ACTIVERT_VERSION_PATCH))

/**
 * Check if ActiveRT version is at least X.Y.Z
 */
#define ACTIVERT_VERSION_CHECK(major, minor, patch) \
    (ACTIVERT_VERSION >= (((major) << 24) | ((minor) << 16) | (patch)))

/**
 * Convenience aliases for common operations
 * These make the API more concise while maintaining clarity
 */
#define activert_post(ao, evt)              activert_active_post((ao), (evt))
#define activert_post_isr(ao, evt, wake)    activert_active_post_from_isr((ao), (evt), (wake))
#define activert_notify(ao, bits)           activert_active_notify((ao), (bits))
#define activert_notify_isr(ao, bits, wake) activert_active_notify_from_isr((ao), (bits), (wake))

    /*******************************************************************************
* Quick Reference - Common Patterns
*******************************************************************************/

    /*
 * PATTERN 1: Create Event Pool
 * 
 *   #define POOL_SIZE 10
 *   static my_event_t pool_storage[POOL_SIZE];
 *   
 *   activert_event_pool_t* pool = activert_event_pool_create(
 *       "MyPool",
 *       pool_storage,
 *       sizeof(my_event_t),
 *       POOL_SIZE,
 *       ACTIVERT_POOL_POLICY_BLOCK
 *   );
 */

    /*
 * PATTERN 2: Create Active Object (Single Queue)
 *
 *   static StackType_t stack[512];
 *   static StaticTask_t task_cb;
 *   static StaticQueue_t queue_cb;
 *   static activert_event_t* queue_storage[16];
 *   static activert_active_t ao_storage;
 *   static activert_queue_t queue_struct;
 *
 *   activert_queue_config_t config = {
 *       .queue_length = 16,
 *       .event_size = sizeof(void*),
 *       .event_pool = pool
 *   };
 *
 *   activert_event_t** queue_storage_array[1] = { queue_storage };
 *
 *   activert_active_t* ao = activert_active_create_static(
 *       "MyTask",
 *       my_dispatch_handler,
 *       5,  // Priority
 *       stack, sizeof(stack), &task_cb,
 *       &config, 1, &queue_cb, queue_storage_array, NULL, NULL,
 *       &ao_storage, &queue_struct
 *   );
 */

    /*
 * PATTERN 3: Post Event to Active Object
 * 
 *   my_event_t* evt = (my_event_t*)activert_event_alloc(pool);
 *   if (evt != NULL) {
 *       evt->signal = MY_SIGNAL;
 *       evt->data = 42;
 *       activert_post(ao, evt);
 *   }
 */

    /*
 * PATTERN 4: Dispatch Handler
 * 
 *   void my_dispatch_handler(activert_active_t* me, activert_event_t* evt) {
 *       my_event_t* e = (my_event_t*)evt;
 *       
 *       switch (e->signal) {
 *           case INIT_SIG:
 *               // Initialize state
 *               break;
 *           case MY_SIGNAL:
 *               // Handle event
 *               break;
 *       }
 *   }
 */

    /*
 * PATTERN 5: Multi-Queue Active Object
 *
 *   static StaticQueue_t queue_cbs[2];
 *   static activert_event_t* queue0_storage[10];
 *   static activert_event_t* queue1_storage[5];
 *   static StaticQueue_t queue_set_cb;
 *   static uint8_t queue_set_storage[(10+5) * sizeof(void*)];
 *   static activert_active_t ao_storage;
 *   static activert_queue_t queue_structs[2];
 *
 *   activert_queue_config_t configs[2] = {
 *       {.queue_length = 10, .event_size = sizeof(void*), .event_pool = pool1},
 *       {.queue_length = 5,  .event_size = sizeof(void*), .event_pool = pool2}
 *   };
 *
 *   activert_event_t** queue_storage_array[2] = { queue0_storage, queue1_storage };
 *
 *   activert_active_t* ao = activert_active_create_static(
 *       "MultiQ",
 *       my_dispatch,
 *       5,
 *       stack, sizeof(stack), &task_cb,
 *       configs, 2,
 *       queue_cbs, queue_storage_array, &queue_set_cb, queue_set_storage,
 *       &ao_storage, queue_structs
 *   );
 */

    /*
 * PATTERN 6: Notification Handler (ISR -> Task)
 *
 *   void my_notify_handler(activert_active_t* me, uint32_t value) {
 *       // Handle notification from ISR
 *   }
 *
 *   static activert_active_t ao_storage;
 *   static activert_queue_t queue_struct;
 *   static StaticQueue_t queue_set_cb;
 *   static StaticSemaphore_t notify_sem_cb;
 *   // Queue set holds every queued event PLUS the notification semaphore (+1):
 *   static uint8_t queue_set_storage[ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(16)];
 *
 *   activert_active_t* ao = activert_active_create_with_notification_static(
 *       "ISR_Task",
 *       my_dispatch,
 *       my_notify_handler,
 *       5,
 *       stack, sizeof(stack), &task_cb,
 *       &config, 1, &queue_cb, queue_storage_array,
 *       &queue_set_cb, queue_set_storage, &notify_sem_cb,
 *       &ao_storage, &queue_struct
 *   );
 *
 *   // From ISR:
 *   activert_notify_from_isr(ao, 0x1234, NULL);
 */

    /*
 * PATTERN 7: Loop Task (No Queue, No Notifications)
 *
 *   void my_loop_fn(activert_active_t* me) {
 *       // Called repeatedly in while(1)
 *       // e.g., some_blocking_operation();
 *   }
 *
 *   ACTIVERT_ACTIVE_DEFINE_LOOP(my_loop, 1024);
 *
 *   // In init function:
 *   ACTIVERT_ACTIVE_INIT_LOOP(my_loop, NULL, my_loop_fn, 3);
 */

#ifdef __cplusplus
}
#endif

#endif /* ACTIVERT_H */
