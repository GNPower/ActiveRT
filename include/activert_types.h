/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_types.h
*   @brief      Core Type Definitions
*   @author     Graham N. Power
*   @date       2025-11-01
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.1.0   gnp     2025-11-01  Initial type definitions for event, pool, and AO
*   0.2.0   gnp     2025-11-15  Packed bitmap pool; overflow policy enum
*   0.4.0   gnp     2025-12-13  Multi-queue types, QueueSet handle, queue config
*   0.5.0   gnp     2025-12-27  Statistics structs for AO and pool
*   0.7.0   gnp     2026-01-24  Notification struct; notify handler typedef
*   1.0.0   gnp     2026-02-28  is_static flag; static_mem tracking; loop_fn_t typedef
*
*******************************************************************************/

#ifndef ACTIVERT_TYPES_H
#define ACTIVERT_TYPES_H

#include "activert_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*******************************************************************************
* Forward Declarations
*******************************************************************************/

typedef struct activert_event activert_event_t;
typedef struct activert_event_pool activert_event_pool_t;
typedef struct activert_active activert_active_t;
typedef struct activert_queue activert_queue_t;
typedef struct activert_queue_config activert_queue_config_t;

/*******************************************************************************
* Signal Type
*******************************************************************************/

/**
 * Signal type
 */
typedef uint16_t activert_signal_t;

/*******************************************************************************
* Event Structure
*******************************************************************************/

/**
 * Base event structure
 * 
 * CRITICAL: This must be the first member of all event types!
 */
struct activert_event {
    activert_signal_t sig;              /**< Event signal */
    activert_event_pool_t* pool;        /**< Owning pool (NULL if malloc'd) */
};

/*******************************************************************************
* Event Pool Overflow Policies
*******************************************************************************/

/**
 * Event pool overflow policy
 */
typedef enum {
    ACTIVERT_POOL_OVERFLOW_DROP,        /**< Drop allocation (return NULL) */
    ACTIVERT_POOL_OVERFLOW_ASSERT,      /**< Assert on overflow (debug only) */
    ACTIVERT_POOL_OVERFLOW_DYNAMIC      /**< Fall back to malloc (not recommended) */
} activert_pool_overflow_policy_t;

/*******************************************************************************
* Event Pool Statistics
*******************************************************************************/

/**
 * Event pool statistics
 */
typedef struct {
    uint32_t allocs_attempted;          /**< Total allocation attempts */
    uint32_t allocs_succeeded;          /**< Successful allocations */
    uint32_t allocs_failed;             /**< Failed allocations */
    uint32_t current_allocated;         /**< Currently allocated count */
    uint32_t peak_allocated;            /**< Peak allocated count */
    uint32_t frees;                     /**< Total frees */
} activert_event_pool_stats_t;

/*******************************************************************************
* Event Pool Structure
*******************************************************************************/

/**
 * Event pool structure
 */
struct activert_event_pool {
    void* pool_memory;                  /**< Event storage array */
    uint8_t* usage_bitmap;             /**< Allocation bitmap */
    size_t event_size;                  /**< Size of each event */
    size_t pool_size;                   /**< Number of events in pool */
    size_t bitmap_size;                 /**< Bitmap size in uint32_t */
    activert_pool_overflow_policy_t policy; /**< Overflow policy */
    SemaphoreHandle_t mutex;            /**< Thread safety mutex */
    StaticSemaphore_t mutex_buffer;     /**< Mutex static storage */
    #if ACTIVERT_ENABLE_STATS
    activert_event_pool_stats_t stats;  /**< Statistics */
    #endif
    #if ACTIVERT_ENABLE_NAMES
    const char* name;                   /**< Pool name (for debugging) */
    #endif
};

/*******************************************************************************
* Active Object Handlers
*******************************************************************************/

/**
 * Active Object dispatch handler
 */
typedef void (*activert_dispatch_handler_t)(
    activert_active_t* me,
    const activert_event_t* e
);

/**
 * Active Object notification handler
 */
typedef void (*activert_notify_handler_t)(
    activert_active_t* me, 
    uint32_t bits
);

/**
 * Loop task function (called repeatedly in while(1) by loop tasks)
 */
typedef void (*activert_loop_fn_t)(activert_active_t* me);

/*******************************************************************************
* Notification Structure
*******************************************************************************/

/**
 * Notification handler info
 */
typedef struct {
    activert_notify_handler_t handler;  /**< Notification handler (NULL if none) */
    uint32_t notify_mask;               /**< Notification mask */

    /* Semaphore-based notification (for queue+notification tasks) */
    SemaphoreHandle_t semaphore;        /**< Semaphore for queue set integration (NULL if not used) */
    StaticSemaphore_t* semaphore_cb;    /**< Static semaphore storage (NULL if dynamic) */
    uint32_t pending_value;             /**< Pending notification bits */
} activert_notification_t;

/*******************************************************************************
* Queue Configuration
*******************************************************************************/

/**
 * Queue configuration (passed to initialization functions)
 */
struct activert_queue_config {
    activert_signal_t signal_base;      /**< Base signal for this queue */
    uint16_t signal_count;              /**< Number of signals routed to this queue */
    UBaseType_t queue_length;           /**< Queue depth */
    activert_event_pool_t* event_pool;  /**< Associated event pool (optional) */
};

/*******************************************************************************
* Queue Statistics
*******************************************************************************/

/**
 * Per-queue statistics
 */
typedef struct {
    uint32_t posts_attempted;           /**< Total post attempts */
    uint32_t posts_succeeded;           /**< Successful posts */
    uint32_t posts_failed;              /**< Failed posts (queue full) */
    UBaseType_t current_depth;          /**< Current queue depth */
    UBaseType_t peak_depth;             /**< Peak queue depth */
} activert_queue_stats_t;

/*******************************************************************************
* Queue Structure
*******************************************************************************/

/**
 * Queue structure for Active Object
 */
struct activert_queue {
    QueueHandle_t handle;               /**< FreeRTOS queue handle */
    StaticQueue_t queue_buffer;         /**< Static queue storage */
    uint8_t* storage;                   /**< Queue item storage */
    
    /* Queue configuration (copied from activert_queue_config_t during init) */
    activert_signal_t signal_base;      /**< Base signal for this queue */
    uint16_t signal_count;              /**< Number of signals routed here */
    UBaseType_t queue_length;           /**< Queue depth */
    activert_event_pool_t* event_pool;  /**< Associated event pool (optional) */
    
    #if ACTIVERT_ENABLE_STATS
    activert_queue_stats_t stats;       /**< Queue statistics */
    #endif
};

/*******************************************************************************
* Active Object Statistics
*******************************************************************************/

/**
 * Active Object statistics
 */
typedef struct {
    uint32_t events_processed;          /**< Total events processed */
    uint32_t events_dropped;            /**< Events dropped (queue full) */
    uint32_t notifications_received;    /**< Task notifications received */
    #if ACTIVERT_ENABLE_TIMING_STATS
    TickType_t total_processing_time;   /**< Cumulative processing time */
    TickType_t avg_processing_time;     /**< Average processing time */
    TickType_t max_processing_time;     /**< Maximum processing time */
    activert_signal_t slowest_signal;   /**< Signal with max processing time */
    #endif
} activert_active_stats_t;

/*******************************************************************************
* Static Memory Tracking
*******************************************************************************/

/**
 * Static memory buffers for Active Object
 * 
 * When an Active Object is created with static allocation, pointers to
 * the user-provided memory buffers are stored here for tracking purposes.
 */
typedef struct {
    StaticTask_t* thread_cb;            /**< Task control block */
    StaticQueue_t* queue_cbs;           /**< Queue control blocks array */
    StaticQueue_t* queue_set_cb;        /**< Queue set control block */
    uint8_t* queue_set_storage;         /**< Queue set storage buffer */
} activert_static_mem_t;

/*******************************************************************************
* Active Object Structure
*******************************************************************************/

/**
 * Active Object structure
 */
struct activert_active {
    TaskHandle_t thread;                /**< FreeRTOS task handle */
    QueueSetHandle_t queue_set;         /**< Queue set (for multi-queue) */
    StaticQueue_t queue_set_buffer;     /**< Queue set static storage */
    activert_queue_t* queues;           /**< Array of queues */
    uint8_t queue_count;                /**< Number of queues */
    UBaseType_t priority;               /**< Task priority */
    activert_dispatch_handler_t dispatch; /**< Event dispatch handler */
    activert_loop_fn_t loop_fn;           /**< Loop function (NULL for queue/notify tasks) */
    activert_notification_t notification; /**< Notification handler info */
    
    /* Static memory tracking */
    bool is_static;                     /**< True if created with static allocation */
    activert_static_mem_t static_mem;   /**< Static memory buffers (if is_static) */

    /* User-defined context */
    void* context;                      /**< User-defined context pointer (accessible from handlers) */

    #if ACTIVERT_ENABLE_STATS
    activert_active_stats_t stats;      /**< Statistics */
    #endif
    #if ACTIVERT_ENABLE_NAMES
    const char* name;                   /**< Task name */
    #endif
};

#endif /* ACTIVERT_TYPES_H */
