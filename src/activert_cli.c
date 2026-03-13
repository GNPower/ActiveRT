/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_cli.c
*   @brief      CLI Commands Implementation
*   @author     Graham N. Power
*   @date       2026-01-10
*   @version    1.0.0
*
*   All command functions print via ACTIVERT_CLI_PRINTF and parse arguments
*   via ACTIVERT_CLI_GET_TOKEN. Both are configurable in activert_config.h.
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.6.0   gnp     2026-01-10  Initial CLI layer (summary, list, show, pool, health)
*   1.0.0   gnp     2026-02-28  Added reset, perf, report commands; ACTIVERT_ENABLE_CLI guard
*
*******************************************************************************/

#include "activert_cli.h"
#include "activert_stats.h"
#include "activert_active.h"
#include "activert_event.h"
#include <string.h>
#include <stdlib.h>

#if ACTIVERT_ENABLE_STATS

/*******************************************************************************
* Helper: Find Active Object by name or index string
*******************************************************************************/

static activert_active_t* find_active(const char* arg)
{
    if (arg == NULL)
    {
        return NULL;
    }

    // Try as numeric index first
    char* endptr;
    unsigned long idx = strtoul(arg, &endptr, 10);
    if (*endptr == '\0')
    {
        return activert_stats_get_active((size_t)idx);
    }

    // Search by name
    #if ACTIVERT_ENABLE_NAMES
    size_t count = activert_stats_get_active_count();
    for (size_t i = 0; i < count; i++)
    {
        activert_active_t* ao = activert_stats_get_active(i);
        if (ao && ao->name && strcmp(ao->name, arg) == 0)
        {
            return ao;
        }
    }
    #endif /* ACTIVERT_ENABLE_NAMES */

    return NULL;
}

/*******************************************************************************
* Helper: Find Event Pool by name or index string
*******************************************************************************/

static activert_event_pool_t* find_pool(const char* arg)
{
    if (arg == NULL)
    {
        return NULL;
    }

    // Try as numeric index first
    char* endptr;
    unsigned long idx = strtoul(arg, &endptr, 10);
    if (*endptr == '\0')
    {
        return activert_stats_get_pool((size_t)idx);
    }

    // Search by name
    #if ACTIVERT_ENABLE_NAMES
    size_t count = activert_stats_get_pool_count();
    for (size_t i = 0; i < count; i++)
    {
        activert_event_pool_t* pool = activert_stats_get_pool(i);
        if (pool && pool->name && strcmp(pool->name, arg) == 0)
        {
            return pool;
        }
    }
    #endif /* ACTIVERT_ENABLE_NAMES */

    return NULL;
}

/*******************************************************************************
* Command Handlers
*******************************************************************************/

void activert_cli_cmd_summary(const char* args)
{
    (void)args;

    ACTIVERT_CLI_PRINTF("ActiveRT System Summary");
    ACTIVERT_CLI_PRINTF("=======================");
    ACTIVERT_CLI_PRINTF("Active Objects:   %u", (unsigned int)activert_stats_get_active_count());
    ACTIVERT_CLI_PRINTF("Event Pools:      %u", (unsigned int)activert_stats_get_pool_count());
    ACTIVERT_CLI_PRINTF(
        "Events Processed: %u", (unsigned int)activert_stats_get_total_events_processed()
    );
    ACTIVERT_CLI_PRINTF(
        "Events Dropped:   %u", (unsigned int)activert_stats_get_total_events_dropped()
    );
    ACTIVERT_CLI_PRINTF(
        "Notifications:    %u", (unsigned int)activert_stats_get_total_notifications()
    );
    ACTIVERT_CLI_PRINTF(
        "Pool Allocs:      %u", (unsigned int)activert_stats_get_total_pool_allocs()
    );
    ACTIVERT_CLI_PRINTF(
        "Pool Failures:    %u", (unsigned int)activert_stats_get_total_pool_failures()
    );

    activert_health_check_t health;
    activert_stats_health_check(&health);

    ACTIVERT_CLI_PRINTF("");
    switch (health.status)
    {
        case ACTIVERT_HEALTH_OK:
            ACTIVERT_CLI_PRINTF("Health: OK");
            break;
        case ACTIVERT_HEALTH_WARNING:
            ACTIVERT_CLI_PRINTF("Health: WARNING (%u)", (unsigned int)health.warnings);
            break;
        case ACTIVERT_HEALTH_CRITICAL:
            ACTIVERT_CLI_PRINTF(
                "Health: CRITICAL (%u warnings, %u critical)",
                (unsigned int)health.warnings,
                (unsigned int)health.criticals
            );
            break;
        default:
            ACTIVERT_CLI_PRINTF("Health: UNKNOWN");
            break;
    }
}

void activert_cli_cmd_list(const char* args)
{
    const char* type = ACTIVERT_CLI_GET_TOKEN(args, 1);

    if ((type != NULL) && (strcmp(type, "pools") == 0))
    {
        // List event pools
        size_t count = activert_stats_get_pool_count();
        ACTIVERT_CLI_PRINTF("Event Pools (%u)", (unsigned int)count);
        ACTIVERT_CLI_PRINTF("==============");

        for (size_t i = 0; i < count; i++)
        {
            activert_event_pool_t* pool = activert_stats_get_pool(i);
            if (pool == NULL)
            {
                continue;
            }

    #if ACTIVERT_ENABLE_NAMES
            const char* name = pool->name ? pool->name : "unnamed";
    #else  /* ACTIVERT_ENABLE_NAMES */
            const char* name = "-";
    #endif /* ACTIVERT_ENABLE_NAMES */

            ACTIVERT_CLI_PRINTF(
                "  [%u] %-16s  size: %u  allocs: %u  fails: %u",
                (unsigned int)i,
                name,
                (unsigned int)pool->pool_size,
                (unsigned int)pool->stats.allocs_succeeded,
                (unsigned int)pool->stats.allocs_failed
            );
        }
    }
    else
    {
        // List active objects
        size_t count = activert_stats_get_active_count();
        ACTIVERT_CLI_PRINTF("Active Objects (%u)", (unsigned int)count);
        ACTIVERT_CLI_PRINTF("==================");

        for (size_t i = 0; i < count; i++)
        {
            activert_active_t* ao = activert_stats_get_active(i);
            if (ao == NULL)
            {
                continue;
            }

    #if ACTIVERT_ENABLE_NAMES
            const char* name = ao->name ? ao->name : "unnamed";
    #else  /* ACTIVERT_ENABLE_NAMES */
            const char* name = "-";
    #endif /* ACTIVERT_ENABLE_NAMES */

            ACTIVERT_CLI_PRINTF(
                "  [%u] %-16s  pri: %u  events: %u  dropped: %u",
                (unsigned int)i,
                name,
                (unsigned int)ao->priority,
                (unsigned int)ao->stats.events_processed,
                (unsigned int)ao->stats.events_dropped
            );
        }
    }
}

void activert_cli_cmd_show(const char* args)
{
    const char* arg = ACTIVERT_CLI_GET_TOKEN(args, 1);
    if (arg == NULL)
    {
        ACTIVERT_CLI_PRINTF("Usage: activert show <name|index>");
        return;
    }

    activert_active_t* ao = find_active(arg);
    if (ao == NULL)
    {
        ACTIVERT_CLI_PRINTF("Active Object '%s' not found", arg);
        return;
    }

    #if ACTIVERT_ENABLE_NAMES
    const char* name = ao->name ? ao->name : "unnamed";
    #else  /* ACTIVERT_ENABLE_NAMES */
    const char* name = "AO";
    #endif /* ACTIVERT_ENABLE_NAMES */

    ACTIVERT_CLI_PRINTF("Active Object: %s", name);
    ACTIVERT_CLI_PRINTF("================================");
    ACTIVERT_CLI_PRINTF("Priority:       %u", (unsigned int)ao->priority);
    ACTIVERT_CLI_PRINTF("Queue count:    %u", (unsigned int)ao->queue_count);
    ACTIVERT_CLI_PRINTF("");
    ACTIVERT_CLI_PRINTF("Event Statistics:");
    ACTIVERT_CLI_PRINTF("  Processed:    %u", (unsigned int)ao->stats.events_processed);
    ACTIVERT_CLI_PRINTF("  Dropped:      %u", (unsigned int)ao->stats.events_dropped);
    ACTIVERT_CLI_PRINTF("  Notifications: %u", (unsigned int)ao->stats.notifications_received);

    #if ACTIVERT_ENABLE_TIMING_STATS
    ACTIVERT_CLI_PRINTF("  Avg time:     %u ticks", (unsigned int)ao->stats.avg_processing_time);
    ACTIVERT_CLI_PRINTF(
        "  Max time:     %u ticks (sig %u)",
        (unsigned int)ao->stats.max_processing_time,
        (unsigned int)ao->stats.slowest_signal
    );
    #endif /* ACTIVERT_ENABLE_TIMING_STATS */

    // Print queue details
    for (uint8_t q = 0; q < ao->queue_count; q++)
    {
        ACTIVERT_CLI_PRINTF("");
        ACTIVERT_CLI_PRINTF("Queue %u:", (unsigned int)q);
        ACTIVERT_CLI_PRINTF("  Length:       %u", (unsigned int)ao->queues[q].queue_length);
        ACTIVERT_CLI_PRINTF(
            "  Posts:        %u (%u OK, %u failed)",
            (unsigned int)ao->queues[q].stats.posts_attempted,
            (unsigned int)ao->queues[q].stats.posts_succeeded,
            (unsigned int)ao->queues[q].stats.posts_failed
        );
        ACTIVERT_CLI_PRINTF("  Current:      %u", (unsigned int)ao->queues[q].stats.current_depth);
        ACTIVERT_CLI_PRINTF("  Peak:         %u", (unsigned int)ao->queues[q].stats.peak_depth);
    }

    // Stack high water mark
    uint32_t stack_hwm = activert_active_get_stack_high_water(ao);
    ACTIVERT_CLI_PRINTF("");
    ACTIVERT_CLI_PRINTF("Stack HWM:      %u bytes free", (unsigned int)stack_hwm);
}

void activert_cli_cmd_pool(const char* args)
{
    const char* arg = ACTIVERT_CLI_GET_TOKEN(args, 1);
    if (arg == NULL)
    {
        ACTIVERT_CLI_PRINTF("Usage: activert pool <name|index>");
        return;
    }

    activert_event_pool_t* pool = find_pool(arg);
    if (pool == NULL)
    {
        ACTIVERT_CLI_PRINTF("Event Pool '%s' not found", arg);
        return;
    }

    #if ACTIVERT_ENABLE_NAMES
    const char* name = pool->name ? pool->name : "unnamed";
    #else  /* ACTIVERT_ENABLE_NAMES */
    const char* name = "Pool";
    #endif /* ACTIVERT_ENABLE_NAMES */

    ACTIVERT_CLI_PRINTF("Event Pool: %s", name);
    ACTIVERT_CLI_PRINTF("================================");
    ACTIVERT_CLI_PRINTF(
        "Size:           %u events x %u bytes = %u bytes",
        (unsigned int)pool->pool_size,
        (unsigned int)pool->event_size,
        (unsigned int)(pool->pool_size * pool->event_size)
    );
    ACTIVERT_CLI_PRINTF(
        "Current:        %u / %u",
        (unsigned int)pool->stats.current_allocated,
        (unsigned int)pool->pool_size
    );
    ACTIVERT_CLI_PRINTF(
        "Peak:           %u / %u",
        (unsigned int)pool->stats.peak_allocated,
        (unsigned int)pool->pool_size
    );
    ACTIVERT_CLI_PRINTF(
        "Allocs:         %u attempted, %u succeeded, %u failed",
        (unsigned int)pool->stats.allocs_attempted,
        (unsigned int)pool->stats.allocs_succeeded,
        (unsigned int)pool->stats.allocs_failed
    );
    ACTIVERT_CLI_PRINTF("Frees:          %u", (unsigned int)pool->stats.frees);

    if (pool->stats.allocs_attempted > 0U)
    {
        uint32_t rate = (pool->stats.allocs_succeeded * 100U) / pool->stats.allocs_attempted;
        ACTIVERT_CLI_PRINTF("Success rate:   %u%%", (unsigned int)rate);
    }
}

void activert_cli_cmd_health(const char* args)
{
    (void)args;

    activert_health_check_t health;
    activert_stats_health_check(&health);

    ACTIVERT_CLI_PRINTF("Health Check");
    ACTIVERT_CLI_PRINTF("============");

    switch (health.status)
    {
        case ACTIVERT_HEALTH_OK:
            ACTIVERT_CLI_PRINTF("Status: OK - No issues detected");
            break;
        case ACTIVERT_HEALTH_WARNING:
            ACTIVERT_CLI_PRINTF("Status: WARNING");
            break;
        case ACTIVERT_HEALTH_CRITICAL:
            ACTIVERT_CLI_PRINTF("Status: CRITICAL");
            break;
        default:
            ACTIVERT_CLI_PRINTF("Status: UNKNOWN");
            break;
    }

    if ((health.warnings > 0U) || (health.criticals > 0U))
    {
        ACTIVERT_CLI_PRINTF("");
        ACTIVERT_CLI_PRINTF("Issues:");

        if (health.high_queue_utilization)
        {
            ACTIVERT_CLI_PRINTF("  [WARN] High queue utilization (>80%%)");
        }
        if (health.pool_exhaustion)
        {
            ACTIVERT_CLI_PRINTF("  [WARN] Pool exhaustion detected");
        }
        if (health.high_drop_rate)
        {
            ACTIVERT_CLI_PRINTF("  [WARN] High event drop rate (>5%%)");
        }
        if (health.low_stack)
        {
            ACTIVERT_CLI_PRINTF("  [WARN] Low stack space (<512 bytes)");
        }
        if (health.queue_overflow)
        {
            ACTIVERT_CLI_PRINTF("  [CRIT] Queue overflow!");
        }
        if (health.pool_critical)
        {
            ACTIVERT_CLI_PRINTF("  [CRIT] Pool failure rate critical (>50%%)!");
        }
        if (health.stack_overflow_risk)
        {
            ACTIVERT_CLI_PRINTF("  [CRIT] Stack overflow risk (<256 bytes)!");
        }
    }
}

void activert_cli_cmd_reset(const char* args)
{
    const char* arg = ACTIVERT_CLI_GET_TOKEN(args, 1);

    if (arg == NULL)
    {
        ACTIVERT_CLI_PRINTF("Usage: activert reset [all]");
        return;
    }

    if (strcmp(arg, "all") == 0)
    {
        activert_stats_reset_all();
        ACTIVERT_CLI_PRINTF("All statistics reset");
    }
    else
    {
        ACTIVERT_CLI_PRINTF("Unknown target '%s'. Use 'all'.", arg);
    }
}

void activert_cli_cmd_perf(const char* args)
{
    (void)args;

    #if ACTIVERT_ENABLE_TIMING_STATS
    activert_perf_summary_t perf;
    activert_stats_get_perf_summary(&perf);

    ACTIVERT_CLI_PRINTF("Performance Analysis");
    ACTIVERT_CLI_PRINTF("====================");
    ACTIVERT_CLI_PRINTF("Total Events:     %u", (unsigned int)perf.total_events);
    ACTIVERT_CLI_PRINTF("Avg Processing:   %u ticks", (unsigned int)perf.avg_processing_time);
    ACTIVERT_CLI_PRINTF("Max Processing:   %u ticks", (unsigned int)perf.max_processing_time);
    ACTIVERT_CLI_PRINTF("Slowest Signal:   %u", (unsigned int)perf.slowest_signal);

        #if ACTIVERT_ENABLE_NAMES
    if (perf.slowest_task_name != NULL)
    {
        ACTIVERT_CLI_PRINTF("Slowest Task:     %s", perf.slowest_task_name);
    }
        #endif /* ACTIVERT_ENABLE_NAMES */

    activert_active_t* busiest = activert_stats_find_busiest_active();
    if (busiest != NULL)
    {
        #if ACTIVERT_ENABLE_NAMES
        ACTIVERT_CLI_PRINTF(
            "Busiest Task:     %s (%u events)",
            busiest->name ? busiest->name : "unnamed",
            (unsigned int)busiest->stats.events_processed
        );
        #else  /* ACTIVERT_ENABLE_NAMES */
        ACTIVERT_CLI_PRINTF(
            "Busiest Task:     %u events", (unsigned int)busiest->stats.events_processed
        );
        #endif /* ACTIVERT_ENABLE_NAMES */
    }
    #else  /* ACTIVERT_ENABLE_TIMING_STATS */
    ACTIVERT_CLI_PRINTF("Timing statistics disabled (ACTIVERT_ENABLE_TIMING_STATS=0)");
    #endif /* ACTIVERT_ENABLE_TIMING_STATS */
}

void activert_cli_cmd_report(const char* args)
{
    activert_cli_cmd_summary(args);
    ACTIVERT_CLI_PRINTF("");
    activert_cli_cmd_list(NULL);
    ACTIVERT_CLI_PRINTF("");
    // List pools by creating a fake args string - just call list with "pools"
    size_t pool_count = activert_stats_get_pool_count();
    if (pool_count > 0U)
    {
        ACTIVERT_CLI_PRINTF("Event Pools (%u)", (unsigned int)pool_count);
        ACTIVERT_CLI_PRINTF("==============");
        for (size_t i = 0; i < pool_count; i++)
        {
            activert_event_pool_t* pool = activert_stats_get_pool(i);
            if (pool == NULL)
            {
                continue;
            }
    #if ACTIVERT_ENABLE_NAMES
            const char* name = pool->name ? pool->name : "unnamed";
    #else  /* ACTIVERT_ENABLE_NAMES */
            const char* name = "-";
    #endif /* ACTIVERT_ENABLE_NAMES */
            ACTIVERT_CLI_PRINTF(
                "  [%u] %-16s  size: %u  allocs: %u  fails: %u",
                (unsigned int)i,
                name,
                (unsigned int)pool->pool_size,
                (unsigned int)pool->stats.allocs_succeeded,
                (unsigned int)pool->stats.allocs_failed
            );
        }
    }
    ACTIVERT_CLI_PRINTF("");
    activert_cli_cmd_health(args);
    ACTIVERT_CLI_PRINTF("");
    activert_cli_cmd_perf(args);
}

void activert_cli_cmd_help(const char* args)
{
    (void)args;

    ACTIVERT_CLI_PRINTF("ActiveRT CLI Commands");
    ACTIVERT_CLI_PRINTF("=====================");
    ACTIVERT_CLI_PRINTF("activert summary              - System summary");
    ACTIVERT_CLI_PRINTF("activert list [actives|pools] - List objects");
    ACTIVERT_CLI_PRINTF("activert show <name|index>    - Show Active Object");
    ACTIVERT_CLI_PRINTF("activert pool <name|index>    - Show Event Pool");
    ACTIVERT_CLI_PRINTF("activert health               - Health check");
    ACTIVERT_CLI_PRINTF("activert reset [all]          - Reset statistics");
    ACTIVERT_CLI_PRINTF("activert perf                 - Performance analysis");
    ACTIVERT_CLI_PRINTF("activert report               - Full report");
    ACTIVERT_CLI_PRINTF("activert help                 - This help");
}

/*******************************************************************************
* Formatting Helpers
*******************************************************************************/

const char* activert_cli_health_status_str(activert_health_status_t status)
{
    switch (status)
    {
        case ACTIVERT_HEALTH_OK:
            return "OK";
        case ACTIVERT_HEALTH_WARNING:
            return "WARNING";
        case ACTIVERT_HEALTH_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

const char* activert_cli_format_bytes(size_t bytes, char* buffer, size_t buffer_len)
{
    if (bytes < 1024U)
    {
        snprintf(buffer, buffer_len, "%u B", (unsigned int)bytes);
    }
    else if (bytes < ((size_t)1024U * 1024U))
    {
        snprintf(buffer, buffer_len, "%u KB", (unsigned int)(bytes / 1024U));
    }
    else
    {
        snprintf(buffer, buffer_len, "%u MB", (unsigned int)(bytes / ((size_t)1024U * 1024U)));
    }
    return buffer;
}

const char*
activert_cli_format_percent(uint32_t value, uint32_t max, char* buffer, size_t buffer_len)
{
    if (max == 0U)
    {
        snprintf(buffer, buffer_len, "0%%");
    }
    else
    {
        uint32_t percent = (value * 100U) / max;
        snprintf(buffer, buffer_len, "%u%%", (unsigned int)percent);
    }
    return buffer;
}

#endif /* ACTIVERT_ENABLE_STATS */
