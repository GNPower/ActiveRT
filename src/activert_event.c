/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_event.c
*   @brief      Event Pool Implementation
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial event pool with mutex and basic bitmap
*   0.2.0   gnp     2025-11-15  Packed bit-per-slot bitmap; overflow policy enforcement
*   0.3.0   gnp     2025-11-29  ISR-safe alloc and free variants
*   0.5.0   gnp     2025-12-27  Per-pool statistics integration
*   1.0.0   gnp     2026-02-28  Static pool initialisation; ACTIVERT_MALLOC macro
*
*******************************************************************************/

#include "activert_event.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
* Internal Helper Functions
*******************************************************************************/

/**
 * Find first free event in pool using bitmap
 * 
 * @param pool          Pool to search
 * @return              Index of free event, or -1 if none available
 */
static int find_free_event(activert_event_pool_t* pool) {
    size_t bitmap_bytes = (pool->pool_size + 7) / 8;
    
    for (size_t byte_idx = 0; byte_idx < bitmap_bytes; byte_idx++) {
        uint8_t byte = pool->usage_bitmap[byte_idx];
        
        // Skip if all bits set (all events in this byte are allocated)
        if (byte == 0xFF) {
            continue;
        }
        
        // Find first zero bit
        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            size_t event_idx = (byte_idx * 8) + bit_idx;
            
            // Check if we've gone past the pool size
            if (event_idx >= pool->pool_size) {
                return -1;
            }
            
            // Check if this bit is zero (event is free)
            if (!(byte & (1 << bit_idx))) {
                return (int)event_idx;
            }
        }
    }
    
    return -1;  // No free events
}

/**
 * Mark event as allocated in bitmap
 * 
 * @param pool          Pool
 * @param event_idx     Event index to mark
 */
static void mark_event_allocated(
    activert_event_pool_t* pool, 
    size_t event_idx
) 
{
    size_t byte_idx = event_idx / 8;
    uint8_t bit_idx = event_idx % 8;
    pool->usage_bitmap[byte_idx] |= (1 << bit_idx);
}

/**
 * Mark event as free in bitmap
 * 
 * @param pool          Pool
 * @param event_idx     Event index to mark
 */
static void mark_event_free(
    activert_event_pool_t* pool, 
    size_t event_idx
) 
{
    size_t byte_idx = event_idx / 8;
    uint8_t bit_idx = event_idx % 8;
    pool->usage_bitmap[byte_idx] &= ~(1 << bit_idx);
}

/**
 * Get pointer to event at index
 * 
 * @param pool          Pool
 * @param event_idx     Event index
 * @return              Pointer to event
 */
static activert_event_t* get_event_at_index(
    activert_event_pool_t* pool, 
    size_t event_idx
) 
{
    uint8_t* base = (uint8_t*)pool->pool_memory;
    return (activert_event_t*)(base + (event_idx * pool->event_size));
}

/**
 * Get index of event in pool
 * 
 * @param pool          Pool
 * @param event         Event pointer
 * @return              Event index, or -1 if not in pool
 */
static int get_event_index(
    activert_event_pool_t* pool, 
    const activert_event_t* event
) 
{
    uint8_t* base = (uint8_t*)pool->pool_memory;
    uint8_t* evt_ptr = (uint8_t*)event;
    
    // Check if event is within pool memory range
    if (evt_ptr < base || evt_ptr >= (base + (pool->pool_size * pool->event_size))) {
        return -1;
    }
    
    // Calculate index
    ptrdiff_t offset = evt_ptr - base;
    size_t event_idx = offset / pool->event_size;
    
    // Verify alignment
    if (offset % pool->event_size != 0) {
        return -1;  // Misaligned pointer
    }
    
    return (int)event_idx;
}

/*******************************************************************************
* Event Pool Creation
*******************************************************************************/

activert_event_pool_t* activert_event_pool_create(
    const char* name,
    void* pool_memory,
    size_t event_size,
    size_t pool_size,
    activert_pool_overflow_policy_t policy
)
{
    // Validate parameters
    ACTIVERT_ASSERT(pool_memory != NULL);
    ACTIVERT_ASSERT(event_size >= sizeof(activert_event_t));
    ACTIVERT_ASSERT(pool_size > 0);
    
    // Allocate pool structure
    activert_event_pool_t* pool = (activert_event_pool_t*)ACTIVERT_MALLOC(sizeof(activert_event_pool_t));
    if (pool == NULL) {
        return NULL;
    }
    
    // Allocate bitmap (1 bit per event)
    size_t bitmap_bytes = (pool_size + 7) / 8;
    pool->usage_bitmap = (uint8_t*)ACTIVERT_MALLOC(bitmap_bytes);
    if (pool->usage_bitmap == NULL) {
        ACTIVERT_FREE(pool);
        return NULL;
    }
    
    // Initialize bitmap (all zeros = all free)
    memset(pool->usage_bitmap, 0, bitmap_bytes);
    
    // Create mutex for thread-safe access
    pool->mutex = xSemaphoreCreateMutex();
    if (pool->mutex == NULL) {
        ACTIVERT_FREE(pool->usage_bitmap);
        ACTIVERT_FREE(pool);
        return NULL;
    }
    
    // Initialize pool fields
    #if ACTIVERT_ENABLE_NAMES
    pool->name = name;
    #endif
    pool->pool_memory = pool_memory;
    pool->event_size = event_size;
    pool->pool_size = pool_size;
    pool->policy = policy;
    
    // Initialize statistics
    #if ACTIVERT_ENABLE_STATS
    memset(&pool->stats, 0, sizeof(pool->stats));
    #endif
    
    #if ACTIVERT_ENABLE_DEBUG
    printf("activert_event_pool_create: Created pool '%s' with %zu events of %zu bytes each\n",
           name ? name : "unnamed", pool_size, event_size);
    #endif

    #if ACTIVERT_ENABLE_STATS
    activert_stats_register_pool(pool);
    #endif

    return pool;
}

void activert_event_pool_init_static(
    activert_event_pool_t* pool,
    const char* name,
    void* pool_memory,
    uint8_t* bitmap,
    size_t event_size,
    size_t pool_size,
    activert_pool_overflow_policy_t policy
)
{
    ACTIVERT_ASSERT(pool != NULL);
    ACTIVERT_ASSERT(pool_memory != NULL);
    ACTIVERT_ASSERT(bitmap != NULL);
    ACTIVERT_ASSERT(event_size >= sizeof(activert_event_t));
    ACTIVERT_ASSERT(pool_size > 0);

    size_t bitmap_bytes = (pool_size + 7) / 8;
    memset(bitmap, 0, bitmap_bytes);

    pool->mutex = xSemaphoreCreateMutexStatic(&pool->mutex_buffer);
    ACTIVERT_ASSERT(pool->mutex != NULL);

    #if ACTIVERT_ENABLE_NAMES
    pool->name = name;
    #else
    (void)name;
    #endif
    pool->pool_memory = pool_memory;
    pool->usage_bitmap = bitmap;
    pool->event_size = event_size;
    pool->pool_size = pool_size;
    pool->policy = policy;

    #if ACTIVERT_ENABLE_STATS
    memset(&pool->stats, 0, sizeof(pool->stats));
    #endif

    #if ACTIVERT_ENABLE_DEBUG
    printf("activert_event_pool_init_static: Initialized pool '%s' with %zu events of %zu bytes each\n",
           name ? name : "unnamed", pool_size, event_size);
    #endif

    #if ACTIVERT_ENABLE_STATS
    activert_stats_register_pool(pool);
    #endif
}

#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION

activert_event_pool_t* activert_event_pool_create_dynamic(
    const char* name,
    size_t event_size,
    size_t pool_size,
    activert_pool_overflow_policy_t policy
)
{
    // Validate parameters
    ACTIVERT_ASSERT(event_size >= sizeof(activert_event_t));
    ACTIVERT_ASSERT(pool_size > 0);
    
    // Allocate pool memory
    void* pool_memory = ACTIVERT_MALLOC(pool_size * event_size);
    if (pool_memory == NULL) {
        return NULL;
    }
    
    // Create pool using static creation function
    activert_event_pool_t* pool = activert_event_pool_create(
        name, pool_memory, event_size, pool_size, policy
    );
    
    if (pool == NULL) {
        ACTIVERT_FREE(pool_memory);
        return NULL;
    }
    
    #if ACTIVERT_ENABLE_DEBUG
    printf("activert_event_pool_create_dynamic: Created dynamic pool '%s'\n",
           name ? name : "unnamed");
    #endif
    
    return pool;
}

void activert_event_pool_destroy(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    
    #if ACTIVERT_ENABLE_DEBUG
    #if ACTIVERT_ENABLE_NAMES
    printf("activert_event_pool_destroy: Destroying pool '%s'\n", 
           pool->name ? pool->name : "unnamed");
    #endif
    #endif
    
    vSemaphoreDelete(pool->mutex);
    ACTIVERT_FREE(pool->usage_bitmap);
    ACTIVERT_FREE(pool->pool_memory);
    ACTIVERT_FREE(pool);
}

#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */

/*******************************************************************************
* Event Allocation
*******************************************************************************/

activert_event_t* activert_event_pool_alloc(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    
    #if ACTIVERT_ENABLE_STATS
    pool->stats.allocs_attempted++;
    #endif
    
    // Take mutex for thread-safe access
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }
    
    // Find free event
    int event_idx = find_free_event(pool);
    
    if (event_idx < 0) {
        // Pool exhausted
        xSemaphoreGive(pool->mutex);
        
        #if ACTIVERT_ENABLE_STATS
        pool->stats.allocs_failed++;
        #endif
        
        #if ACTIVERT_ENABLE_POOL_OVERFLOW_DETECTION
        #if ACTIVERT_ENABLE_NAMES
        printf("WARNING: Event pool '%s' exhausted!\n", 
               pool->name ? pool->name : "unnamed");
        #else
        printf("WARNING: Event pool exhausted!\n");
        #endif
        #endif
        
        // Handle overflow based on policy
        switch (pool->policy) {
            case ACTIVERT_POOL_OVERFLOW_DROP:
                return NULL;
                
            case ACTIVERT_POOL_OVERFLOW_ASSERT:
                ACTIVERT_ASSERT(0);  // Trap
                return NULL;
                
            case ACTIVERT_POOL_OVERFLOW_DYNAMIC:
                #if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
                // Fall back to dynamic allocation
                activert_event_t* event = (activert_event_t*)ACTIVERT_MALLOC(pool->event_size);
                if (event) {
                    event->pool = NULL;  // Mark as dynamically allocated
                }
                return event;
                #else
                return NULL;
                #endif
                
            default:
                return NULL;
        }
    }
    
    // Mark event as allocated
    mark_event_allocated(pool, event_idx);
    
    // Get event pointer
    activert_event_t* event = get_event_at_index(pool, event_idx);
    
    // Initialize event
    memset(event, 0, pool->event_size);
    event->pool = pool;  // Track which pool this came from
    
    // Update statistics
    #if ACTIVERT_ENABLE_STATS
    pool->stats.allocs_succeeded++;
    pool->stats.current_allocated++;
    if (pool->stats.current_allocated > pool->stats.peak_allocated) {
        pool->stats.peak_allocated = pool->stats.current_allocated;
    }
    #endif
    
    xSemaphoreGive(pool->mutex);
    
    #if ACTIVERT_ENABLE_DEBUG
    printf("activert_event_pool_alloc: Allocated event %d from pool '%s'\n",
           event_idx, pool->name ? pool->name : "unnamed");
    #endif
    
    return event;
}

activert_event_t* activert_event_pool_alloc_from_isr(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);

    #if ACTIVERT_ENABLE_STATS
    pool->stats.allocs_attempted++;
    #endif

    // Use critical section — xSemaphoreTakeFromISR does not support mutexes
    UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    // Find free event
    int event_idx = find_free_event(pool);

    if (event_idx < 0) {
        // Pool exhausted
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

        #if ACTIVERT_ENABLE_STATS
        pool->stats.allocs_failed++;
        #endif

        // In ISR, we can only drop events
        return NULL;
    }

    // Mark event as allocated
    mark_event_allocated(pool, event_idx);

    // Get event pointer
    activert_event_t* event = get_event_at_index(pool, event_idx);

    // Initialize event
    memset(event, 0, pool->event_size);
    event->pool = pool;

    // Update statistics
    #if ACTIVERT_ENABLE_STATS
    pool->stats.allocs_succeeded++;
    pool->stats.current_allocated++;
    if (pool->stats.current_allocated > pool->stats.peak_allocated) {
        pool->stats.peak_allocated = pool->stats.current_allocated;
    }
    #endif

    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

    return event;
}

/*******************************************************************************
* Event Deallocation
*******************************************************************************/

void activert_event_pool_free(activert_event_t* event) {
    ACTIVERT_ASSERT(event != NULL);
    
    // Check if event has pool pointer
    if (event->pool == NULL) {
        // Dynamically allocated event
        #if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
        ACTIVERT_FREE(event);
        #endif
        return;
    }
    
    activert_event_pool_t* pool = event->pool;
    
    // Take mutex
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    // Get event index
    int event_idx = get_event_index(pool, event);
    
    if (event_idx < 0) {
        // Event not in pool - error!
        xSemaphoreGive(pool->mutex);
        ACTIVERT_ASSERT(0);
        return;
    }
    
    // Mark event as free
    mark_event_free(pool, event_idx);
    
    // Update statistics
    #if ACTIVERT_ENABLE_STATS
    pool->stats.frees++;
    pool->stats.current_allocated--;
    #endif
    
    xSemaphoreGive(pool->mutex);
}

void activert_event_pool_free_from_isr(activert_event_t* event) {
    ACTIVERT_ASSERT(event != NULL);

    // Check if event has pool pointer
    if (event->pool == NULL) {
        // Dynamically allocated event - can't free from ISR safely
        ACTIVERT_ASSERT(0);
        return;
    }

    activert_event_pool_t* pool = event->pool;

    // Use critical section — xSemaphoreTakeFromISR does not support mutexes
    UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    // Get event index
    int event_idx = get_event_index(pool, event);

    if (event_idx < 0) {
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
        ACTIVERT_ASSERT(0);
        return;
    }

    // Mark event as free
    mark_event_free(pool, event_idx);

    // Update statistics
    #if ACTIVERT_ENABLE_STATS
    pool->stats.frees++;
    pool->stats.current_allocated--;
    #endif

    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

/*******************************************************************************
* Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

size_t activert_event_pool_get_free_count(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    return pool->pool_size - pool->stats.current_allocated;
}

size_t activert_event_pool_get_peak_usage(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    return pool->stats.peak_allocated;
}

uint32_t activert_event_pool_get_alloc_attempts(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    return pool->stats.allocs_attempted;
}

uint32_t activert_event_pool_get_alloc_failures(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    return pool->stats.allocs_failed;
}

void activert_event_pool_reset_stats(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    
    xSemaphoreTake(pool->mutex, portMAX_DELAY);
    
    // Reset counters but keep current_allocated
    uint32_t current = pool->stats.current_allocated;
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.current_allocated = current;
    pool->stats.peak_allocated = current;
    
    xSemaphoreGive(pool->mutex);
}

void activert_event_pool_print_stats(activert_event_pool_t* pool) {
    ACTIVERT_ASSERT(pool != NULL);
    
    xSemaphoreTake(pool->mutex, portMAX_DELAY);
    
    printf("================================================================\n");
    #if ACTIVERT_ENABLE_NAMES
    printf("Event Pool: %s\n", pool->name ? pool->name : "unnamed");
    #else
    printf("Event Pool\n");
    #endif
    printf("================================================================\n");
    printf("Size:           %zu events x %zu bytes = %zu bytes\n",
           pool->pool_size, pool->event_size, pool->pool_size * pool->event_size);
    printf("Current:        %u / %zu (%u%%)\n",
           pool->stats.current_allocated, pool->pool_size,
           (uint32_t)((pool->stats.current_allocated * 100) / pool->pool_size));
    printf("Peak:           %u / %zu (%u%%)\n",
           pool->stats.peak_allocated, pool->pool_size,
           (uint32_t)((pool->stats.peak_allocated * 100) / pool->pool_size));
    printf("Allocations:    %u (%u OK, %u failed)\n",
           pool->stats.allocs_attempted,
           pool->stats.allocs_succeeded,
           pool->stats.allocs_failed);
    
    if (pool->stats.allocs_attempted > 0) {
        float success_rate = (float)pool->stats.allocs_succeeded * 100.0f / 
                            (float)pool->stats.allocs_attempted;
        printf("Success rate:   %.2f%%\n", success_rate);
    }
    
    printf("================================================================\n");
    
    xSemaphoreGive(pool->mutex);
}

#endif /* ACTIVERT_ENABLE_STATS */
