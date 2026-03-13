/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_event.h
*   @brief      Event Pool API
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial event pool API
*   0.2.0   gnp     2025-11-15  Overflow policy; activert_event_pool_init_static
*   0.3.0   gnp     2025-11-29  ISR-safe alloc and free declarations
*   0.5.0   gnp     2025-12-27  Pool statistics accessors and reset
*   1.0.0   gnp     2026-02-28  ACTIVERT_EVENT_POOL_DEFINE/INIT static allocation macros
*
*******************************************************************************/

#ifndef ACTIVERT_EVENT_H
#define ACTIVERT_EVENT_H

#include "activert_types.h"
#include "activert_config.h"
#if ACTIVERT_ENABLE_STATS == 1
    #include "activert_stats.h"
#endif

/*******************************************************************************
* Event Pool Creation
*******************************************************************************/

/**
 * Create an event pool with caller-provided event memory
 *
 * The event storage array is provided by the caller (typically a static array).
 * The pool control struct, bitmap, and mutex are heap-allocated via ACTIVERT_MALLOC.
 * For fully static (zero-heap) pools, use activert_event_pool_init_static() or
 * the ACTIVERT_EVENT_POOL_DEFINE / ACTIVERT_EVENT_POOL_INIT macro pair instead.
 *
 * @param name          Pool name (for debugging, can be NULL if names disabled)
 * @param pool_memory   Pre-allocated memory for events (must remain valid)
 * @param event_size    Size of each event in bytes (must be >= sizeof(activert_event_t))
 * @param pool_size     Number of events in pool
 * @param policy        Overflow policy
 * @return              Pointer to initialized pool, or NULL on error
 *
 * Example:
 *   static uint8_t pool_mem[16][sizeof(my_event_t)];
 *   activert_event_pool_t* pool = activert_event_pool_create(
 *       "my_pool", pool_mem, sizeof(my_event_t), 16,
 *       ACTIVERT_POOL_OVERFLOW_DROP
 *   );
 */
activert_event_pool_t* activert_event_pool_create(
    const char* name,
    void* pool_memory,
    size_t event_size,
    size_t pool_size,
    activert_pool_overflow_policy_t policy
);

/**
 * Initialize an event pool in-place using fully static storage
 *
 * Unlike activert_event_pool_create(), this function uses NO dynamic
 * allocation. The pool struct, event memory, and bitmap are all provided by
 * the caller as pre-allocated static storage. The mutex is created using the
 * StaticSemaphore_t buffer embedded in the pool struct.
 *
 * This is the function used by the ACTIVERT_EVENT_POOL_INIT macro, which
 * pairs with ACTIVERT_EVENT_POOL_DEFINE to provide fully static event pools
 * whose address (&pool_struct) is a compile-time constant.
 *
 * @param pool          Pre-allocated pool struct to initialize
 * @param name          Pool name (for debugging, can be NULL if names disabled)
 * @param pool_memory   Pre-allocated memory for events
 * @param bitmap        Pre-allocated bitmap for allocation tracking
 * @param event_size    Size of each event in bytes
 * @param pool_size     Number of events in pool
 * @param policy        Overflow policy
 */
void activert_event_pool_init_static(
    activert_event_pool_t* pool,
    const char* name,
    void* pool_memory,
    uint8_t* bitmap,
    size_t event_size,
    size_t pool_size,
    activert_pool_overflow_policy_t policy
);

/**
 * Create a dynamic event pool
 * 
 * Creates an event pool using malloc. Can be destroyed with activert_event_pool_destroy().
 * Thread-safe (uses FreeRTOS mutex).
 * 
 * @param name          Pool name (for debugging, can be NULL if names disabled)
 * @param event_size    Size of each event in bytes (must be >= sizeof(activert_event_t))
 * @param pool_size     Number of events in pool
 * @param policy        Overflow policy
 * @return              Pointer to initialized pool, or NULL on error
 * 
 * Example:
 *   activert_event_pool_t* pool = activert_event_pool_create_dynamic(
 *       "my_pool", sizeof(my_event_t), 16,
 *       ACTIVERT_POOL_OVERFLOW_DROP
 *   );
 * 
 * Note: Requires ACTIVERT_ENABLE_DYNAMIC_ALLOCATION=1
 */
#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
activert_event_pool_t* activert_event_pool_create_dynamic(
    const char* name, size_t event_size, size_t pool_size, activert_pool_overflow_policy_t policy
);

/**
 * Destroy a dynamic event pool
 * 
 * Frees all memory associated with the pool.
 * Must not be called while events are still allocated!
 * 
 * @param pool          Pool to destroy (must be dynamically allocated)
 * 
 * Warning: Does not check if events are still allocated.
 *          Ensure all events are freed before destroying pool.
 */
void activert_event_pool_destroy(activert_event_pool_t* pool);
#endif

/*******************************************************************************
* Event Allocation and Deallocation
*******************************************************************************/

/**
 * Allocate an event from a pool
 * 
 * Thread-safe. Blocks if pool is empty (unless policy is DROP).
 * Automatically sets event->pool pointer for tracking.
 * 
 * @param pool          Pool to allocate from
 * @return              Pointer to event, or NULL if pool exhausted
 * 
 * Example:
 *   my_event_t* evt = (my_event_t*)activert_event_pool_alloc(&my_pool);
 *   if (evt) {
 *       evt->base.sig = MY_SIGNAL;
 *       evt->temperature = 25.5f;
 *       activert_active_post(target_task, &evt->base);
 *   }
 * 
 * Note: Event is automatically freed after dispatch by Active Object event loop.
 *       Manual free only needed if event is not posted.
 */
activert_event_t* activert_event_pool_alloc(activert_event_pool_t* pool);

/**
 * Allocate an event from ISR context
 * 
 * ISR-safe version of activert_event_pool_alloc().
 * Never blocks - returns immediately if pool is empty.
 * 
 * @param pool          Pool to allocate from
 * @return              Pointer to event, or NULL if pool exhausted
 * 
 * Example (in ISR):
 *   void CAN_RX_IRQHandler(void) {
 *       can_event_t* evt = (can_event_t*)activert_event_pool_alloc_from_isr(&can_pool);
 *       if (evt) {
 *           evt->base.sig = CAN_RX_SIG;
 *           read_can_hardware(&evt->data);
 *           activert_active_post_from_isr(can_task, &evt->base, &xHigherPriorityTaskWoken);
 *       }
 *       portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
 *   }
 */
activert_event_t* activert_event_pool_alloc_from_isr(activert_event_pool_t* pool);

/**
 * Free an event back to its pool
 * 
 * Thread-safe. Returns event to the pool it was allocated from.
 * Uses event->pool pointer to find the pool.
 * 
 * @param event         Event to free (must have valid pool pointer)
 * 
 * Note: Normally called automatically by Active Object event loop.
 *       Only call manually if event was allocated but not posted.
 * 
 * Example:
 *   my_event_t* evt = (my_event_t*)activert_event_pool_alloc(&my_pool);
 *   if (evt) {
 *       // ... error occurred, can't post event
 *       activert_event_pool_free(&evt->base);  // Return to pool
 *   }
 */
void activert_event_pool_free(activert_event_t* event);

/**
 * Free an event from ISR context
 * 
 * ISR-safe version of activert_event_pool_free().
 * 
 * @param event         Event to free
 */
void activert_event_pool_free_from_isr(activert_event_t* event);

/*******************************************************************************
* Event Pool Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

/**
 * Get number of free events in pool
 * 
 * @param pool          Pool to query
 * @return              Number of free (available) events
 */
size_t activert_event_pool_get_free_count(activert_event_pool_t* pool);

/**
 * Get peak usage of pool
 * 
 * @param pool          Pool to query
 * @return              Maximum number of events that were allocated simultaneously
 */
size_t activert_event_pool_get_peak_usage(activert_event_pool_t* pool);

/**
 * Get total allocation attempts
 * 
 * @param pool          Pool to query
 * @return              Total number of allocation attempts (successful + failed)
 */
uint32_t activert_event_pool_get_alloc_attempts(activert_event_pool_t* pool);

/**
 * Get failed allocation count
 * 
 * @param pool          Pool to query
 * @return              Number of allocations that failed (pool exhausted)
 */
uint32_t activert_event_pool_get_alloc_failures(activert_event_pool_t* pool);

/**
 * Reset pool statistics
 * 
 * Resets counters (attempts, failures, peak) to zero.
 * Does not affect current allocations.
 * 
 * @param pool          Pool to reset
 */
void activert_event_pool_reset_stats(activert_event_pool_t* pool);

/**
 * Print pool statistics to stdout
 * 
 * Prints formatted statistics including:
 * - Pool name and size
 * - Current allocation
 * - Peak allocation
 * - Success/failure counts
 * - Allocation rate
 * 
 * @param pool          Pool to print
 * 
 * Example output:
 *   ================================================================
 *   Event Pool: my_event_pool
 *   ================================================================
 *   Size:           16 events x 72 bytes = 1152 bytes
 *   Current:        3 / 16 (18%)
 *   Peak:           12 / 16 (75%)
 *   Allocations:    14523 (14520 OK, 3 failed)
 *   Success rate:   99.98%
 *   ================================================================
 */
void activert_event_pool_print_stats(activert_event_pool_t* pool);

#endif /* ACTIVERT_ENABLE_STATS */

/*******************************************************************************
* Helper Macros
*******************************************************************************/

/**
 * Define a static event pool with automatic memory allocation
 * 
 * Creates static storage for pool memory, bitmap, and pool structure.
 * Must be followed by ACTIVERT_EVENT_POOL_INIT() in initialization code.
 * 
 * @param pool_name     Name of the pool variable
 * @param event_type    Type of events in pool (e.g., my_event_t)
 * @param count         Number of events
 * @param policy        Overflow policy
 * 
 * Example:
 *   // In header or top of file:
 *   ACTIVERT_EVENT_POOL_DEFINE(my_event_pool, my_event_t, 16, ACTIVERT_POOL_OVERFLOW_DROP);
 * 
 *   // In initialization function:
 *   ACTIVERT_EVENT_POOL_INIT(my_event_pool, my_event_t, 16, ACTIVERT_POOL_OVERFLOW_DROP);
 */
/* cppcheck-suppress misra-c2012-20.7
 * Deviation: pool_name is used only as a token-paste prefix or declaration
 * identifier. event_type is a type name inside sizeof() — C syntax does not
 * permit an additional layer of parentheses around a type name in that
 * position. count is parenthesised at every arithmetic use site; the bare
 * use in the first array dimension cannot be further parenthesised in an
 * array declarator without introducing a VLA interpretation on some
 * compilers, so a suppression is used instead. */
#define ACTIVERT_EVENT_POOL_DEFINE(pool_name, event_type, count, policy) \
    static uint8_t pool_name##_memory[(count)][sizeof(event_type)];      \
    static uint8_t pool_name##_bitmap[((count) + 7U) / 8U];              \
    static activert_event_pool_t pool_name##_struct;                     \
    static activert_event_pool_t* pool_name = NULL;

/**
 * Initialize a static event pool defined with ACTIVERT_EVENT_POOL_DEFINE()
 * 
 * @param pool_name     Name of the pool variable
 * @param event_type    Type of events in pool
 * @param count         Number of events
 * @param policy        Overflow policy
 * 
 * Example:
 *   void system_init(void) {
 *       ACTIVERT_EVENT_POOL_INIT(my_event_pool, my_event_t, 16, ACTIVERT_POOL_OVERFLOW_DROP);
 *       
 *       // Now pool is ready to use
 *       my_event_t* evt = (my_event_t*)activert_event_pool_alloc(my_event_pool);
 *   }
 */
/* cppcheck-suppress misra-c2012-20.7
 * Deviation: pool_name is used as a declaration identifier and assignment
 * target. event_type is a type name inside sizeof() — Rule 20.7
 * parenthesisation is inapplicable to type names. count and policy are
 * parenthesised at their function-argument use sites. */
#define ACTIVERT_EVENT_POOL_INIT(pool_name, event_type, count, policy) \
    do                                                                 \
    {                                                                  \
        activert_event_pool_init_static(                               \
            &pool_name##_struct,                                       \
            #pool_name,                                                \
            pool_name##_memory,                                        \
            pool_name##_bitmap,                                        \
            sizeof(event_type),                                        \
            (count),                                                   \
            (policy)                                                   \
        );                                                             \
        pool_name = &pool_name##_struct;                               \
    } while (0)

#endif /* ACTIVERT_EVENT_H */
