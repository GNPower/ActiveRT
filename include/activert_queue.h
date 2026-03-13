/*******************************************************************************
*   ActiveRT - Active Object Framework for FreeRTOS
*
*   @file       activert_queue.h
*   @brief      Multi-Queue Utilities and Helper Macros
*   @author     Graham N. Power
*   @date       2025-12-13
*   @version    1.0.0
*
*   Revision History:
*
*   Ver     Who     Date        Changes
*   -----   ----    ----------  -----------------------------------------------
*   0.4.0   gnp     2025-12-13  Initial multi-queue API and helper macros
*   1.0.0   gnp     2026-02-28  Removed per-queue-count macros; queue stats helpers
*
*******************************************************************************/

#ifndef ACTIVERT_QUEUE_H
#define ACTIVERT_QUEUE_H

#include "activert_types.h"

/*******************************************************************************
* Queue Management
*******************************************************************************/

/**
 * Get queue depth (current number of events)
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              Number of events in queue
 */
UBaseType_t activert_queue_get_depth(activert_active_t* me, uint8_t queue_index);

/**
 * Get queue free space
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              Number of free slots in queue
 */
UBaseType_t activert_queue_get_free_space(activert_active_t* me, uint8_t queue_index);

/**
 * Check if queue is full
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              true if queue is full
 */
bool activert_queue_is_full(activert_active_t* me, uint8_t queue_index);

/**
 * Check if queue is empty
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              true if queue is empty
 */
bool activert_queue_is_empty(activert_active_t* me, uint8_t queue_index);

/**
 * Flush all events from a queue
 * 
 * Removes and discards all events from the specified queue.
 * Events from event pools are recycled.
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              Number of events flushed
 */
uint32_t activert_queue_flush(activert_active_t* me, uint8_t queue_index);

/**
 * Get queue configuration
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @param config        Output: queue configuration
 * @return              0 on success, -1 on error
 */
int activert_queue_get_config(
    activert_active_t* me, uint8_t queue_index, activert_queue_config_t* config
);

/*******************************************************************************
* Queue Statistics Helpers
*******************************************************************************/

#if ACTIVERT_ENABLE_STATS

/**
 * Print statistics for all queues in an Active Object
 * 
 * @param me            Active Object
 */
void activert_queue_print_all_stats(activert_active_t* me);

/**
 * Print statistics for a specific queue
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 */
void activert_queue_print_stats(activert_active_t* me, uint8_t queue_index);

/**
 * Get queue utilization percentage
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              Utilization percentage (0-100)
 */
uint8_t activert_queue_get_utilization(activert_active_t* me, uint8_t queue_index);

/**
 * Get queue peak utilization percentage
 * 
 * @param me            Active Object
 * @param queue_index   Queue index
 * @return              Peak utilization percentage (0-100)
 */
uint8_t activert_queue_get_peak_utilization(activert_active_t* me, uint8_t queue_index);

#endif /* ACTIVERT_ENABLE_STATS */

#endif /* ACTIVERT_QUEUE_H */
