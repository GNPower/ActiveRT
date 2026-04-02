/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_cli.h
*   @brief      CLI Commands for Runtime Inspection
*   @author     Graham N. Power
*   @date       2026-01-10
*   @version    1.0.0
*
*   Provides standalone CLI command functions for runtime inspection.
*   Each function prints output via ACTIVERT_CLI_PRINTF (configurable in
*   activert_config.h). Designed to integrate with any CLI system through
*   a thin bridge layer.
*
*   Configuration (activert_config.h):
*     ACTIVERT_CLI_PRINTF     - printf-like function for CLI output
*     ACTIVERT_CLI_GET_TOKEN  - function to extract arg tokens from string
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.6.0   gnp     2026-01-10  Initial CLI command declarations
*   1.0.0   gnp     2026-02-28  Format helper declarations; ACTIVERT_ENABLE_CLI guard
*
*******************************************************************************/

#ifndef ACTIVERT_CLI_H
#define ACTIVERT_CLI_H

#include "activert_config.h"
#include "activert_stats.h"

#if ACTIVERT_ENABLE_STATS

/*******************************************************************************
* CLI Command Functions
*
* Each function takes a tokenized argument string and prints output via
* ACTIVERT_CLI_PRINTF. The args parameter is passed directly from the host
* CLI system's dispatcher.
*******************************************************************************/

/**
 * Print ActiveRT system summary
 *
 * Output: AO count, pool count, events processed/dropped, health status
 *
 * @param args  Unused (no arguments)
 */
void activert_cli_cmd_summary(const char* args);

/**
 * List all Active Objects or Event Pools
 *
 * Usage Example: activert list [actives|pools]
 * Default (no args) lists Active Objects.
 *
 * @param args  Optional: [actives|pools], "pools" to list pools instead of actives
 */
void activert_cli_cmd_list(const char* args);

/**
 * Show detailed statistics for an Active Object
 *
 * Usage Example: activert show <name|index>
 *
 * Output: events processed/dropped, notifications, queue stats, stack usage
 *
 * @param args  Name or index of Active Object
 */
void activert_cli_cmd_show(const char* args);

/**
 * Show detailed statistics for an Event Pool
 *
 * Usage Example: activert pool <name|index>
 *
 * Output: allocs attempted/succeeded/failed, frees, peak, current
 *
 * @param args  Name or index of Event Pool
 */
void activert_cli_cmd_pool(const char* args);

/**
 * Run system health check
 *
 * Output: health status (OK/WARNING/CRITICAL) and list of issues
 *
 * @param args  Unused (no arguments)
 */
void activert_cli_cmd_health(const char* args);

/**
 * Reset statistics counters
 *
 * Usage Example: activert reset [all]
 *
 * @param args  "all" to reset all statistics
 */
void activert_cli_cmd_reset(const char* args);

/**
 * Show performance analysis
 *
 * Output: total events, avg/max processing time, slowest/busiest task
 * Requires ACTIVERT_ENABLE_TIMING_STATS=1
 *
 * @param args  Unused (no arguments)
 */
void activert_cli_cmd_perf(const char* args);

/**
 * Print full system report
 *
 * Prints summary, all AOs, all pools, and performance analysis.
 *
 * @param args  Unused (no arguments)
 */
void activert_cli_cmd_report(const char* args);

/**
 * Print help for ActiveRT CLI commands
 *
 * @param args  Unused (no arguments)
 */
void activert_cli_cmd_help(const char* args);

/*******************************************************************************
* Formatting Helpers
*******************************************************************************/

/**
 * Format health status as string
 */
const char* activert_cli_health_status_str(activert_health_status_t status);

/**
 * Format byte count as human-readable string
 */
const char* activert_cli_format_bytes(size_t bytes, char* buffer, size_t buffer_len);

/**
 * Format percentage
 */
const char*
activert_cli_format_percent(uint32_t value, uint32_t max, char* buffer, size_t buffer_len);

#endif /* ACTIVERT_ENABLE_STATS */

#endif /* ACTIVERT_CLI_H */
