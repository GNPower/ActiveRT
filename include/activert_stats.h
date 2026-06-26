/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_stats.h
*   @brief      Statistics API
*   @author     Graham N. Power
*   @date       2025-12-27
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.5.0   gnp     2025-12-27  Initial statistics API and global registry
*   0.6.0   gnp     2026-01-10  Health check, performance summary, monitoring callbacks
*   1.0.0   gnp     2026-02-28  Telemetry export, statistics reset, full report print
*
*******************************************************************************/

#ifndef ACTIVERT_STATS_H
#define ACTIVERT_STATS_H

#include "activert_types.h"

#if ACTIVERT_ENABLE_STATS

/*******************************************************************************
* Global Statistics Registry
*******************************************************************************/

/**
 * Register Active Object for global statistics tracking
 * 
 * Called automatically by activert_active_create_static/dynamic.
 * Allows system-wide statistics queries.
 * 
 * @param active        Active Object to register
 * @return              0 on success, -1 on error (registry full)
 */
int activert_stats_register_active(activert_active_t* active);

/**
 * Unregister Active Object from global statistics
 * 
 * Called automatically by activert_active_destroy.
 * 
 * @param active        Active Object to unregister
 */
void activert_stats_unregister_active(activert_active_t* active);

/**
 * Register Event Pool for global statistics tracking
 * 
 * Called automatically by activert_event_pool_create/create_dynamic/init_static.
 * 
 * @param pool          Event Pool to register
 * @return              0 on success, -1 on error (registry full)
 */
int activert_stats_register_pool(activert_event_pool_t* pool);

/**
 * Unregister Event Pool from global statistics
 * 
 * Called automatically by activert_event_pool_destroy.
 * 
 * @param pool          Event Pool to unregister
 */
void activert_stats_unregister_pool(activert_event_pool_t* pool);

/*******************************************************************************
* System-Wide Statistics
*******************************************************************************/

/**
 * Get number of registered Active Objects
 * 
 * @return              Count of Active Objects in system
 */
size_t activert_stats_get_active_count(void);

/**
 * Get number of registered Event Pools
 *
 * @return              Count of Event Pools in system
 */
size_t activert_stats_get_pool_count(void);

/**
 * Get registered Active Object by index
 *
 * @param index         Index (0-based, up to get_active_count()-1)
 * @return              Pointer to Active Object, or NULL if index out of range
 */
activert_active_t* activert_stats_get_active(size_t index);

/**
 * Get registered Event Pool by index
 *
 * @param index         Index (0-based, up to get_pool_count()-1)
 * @return              Pointer to Event Pool, or NULL if index out of range
 */
activert_event_pool_t* activert_stats_get_pool(size_t index);

/**
 * Get total events processed across all Active Objects
 * 
 * @return              Total event count
 */
uint32_t activert_stats_get_total_events_processed(void);

/**
 * Get total events dropped across all Active Objects
 * 
 * @return              Total dropped count
 */
uint32_t activert_stats_get_total_events_dropped(void);

/**
 * Get total notifications received across all Active Objects
 * 
 * @return              Total notification count
 */
uint32_t activert_stats_get_total_notifications(void);

/**
 * Get total event pool allocations
 * 
 * @return              Total allocation attempts
 */
uint32_t activert_stats_get_total_pool_allocs(void);

/**
 * Get total event pool allocation failures
 * 
 * @return              Total allocation failures
 */
uint32_t activert_stats_get_total_pool_failures(void);

/*******************************************************************************
* Health Monitoring
*******************************************************************************/

/**
 * System health status
 */
typedef enum
{
    ACTIVERT_HEALTH_OK,      /**< System healthy */
    ACTIVERT_HEALTH_WARNING, /**< Some warnings detected */
    ACTIVERT_HEALTH_CRITICAL /**< Critical issues detected */
} activert_health_status_t;

/**
 * Health check result
 */
typedef struct
{
    activert_health_status_t status;
    uint32_t warnings;
    uint32_t criticals;

    // Warning flags
    bool high_queue_utilization; /**< Any queue >80% full */
    bool pool_exhaustion;        /**< Any pool exhausted */
    bool high_drop_rate;         /**< Event drop rate >5% */
    bool low_stack;              /**< Any task stack <512 bytes free */

    // Critical flags
    bool queue_overflow;      /**< Any queue overflowed */
    bool pool_critical;       /**< Pool allocation failure rate >50% */
    bool stack_overflow_risk; /**< Task stack <256 bytes free */
} activert_health_check_t;

/**
 * Perform system-wide health check
 * 
 * Checks all registered Active Objects and Event Pools for issues.
 * 
 * @param result        Output: health check result
 * @return              0 on success, -1 on error
 */
int activert_stats_health_check(activert_health_check_t* result);

    /*******************************************************************************
* Performance Profiling
*******************************************************************************/

    #if ACTIVERT_ENABLE_TIMING_STATS

/**
 * Performance summary
 */
typedef struct
{
    uint32_t total_events;
    TickType_t total_processing_time;
    TickType_t avg_processing_time;
    TickType_t max_processing_time;
    activert_signal_t slowest_signal;
        #if ACTIVERT_ENABLE_NAMES
    const char* slowest_task_name;
        #endif
} activert_perf_summary_t;

/**
 * Get system-wide performance summary
 * 
 * @param summary       Output: performance summary
 * @return              0 on success, -1 on error
 */
int activert_stats_get_perf_summary(activert_perf_summary_t* summary);

/**
 * Find slowest Active Object
 * 
 * @return              Active Object with highest max processing time, or NULL
 */
activert_active_t* activert_stats_find_slowest_active(void);

/**
 * Find most loaded Active Object (highest event count)
 * 
 * @return              Active Object with most events processed, or NULL
 */
activert_active_t* activert_stats_find_busiest_active(void);

    #endif /* ACTIVERT_ENABLE_TIMING_STATS */

/*******************************************************************************
* Report Generation
*******************************************************************************/

/**
 * Print system-wide statistics summary
 * 
 * Prints:
 * - Active Object count and total events
 * - Event Pool count and allocation stats
 * - Health check status
 * - Top 5 busiest tasks
 * - Top 5 most-used pools
 */
void activert_stats_print_summary(void);

/**
 * Print detailed statistics for all Active Objects
 */
void activert_stats_print_all_actives(void);

/**
 * Print detailed statistics for all Event Pools
 */
void activert_stats_print_all_pools(void);

/**
 * Print full system report
 * 
 * Comprehensive report including:
 * - System summary
 * - Health check
 * - All Active Objects
 * - All Event Pools
 * - Performance analysis
 */
void activert_stats_print_full_report(void);

/*******************************************************************************
* Real-Time Monitoring
*******************************************************************************/

/**
 * Monitoring callback function type
 * 
 * Called when monitored condition is detected.
 * 
 * @param active        Active Object (or NULL if pool-related)
 * @param pool          Event Pool (or NULL if Active Object-related)
 * @param message       Description of condition
 */
typedef void (*activert_monitor_callback_t)(
    activert_active_t* active, activert_event_pool_t* pool, const char* message
);

/**
 * Enable queue depth monitoring
 * 
 * Triggers callback when any queue exceeds threshold.
 * 
 * @param threshold_percent    Queue depth threshold (0-100%)
 * @param callback             Callback function
 */
void activert_stats_monitor_queue_depth(
    uint8_t threshold_percent, activert_monitor_callback_t callback
);

/**
 * Enable pool exhaustion monitoring
 * 
 * Triggers callback when pool allocation fails.
 * 
 * @param callback             Callback function
 */
void activert_stats_monitor_pool_exhaustion(activert_monitor_callback_t callback);

/**
 * Enable stack usage monitoring
 * 
 * Triggers callback when task stack drops below threshold.
 * 
 * @param threshold_bytes      Free stack threshold (bytes)
 * @param callback             Callback function
 */
void activert_stats_monitor_stack_usage(
    uint32_t threshold_bytes, activert_monitor_callback_t callback
);

/**
 * Disable all monitoring
 */
void activert_stats_disable_monitoring(void);

/*******************************************************************************
* Statistics Reset
*******************************************************************************/

/**
 * Reset statistics for specific Active Object
 * 
 * @param active        Active Object to reset
 */
void activert_stats_reset_active(activert_active_t* active);

/**
 * Reset statistics for specific Event Pool
 * 
 * @param pool          Event Pool to reset
 */
void activert_stats_reset_pool(activert_event_pool_t* pool);

/**
 * Reset all statistics system-wide
 * 
 * Resets counters for all Active Objects and Event Pools.
 * Does not reset high water marks.
 */
void activert_stats_reset_all(void);

/*******************************************************************************
* Export/Import (for logging systems)
*******************************************************************************/

/**
 * Export statistics to binary buffer
 * 
 * Useful for telemetry downlink or storage.
 * 
 * @param buffer        Output buffer
 * @param buffer_size   Buffer size
 * @return              Bytes written, or -1 on error
 */
int activert_stats_export(uint8_t* buffer, size_t buffer_size);

/**
 * Get export buffer size required
 * 
 * @return              Required buffer size in bytes
 */
size_t activert_stats_get_export_size(void);

#endif /* ACTIVERT_ENABLE_STATS */

#endif /* ACTIVERT_STATS_H */
