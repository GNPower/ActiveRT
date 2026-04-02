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
static int find_free_event(activert_event_pool_t* pool)
{
    size_t bitmap_bytes = (pool->pool_size + 7U) >> 3U;  // Divide by 8 to get byte count

    for (size_t byte_idx = 0; byte_idx < bitmap_bytes; byte_idx++)
    {
        uint8_t byte = pool->usage_bitmap[byte_idx];

        // Skip if all bits set (all events in this byte are allocated)
        if (byte == 0xFFU)
        {
            continue;
        }

        // Find first zero bit
        for (uint8_t bit_idx = 0; bit_idx < 8U; bit_idx++)
        {
            size_t event_idx = (byte_idx << 3U) + bit_idx;

            // Check if we've gone past the pool size
            if (event_idx >= pool->pool_size)
            {
                return -1;
            }

            // Check if this bit is zero (event is free)
            if (!(byte & (1U << bit_idx)))
            {
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
static void mark_event_allocated(activert_event_pool_t* pool, size_t event_idx)
{
    size_t byte_idx = event_idx >> 3U;
    uint8_t bit_idx = (uint8_t)(event_idx & 7U);
    pool->usage_bitmap[byte_idx] |= (1U << bit_idx);
}

/**
 * Mark event as free in bitmap
 *
 * @param pool          Pool
 * @param event_idx     Event index to mark
 */
static void mark_event_free(activert_event_pool_t* pool, size_t event_idx)
{
    size_t byte_idx = event_idx >> 3U;
    uint8_t bit_idx = (uint8_t)(event_idx & 7U);
    pool->usage_bitmap[byte_idx] &= ~(1U << bit_idx);
}

/**
 * Get pointer to event at index
 * 
 * @param pool          Pool
 * @param event_idx     Event index
 * @return              Pointer to event
 */
static activert_event_t* get_event_at_index(activert_event_pool_t* pool, size_t event_idx)
{
    /* cppcheck-suppress misra-c2012-11.5
     * Deviation: pool_memory is void* raw storage; uint8_t* is needed for
     * byte-level array indexing into the pool. */
    uint8_t* base = (uint8_t*)pool->pool_memory;
    void* addr =
        &base[(event_idx * pool->event_size)]; /* array subscript, not pointer arithmetic */
    /* cppcheck-suppress misra-c2012-11.5
     * Deviation: convert void* raw storage back to the typed event pointer.
     * Safe because the pool stores only activert_event_t-derived objects at
     * aligned offsets computed above. */
    return (activert_event_t*)addr;
}

/**
 * Get index of event in pool
 * 
 * @param pool          Pool
 * @param event         Event pointer
 * @return              Event index, or -1 if not in pool
 */
static int get_event_index(activert_event_pool_t* pool, const activert_event_t* event)
{
    /* cppcheck-suppress misra-c2012-11.5
     * Deviation: pool_memory is void* raw storage; uint8_t* needed for
     * byte-level array indexing and range comparison. */
    uint8_t* base = (uint8_t*)pool->pool_memory;
    /* cppcheck-suppress misra-c2012-11.3
     * Deviation: const activert_event_t* cast to const uint8_t* for byte-level
     * comparison to locate the event within the pool. const is preserved. */
    const uint8_t* evt_ptr  = (const uint8_t*)event;
    const uint8_t* pool_end = &base[(pool->pool_size * pool->event_size)];

    // Check if event is within pool memory range
    if ((evt_ptr < base) || (evt_ptr >= pool_end))
    {
        return -1;
    }

    // Calculate index — diff is non-negative here (range check above guarantees
    // evt_ptr >= base), so the cast to size_t is safe.  The cast is applied to
    // the named variable, not the composite subtraction expression, to satisfy
    // MISRA-C Rule 10.8.
    /* cppcheck-suppress misra-c2012-18.4
     * Deviation: pointer subtraction is the only standard C mechanism for
     * computing the byte distance between two pointers into the same array.
     * No array-subscript equivalent exists for subtraction. */
    ptrdiff_t diff   = evt_ptr - base;
    size_t offset    = (size_t)diff;
    size_t event_idx = offset / pool->event_size;

    // Verify alignment
    if ((offset % pool->event_size) != 0U)
    {
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
    ACTIVERT_ASSERT(pool_size > 0U);

    // Allocate pool structure
    /* cppcheck-suppress misra-c2012-11.5
     * Deviation: ACTIVERT_MALLOC returns void*; cast to the allocated type is
     * unavoidable in C dynamic allocation. */
    activert_event_pool_t* pool =
        (activert_event_pool_t*)ACTIVERT_MALLOC(sizeof(activert_event_pool_t));
    if (pool == NULL)
    {
        return NULL;
    }

    // Allocate bitmap (1 bit per event)
    size_t bitmap_bytes = (pool_size + 7U) >> 3U;  // Divide by 8 to get byte count
    /* cppcheck-suppress misra-c2012-11.5
     * Deviation: ACTIVERT_MALLOC returns void*; cast to uint8_t* for bitmap
     * byte array is unavoidable in C dynamic allocation. */
    pool->usage_bitmap = (uint8_t*)ACTIVERT_MALLOC(bitmap_bytes);
    if (pool->usage_bitmap == NULL)
    {
        ACTIVERT_FREE(pool);
        return NULL;
    }

    // Initialize bitmap (all zeros = all free)
    memset(pool->usage_bitmap, 0, bitmap_bytes);

    // Create mutex for thread-safe access
    pool->mutex = xSemaphoreCreateMutex();
    if (pool->mutex == NULL)
    {
        ACTIVERT_FREE(pool->usage_bitmap);
        ACTIVERT_FREE(pool);
        return NULL;
    }

// Initialize pool fields
#if ACTIVERT_ENABLE_NAMES
    pool->name = name;
#endif /* ACTIVERT_ENABLE_NAMES */
    pool->pool_memory = pool_memory;
    pool->event_size  = event_size;
    pool->pool_size   = pool_size;
    pool->policy      = policy;

// Initialize statistics
#if ACTIVERT_ENABLE_STATS
    memset(&pool->stats, 0, sizeof(pool->stats));
#endif /* ACTIVERT_ENABLE_STATS */

#if ACTIVERT_ENABLE_DEBUG
    printf(
        "activert_event_pool_create: Created pool '%s' with %zu events of %zu bytes each\n",
        name ? name : "unnamed",
        pool_size,
        event_size
    );
#endif /* ACTIVERT_ENABLE_DEBUG */

#if ACTIVERT_ENABLE_STATS
    activert_stats_register_pool(pool);
#endif /* ACTIVERT_ENABLE_STATS */

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
    ACTIVERT_ASSERT(pool_size > 0U);

    size_t bitmap_bytes = (pool_size + 7U) >> 3U;  // Divide by 8 to get byte count
    memset(bitmap, 0, bitmap_bytes);

    pool->mutex = xSemaphoreCreateMutexStatic(&pool->mutex_buffer);
    ACTIVERT_ASSERT(pool->mutex != NULL);

#if ACTIVERT_ENABLE_NAMES
    pool->name = name;
#else  /* ACTIVERT_ENABLE_NAMES */
    (void)name;
#endif /* ACTIVERT_ENABLE_NAMES */
    pool->pool_memory  = pool_memory;
    pool->usage_bitmap = bitmap;
    pool->event_size   = event_size;
    pool->pool_size    = pool_size;
    pool->policy       = policy;

#if ACTIVERT_ENABLE_STATS
    memset(&pool->stats, 0, sizeof(pool->stats));
#endif /* ACTIVERT_ENABLE_STATS */

#if ACTIVERT_ENABLE_DEBUG
    printf(
        "activert_event_pool_init_static: Initialized pool '%s' with %zu events of %zu bytes "
        "each\n",
        name ? name : "unnamed",
        pool_size,
        event_size
    );
#endif /* ACTIVERT_ENABLE_DEBUG */

#if ACTIVERT_ENABLE_STATS
    activert_stats_register_pool(pool);
#endif /* ACTIVERT_ENABLE_STATS */
}

#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION

activert_event_pool_t* activert_event_pool_create_dynamic(
    const char* name, size_t event_size, size_t pool_size, activert_pool_overflow_policy_t policy
)
{
    // Validate parameters
    ACTIVERT_ASSERT(event_size >= sizeof(activert_event_t));
    ACTIVERT_ASSERT(pool_size > 0);

    // Allocate pool memory
    void* pool_memory = ACTIVERT_MALLOC(pool_size * event_size);
    if (pool_memory == NULL)
    {
        return NULL;
    }

    // Create pool using static creation function
    activert_event_pool_t* pool =
        activert_event_pool_create(name, pool_memory, event_size, pool_size, policy);

    if (pool == NULL)
    {
        ACTIVERT_FREE(pool_memory);
        return NULL;
    }

    #if ACTIVERT_ENABLE_DEBUG
    printf(
        "activert_event_pool_create_dynamic: Created dynamic pool '%s'\n", name ? name : "unnamed"
    );
    #endif /* ACTIVERT_ENABLE_DEBUG */

    return pool;
}

void activert_event_pool_destroy(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);

    #if ACTIVERT_ENABLE_DEBUG
        #if ACTIVERT_ENABLE_NAMES
    printf(
        "activert_event_pool_destroy: Destroying pool '%s'\n", pool->name ? pool->name : "unnamed"
    );
        #endif /* ACTIVERT_ENABLE_NAMES */
    #endif     /* ACTIVERT_ENABLE_DEBUG */

    vSemaphoreDelete(pool->mutex);
    ACTIVERT_FREE(pool->usage_bitmap);
    ACTIVERT_FREE(pool->pool_memory);
    ACTIVERT_FREE(pool);
}

#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */

/*******************************************************************************
* Event Allocation
*******************************************************************************/

activert_event_t* activert_event_pool_alloc(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);

#if ACTIVERT_ENABLE_STATS
    pool->stats.allocs_attempted++;
#endif /* ACTIVERT_ENABLE_STATS */

    // Take mutex for thread-safe access
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE)
    {
        return NULL;
    }

    // Find free event
    int event_idx = find_free_event(pool);

    if (event_idx < 0)
    {
        // Pool exhausted
        xSemaphoreGive(pool->mutex);

#if ACTIVERT_ENABLE_STATS
        pool->stats.allocs_failed++;
#endif /* ACTIVERT_ENABLE_STATS */

#if ACTIVERT_ENABLE_POOL_OVERFLOW_DETECTION
    #if ACTIVERT_ENABLE_NAMES
        printf("WARNING: Event pool '%s' exhausted!\n", pool->name ? pool->name : "unnamed");
    #else  /* ACTIVERT_ENABLE_NAMES */
        printf("WARNING: Event pool exhausted!\n");
    #endif /* ACTIVERT_ENABLE_NAMES */
#endif     /* ACTIVERT_ENABLE_POOL_OVERFLOW_DETECTION */

        // Handle overflow based on policy
        switch (pool->policy)
        {
            case ACTIVERT_POOL_OVERFLOW_DROP:
                return NULL;

            case ACTIVERT_POOL_OVERFLOW_ASSERT:
                ACTIVERT_ASSERT(0);  // Trap
                return NULL;

            case ACTIVERT_POOL_OVERFLOW_DYNAMIC:
#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
                // Fall back to dynamic allocation
                {
                    activert_event_t* dyn_event =
                        (activert_event_t*)ACTIVERT_MALLOC(pool->event_size);
                    if (dyn_event != NULL)
                    {
                        dyn_event->pool = NULL;  // Mark as dynamically allocated
                    }
                    return dyn_event;
                }
#else
                return NULL;
#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */

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
    if (pool->stats.current_allocated > pool->stats.peak_allocated)
    {
        pool->stats.peak_allocated = pool->stats.current_allocated;
    }
#endif /* ACTIVERT_ENABLE_STATS */

    xSemaphoreGive(pool->mutex);

#if ACTIVERT_ENABLE_DEBUG
    printf(
        "activert_event_pool_alloc: Allocated event %d from pool '%s'\n",
        event_idx,
        pool->name ? pool->name : "unnamed"
    );
#endif /* ACTIVERT_ENABLE_DEBUG */

    return event;
}

activert_event_t* activert_event_pool_alloc_from_isr(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);

#if ACTIVERT_ENABLE_STATS
    pool->stats.allocs_attempted++;
#endif /* ACTIVERT_ENABLE_STATS */

    // Use critical section — xSemaphoreTakeFromISR does not support mutexes
    UBaseType_t ux_saved_interrupt_status = taskENTER_CRITICAL_FROM_ISR();

    // Find free event
    int event_idx = find_free_event(pool);

    if (event_idx < 0)
    {
        // Pool exhausted
        taskEXIT_CRITICAL_FROM_ISR(ux_saved_interrupt_status);

#if ACTIVERT_ENABLE_STATS
        pool->stats.allocs_failed++;
#endif /* ACTIVERT_ENABLE_STATS */

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
    if (pool->stats.current_allocated > pool->stats.peak_allocated)
    {
        pool->stats.peak_allocated = pool->stats.current_allocated;
    }
#endif /* ACTIVERT_ENABLE_STATS */

    taskEXIT_CRITICAL_FROM_ISR(ux_saved_interrupt_status);

    return event;
}

/*******************************************************************************
* Event Deallocation
*******************************************************************************/

void activert_event_pool_free(activert_event_t* event)
{
    ACTIVERT_ASSERT(event != NULL);

    // Check if event has pool pointer
    if (event->pool == NULL)
    {
// Dynamically allocated event
#if ACTIVERT_ENABLE_DYNAMIC_ALLOCATION
        ACTIVERT_FREE(event);
#endif /* ACTIVERT_ENABLE_DYNAMIC_ALLOCATION */
        return;
    }

    activert_event_pool_t* pool = event->pool;

    // Take mutex
    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE)
    {
        return;
    }

    // Get event index
    int event_idx = get_event_index(pool, event);

    if (event_idx < 0)
    {
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
#endif /* ACTIVERT_ENABLE_STATS */

    xSemaphoreGive(pool->mutex);
}

void activert_event_pool_free_from_isr(activert_event_t* event)
{
    ACTIVERT_ASSERT(event != NULL);

    // Check if event has pool pointer
    if (event->pool == NULL)
    {
        // Dynamically allocated event - can't free from ISR safely
        ACTIVERT_ASSERT(0);
        return;
    }

    activert_event_pool_t* pool = event->pool;

    // Use critical section — xSemaphoreTakeFromISR does not support mutexes
    UBaseType_t ux_saved_interrupt_status = taskENTER_CRITICAL_FROM_ISR();

    // Get event index
    int event_idx = get_event_index(pool, event);

    if (event_idx < 0)
    {
        taskEXIT_CRITICAL_FROM_ISR(ux_saved_interrupt_status);
        ACTIVERT_ASSERT(0);
        return;
    }

    // Mark event as free
    mark_event_free(pool, event_idx);

// Update statistics
#if ACTIVERT_ENABLE_STATS
    pool->stats.frees++;
    pool->stats.current_allocated--;
#endif /* ACTIVERT_ENABLE_STATS */

    taskEXIT_CRITICAL_FROM_ISR(ux_saved_interrupt_status);
}

/*******************************************************************************
* Statistics
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

size_t activert_event_pool_get_free_count(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);
    return pool->pool_size - pool->stats.current_allocated;
}

size_t activert_event_pool_get_peak_usage(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);
    return pool->stats.peak_allocated;
}

uint32_t activert_event_pool_get_alloc_attempts(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);
    return pool->stats.allocs_attempted;
}

uint32_t activert_event_pool_get_alloc_failures(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);
    return pool->stats.allocs_failed;
}

void activert_event_pool_reset_stats(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);

    xSemaphoreTake(pool->mutex, portMAX_DELAY);

    // Reset counters but keep current_allocated
    uint32_t current = pool->stats.current_allocated;
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.current_allocated = current;
    pool->stats.peak_allocated    = current;

    xSemaphoreGive(pool->mutex);
}

void activert_event_pool_print_stats(activert_event_pool_t* pool)
{
    ACTIVERT_ASSERT(pool != NULL);

    xSemaphoreTake(pool->mutex, portMAX_DELAY);

    printf("================================================================\n");
    #if ACTIVERT_ENABLE_NAMES
    printf("Event Pool: %s\n", pool->name ? pool->name : "unnamed");
    #else
    printf("Event Pool\n");
    #endif
    printf("================================================================\n");
    printf(
        "Size:           %zu events x %zu bytes = %zu bytes\n",
        pool->pool_size,
        pool->event_size,
        pool->pool_size * pool->event_size
    );
    printf(
        "Current:        %u / %zu (%u%%)\n",
        pool->stats.current_allocated,
        pool->pool_size,
        (uint32_t)(((size_t)pool->stats.current_allocated * 100U) / pool->pool_size)
    );
    printf(
        "Peak:           %u / %zu (%u%%)\n",
        pool->stats.peak_allocated,
        pool->pool_size,
        (uint32_t)(((size_t)pool->stats.peak_allocated * 100U) / pool->pool_size)
    );
    printf(
        "Allocations:    %u (%u OK, %u failed)\n",
        pool->stats.allocs_attempted,
        pool->stats.allocs_succeeded,
        pool->stats.allocs_failed
    );

    if (pool->stats.allocs_attempted > 0U)
    {
        float success_rate =
            (float)pool->stats.allocs_succeeded * 100.0F / (float)pool->stats.allocs_attempted;
        printf("Success rate:   %.2f%%\n", success_rate);
    }

    printf("================================================================\n");

    xSemaphoreGive(pool->mutex);
}

#endif /* ACTIVERT_ENABLE_STATS */
