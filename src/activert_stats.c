/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_stats.c
*   @brief      Statistics and Instrumentation Implementation
*   @author     Graham N. Power
*   @date       2025-12-27
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.5.0   gnp     2025-12-27  Initial statistics registry with AO and pool tracking
*   0.6.0   gnp     2026-01-10  Health check, performance summary, monitoring callbacks
*   1.0.0   gnp     2026-02-28  Telemetry export; statistics reset API
*
*******************************************************************************/

#include "activert_stats.h"
#include "activert_active.h"
#include "activert_event.h"
#include "activert_queue.h"
#include <stdio.h>
#include <string.h>

#if ACTIVERT_ENABLE_STATS

/*******************************************************************************
* Configuration
*******************************************************************************/

#ifndef ACTIVERT_MAX_REGISTERED_ACTIVES
#define ACTIVERT_MAX_REGISTERED_ACTIVES 32
#endif

#ifndef ACTIVERT_MAX_REGISTERED_POOLS
#define ACTIVERT_MAX_REGISTERED_POOLS 32
#endif

/*******************************************************************************
* Global Registry
*******************************************************************************/

static struct {
    activert_active_t* actives[ACTIVERT_MAX_REGISTERED_ACTIVES];
    size_t active_count;
    
    activert_event_pool_t* pools[ACTIVERT_MAX_REGISTERED_POOLS];
    size_t pool_count;
    
    // Monitoring callbacks
    activert_monitor_callback_t queue_monitor;
    uint8_t queue_threshold;
    
    activert_monitor_callback_t pool_monitor;
    activert_monitor_callback_t stack_monitor;
    uint32_t stack_threshold;
    
} g_stats_registry = {0};

/*******************************************************************************
* Registration
*******************************************************************************/

int activert_stats_register_active(activert_active_t* active) {
    if (!active) {
        return -1;
    }
    
    if (g_stats_registry.active_count >= ACTIVERT_MAX_REGISTERED_ACTIVES) {
        printf("ERROR: Active Object registry full\n");
        return -1;
    }
    
    g_stats_registry.actives[g_stats_registry.active_count++] = active;
    return 0;
}

void activert_stats_unregister_active(activert_active_t* active) {
    if (!active) {
        return;
    }
    
    // Find and remove
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        if (g_stats_registry.actives[i] == active) {
            // Shift remaining entries
            for (size_t j = i; j < g_stats_registry.active_count - 1; j++) {
                g_stats_registry.actives[j] = g_stats_registry.actives[j + 1];
            }
            g_stats_registry.active_count--;
            break;
        }
    }
}

int activert_stats_register_pool(activert_event_pool_t* pool) {
    if (!pool) {
        return -1;
    }
    
    if (g_stats_registry.pool_count >= ACTIVERT_MAX_REGISTERED_POOLS) {
        printf("ERROR: Event Pool registry full\n");
        return -1;
    }
    
    g_stats_registry.pools[g_stats_registry.pool_count++] = pool;
    return 0;
}

void activert_stats_unregister_pool(activert_event_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    // Find and remove
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        if (g_stats_registry.pools[i] == pool) {
            // Shift remaining entries
            for (size_t j = i; j < g_stats_registry.pool_count - 1; j++) {
                g_stats_registry.pools[j] = g_stats_registry.pools[j + 1];
            }
            g_stats_registry.pool_count--;
            break;
        }
    }
}

/*******************************************************************************
* System-Wide Statistics
*******************************************************************************/

size_t activert_stats_get_active_count(void) {
    return g_stats_registry.active_count;
}

size_t activert_stats_get_pool_count(void) {
    return g_stats_registry.pool_count;
}

activert_active_t* activert_stats_get_active(size_t index) {
    if (index >= g_stats_registry.active_count) {
        return NULL;
    }
    return g_stats_registry.actives[index];
}

activert_event_pool_t* activert_stats_get_pool(size_t index) {
    if (index >= g_stats_registry.pool_count) {
        return NULL;
    }
    return g_stats_registry.pools[index];
}

uint32_t activert_stats_get_total_events_processed(void) {
    uint32_t total = 0;
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        total += g_stats_registry.actives[i]->stats.events_processed;
    }
    
    return total;
}

uint32_t activert_stats_get_total_events_dropped(void) {
    uint32_t total = 0;
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        total += g_stats_registry.actives[i]->stats.events_dropped;
    }
    
    return total;
}

uint32_t activert_stats_get_total_notifications(void) {
    uint32_t total = 0;
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        total += g_stats_registry.actives[i]->stats.notifications_received;
    }
    
    return total;
}

uint32_t activert_stats_get_total_pool_allocs(void) {
    uint32_t total = 0;
    
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        total += g_stats_registry.pools[i]->stats.allocs_attempted;
    }
    
    return total;
}

uint32_t activert_stats_get_total_pool_failures(void) {
    uint32_t total = 0;
    
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        total += g_stats_registry.pools[i]->stats.allocs_failed;
    }
    
    return total;
}

/*******************************************************************************
* Health Monitoring
*******************************************************************************/

int activert_stats_health_check(activert_health_check_t* result) {
    if (!result) {
        return -1;
    }
    
    memset(result, 0, sizeof(activert_health_check_t));
    result->status = ACTIVERT_HEALTH_OK;
    
    // Check all Active Objects
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        activert_active_t* active = g_stats_registry.actives[i];
        
        // Check queue utilization
        for (uint8_t q = 0; q < active->queue_count; q++) {
            uint8_t util = activert_queue_get_utilization(active, q);
            
            if (util > 80) {
                result->high_queue_utilization = true;
                result->warnings++;
                if (result->status < ACTIVERT_HEALTH_WARNING) {
                    result->status = ACTIVERT_HEALTH_WARNING;
                }
            }
            
            if (util >= 100) {
                result->queue_overflow = true;
                result->criticals++;
                result->status = ACTIVERT_HEALTH_CRITICAL;
            }
        }
        
        // Check drop rate
        if (active->stats.events_processed > 100) {
            float drop_rate = (float)active->stats.events_dropped * 100.0f /
                             (float)(active->stats.events_processed + active->stats.events_dropped);
            
            if (drop_rate > 5.0f) {
                result->high_drop_rate = true;
                result->warnings++;
                if (result->status < ACTIVERT_HEALTH_WARNING) {
                    result->status = ACTIVERT_HEALTH_WARNING;
                }
            }
        }
        
        // Check stack usage
        UBaseType_t stack_free = uxTaskGetStackHighWaterMark(active->thread);
        if (stack_free < 128) {
            result->stack_overflow_risk = true;
            result->criticals++;
            result->status = ACTIVERT_HEALTH_CRITICAL;
        } else if (stack_free < 256) {
            result->low_stack = true;
            result->warnings++;
            if (result->status < ACTIVERT_HEALTH_WARNING) {
                result->status = ACTIVERT_HEALTH_WARNING;
            }
        }
    }
    
    // Check all Event Pools
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        activert_event_pool_t* pool = g_stats_registry.pools[i];
        
        // Check for exhaustion
        if (pool->stats.allocs_failed > 0) {
            result->pool_exhaustion = true;
            result->warnings++;
            if (result->status < ACTIVERT_HEALTH_WARNING) {
                result->status = ACTIVERT_HEALTH_WARNING;
            }
        }
        
        // Check failure rate
        if (pool->stats.allocs_attempted > 100) {
            float fail_rate = (float)pool->stats.allocs_failed * 100.0f /
                             (float)pool->stats.allocs_attempted;
            
            if (fail_rate > 50.0f) {
                result->pool_critical = true;
                result->criticals++;
                result->status = ACTIVERT_HEALTH_CRITICAL;
            }
        }
    }
    
    return 0;
}

/*******************************************************************************
* Performance Profiling
*******************************************************************************/

#if ACTIVERT_ENABLE_TIMING_STATS

int activert_stats_get_perf_summary(activert_perf_summary_t* summary) {
    if (!summary) {
        return -1;
    }
    
    memset(summary, 0, sizeof(activert_perf_summary_t));
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        activert_active_t* active = g_stats_registry.actives[i];
        
        summary->total_events += active->stats.events_processed;
        summary->total_processing_time += active->stats.total_processing_time;
        
        if (active->stats.max_processing_time > summary->max_processing_time) {
            summary->max_processing_time = active->stats.max_processing_time;
            summary->slowest_signal = active->stats.slowest_signal;
            #if ACTIVERT_ENABLE_NAMES
            summary->slowest_task_name = active->name;
            #endif
        }
    }
    
    if (summary->total_events > 0) {
        summary->avg_processing_time = summary->total_processing_time / summary->total_events;
    }
    
    return 0;
}

activert_active_t* activert_stats_find_slowest_active(void) {
    activert_active_t* slowest = NULL;
    TickType_t max_time = 0;
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        if (g_stats_registry.actives[i]->stats.max_processing_time > max_time) {
            max_time = g_stats_registry.actives[i]->stats.max_processing_time;
            slowest = g_stats_registry.actives[i];
        }
    }
    
    return slowest;
}

activert_active_t* activert_stats_find_busiest_active(void) {
    activert_active_t* busiest = NULL;
    uint32_t max_events = 0;
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        if (g_stats_registry.actives[i]->stats.events_processed > max_events) {
            max_events = g_stats_registry.actives[i]->stats.events_processed;
            busiest = g_stats_registry.actives[i];
        }
    }
    
    return busiest;
}

#endif /* ACTIVERT_ENABLE_TIMING_STATS */

/*******************************************************************************
* Report Generation
*******************************************************************************/

void activert_stats_print_summary(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("ActiveRT System Summary\n");
    printf("================================================================================\n");
    
    printf("Active Objects:     %zu\n", g_stats_registry.active_count);
    printf("Event Pools:        %zu\n", g_stats_registry.pool_count);
    printf("Events Processed:   %u\n", activert_stats_get_total_events_processed());
    printf("Events Dropped:     %u\n", activert_stats_get_total_events_dropped());
    printf("Notifications:      %u\n", activert_stats_get_total_notifications());
    printf("Pool Allocations:   %u\n", activert_stats_get_total_pool_allocs());
    printf("Pool Failures:      %u\n", activert_stats_get_total_pool_failures());
    
    // Health check
    activert_health_check_t health;
    activert_stats_health_check(&health);
    
    printf("\nHealth Status:      ");
    switch (health.status) {
        case ACTIVERT_HEALTH_OK:
            printf("OK\n");
            break;
        case ACTIVERT_HEALTH_WARNING:
            printf("WARNING (%u warnings)\n", health.warnings);
            break;
        case ACTIVERT_HEALTH_CRITICAL:
            printf("CRITICAL (%u warnings, %u critical)\n", 
                   health.warnings, health.criticals);
            break;
    }
    
    if (health.warnings > 0 || health.criticals > 0) {
        printf("\nIssues Detected:\n");
        if (health.high_queue_utilization) printf("  - High queue utilization (>80%%)\n");
        if (health.pool_exhaustion) printf("  - Pool exhaustion detected\n");
        if (health.high_drop_rate) printf("  - High event drop rate (>5%%)\n");
        if (health.low_stack) printf("  - Low stack space (<256 bytes)\n");
        if (health.queue_overflow) printf("  - CRITICAL: Queue overflow\n");
        if (health.pool_critical) printf("  - CRITICAL: Pool failure rate >50%%\n");
        if (health.stack_overflow_risk) printf("  - CRITICAL: Stack overflow risk (<128 bytes)\n");
    }
    
    printf("================================================================================\n");
}

void activert_stats_print_all_actives(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("All Active Objects (%zu)\n", g_stats_registry.active_count);
    printf("================================================================================\n");
    
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        activert_active_print_stats(g_stats_registry.actives[i]);
        printf("\n");
    }
}

void activert_stats_print_all_pools(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("All Event Pools (%zu)\n", g_stats_registry.pool_count);
    printf("================================================================================\n");
    
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        activert_event_pool_print_stats(g_stats_registry.pools[i]);
        printf("\n");
    }
}

void activert_stats_print_full_report(void) {
    activert_stats_print_summary();
    activert_stats_print_all_actives();
    activert_stats_print_all_pools();
    
    #if ACTIVERT_ENABLE_TIMING_STATS
    printf("\n");
    printf("================================================================================\n");
    printf("Performance Analysis\n");
    printf("================================================================================\n");
    
    activert_perf_summary_t perf;
    activert_stats_get_perf_summary(&perf);
    
    printf("Total Events:       %u\n", perf.total_events);
    printf("Avg Processing:     %u ticks\n", (unsigned int)perf.avg_processing_time);
    printf("Max Processing:     %u ticks\n", (unsigned int)perf.max_processing_time);
    printf("Slowest Signal:     %u\n", perf.slowest_signal);
    #if ACTIVERT_ENABLE_NAMES
    printf("Slowest Task:       %s\n", perf.slowest_task_name ? perf.slowest_task_name : "unknown");
    #endif
    
    activert_active_t* slowest = activert_stats_find_slowest_active();
    activert_active_t* busiest = activert_stats_find_busiest_active();
    
    #if ACTIVERT_ENABLE_NAMES
    if (slowest) {
        printf("\nSlowest Task:       %s (%u ticks max)\n",
               slowest->name ? slowest->name : "unnamed",
               (unsigned int)slowest->stats.max_processing_time);
    }
    
    if (busiest) {
        printf("Busiest Task:       %s (%u events)\n",
               busiest->name ? busiest->name : "unnamed",
               busiest->stats.events_processed);
    }
    #endif
    
    printf("================================================================================\n");
    #endif
}

/*******************************************************************************
* Statistics Reset
*******************************************************************************/

void activert_stats_reset_active(activert_active_t* active) {
    if (!active) {
        return;
    }
    
    // Reset counters but preserve high water marks
    active->stats.events_processed = 0;
    active->stats.events_dropped = 0;
    active->stats.notifications_received = 0;
    
    #if ACTIVERT_ENABLE_TIMING_STATS
    active->stats.total_processing_time = 0;
    active->stats.avg_processing_time = 0;
    // Keep max_processing_time as high water mark
    #endif
    
    // Reset queue stats
    for (uint8_t i = 0; i < active->queue_count; i++) {
        active->queues[i].stats.posts_attempted = 0;
        active->queues[i].stats.posts_succeeded = 0;
        active->queues[i].stats.posts_failed = 0;
        // Keep peak_depth as high water mark
    }
}

void activert_stats_reset_pool(activert_event_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    pool->stats.allocs_attempted = 0;
    pool->stats.allocs_succeeded = 0;
    pool->stats.allocs_failed = 0;
    // Keep peak_usage as high water mark
}

void activert_stats_reset_all(void) {
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        activert_stats_reset_active(g_stats_registry.actives[i]);
    }
    
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        activert_stats_reset_pool(g_stats_registry.pools[i]);
    }
}

/*******************************************************************************
* Real-Time Monitoring
*******************************************************************************/

void activert_stats_monitor_queue_depth(
    uint8_t threshold_percent,
    activert_monitor_callback_t callback
) {
    g_stats_registry.queue_monitor = callback;
    g_stats_registry.queue_threshold = threshold_percent;
}

void activert_stats_monitor_pool_exhaustion(activert_monitor_callback_t callback) {
    g_stats_registry.pool_monitor = callback;
}

void activert_stats_monitor_stack_usage(
    uint32_t threshold_bytes,
    activert_monitor_callback_t callback
) {
    g_stats_registry.stack_monitor = callback;
    g_stats_registry.stack_threshold = threshold_bytes;
}

void activert_stats_disable_monitoring(void) {
    g_stats_registry.queue_monitor = NULL;
    g_stats_registry.pool_monitor = NULL;
    g_stats_registry.stack_monitor = NULL;
}

/*******************************************************************************
* Export/Import
*******************************************************************************/

size_t activert_stats_get_export_size(void) {
    // Simple calculation: header + (actives × stats_size) + (pools × stats_size)
    return sizeof(uint32_t) * 2 +  // Header: active_count, pool_count
           g_stats_registry.active_count * sizeof(activert_active_stats_t) +
           g_stats_registry.pool_count * sizeof(activert_event_pool_stats_t);
}

int activert_stats_export(
    uint8_t* buffer, 
    size_t buffer_size
) {
    if (!buffer) {
        return -1;
    }
    
    size_t required = activert_stats_get_export_size();
    if (buffer_size < required) {
        return -1;
    }
    
    uint8_t* ptr = buffer;
    
    // Write header
    *(uint32_t*)ptr = g_stats_registry.active_count;
    ptr += sizeof(uint32_t);
    
    *(uint32_t*)ptr = g_stats_registry.pool_count;
    ptr += sizeof(uint32_t);
    
    // Write Active Object stats
    for (size_t i = 0; i < g_stats_registry.active_count; i++) {
        memcpy(ptr, &g_stats_registry.actives[i]->stats, sizeof(activert_active_stats_t));
        ptr += sizeof(activert_active_stats_t);
    }
    
    // Write Event Pool stats
    for (size_t i = 0; i < g_stats_registry.pool_count; i++) {
        memcpy(ptr, &g_stats_registry.pools[i]->stats, sizeof(activert_event_pool_stats_t));
        ptr += sizeof(activert_event_pool_stats_t);
    }
    
    return (int)(ptr - buffer);
}

#endif /* ACTIVERT_ENABLE_STATS */
