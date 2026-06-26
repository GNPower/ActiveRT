/*******************************************************************************
 * FreeRTOSConfig.h
 *
 * This file configures the FreeRTOS kernel for testing via the Windows 
 * simulator port (MSVC_MINGW), which runs FreeRTOS tasks as Win32
 * threads. It works with both MSVC (cl.exe) and MinGW-w64 (gcc).
 *
 * It mirrors test/posix_config/FreeRTOSConfig.h so that tests behave
 * identically on Windows and Linux. The only differences are Windows-port
 * specifics (no POSIX errno). Used only in the test/ build on Windows.
 ******************************************************************************/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

/*----------------------------------------------------------------------------
 * Scheduler behaviour
 *--------------------------------------------------------------------------*/
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    10
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 256 )
#define configMAX_TASK_NAME_LEN                 32
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0

/*----------------------------------------------------------------------------
 * Memory management
 *--------------------------------------------------------------------------*/
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 256 * 1024 ) )
#define configAPPLICATION_ALLOCATED_HEAP        0

/*----------------------------------------------------------------------------
 * Hooks (disabled to keep test setup minimal)
 *--------------------------------------------------------------------------*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          0

/*----------------------------------------------------------------------------
 * FreeRTOS features required by ActiveRT
 *--------------------------------------------------------------------------*/
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    1   /* Required: ActiveRT multi-queue */
#define configUSE_TASK_NOTIFICATIONS            1   /* Required: ActiveRT notifications */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/*----------------------------------------------------------------------------
 * Optional kernel features
 *--------------------------------------------------------------------------*/
#define configUSE_TIMERS                        0   /* Disabled: no timer task needed */
#define configUSE_EVENT_GROUPS                  0
#define configUSE_STREAM_BUFFERS                0
#define configUSE_CO_ROUTINES                   0

/*----------------------------------------------------------------------------
 * Assertions print location and abort on failure
 *--------------------------------------------------------------------------*/
#define configASSERT( x )                                                   \
    do {                                                                    \
        if ( ( x ) == 0 )                                                   \
        {                                                                   \
            fprintf( stderr,                                                \
                     "FreeRTOS configASSERT FAILED: %s:%d\n",               \
                     __FILE__, __LINE__ );                                  \
            fflush( stderr );                                               \
            abort();                                                        \
        }                                                                   \
    } while( 0 )

/*----------------------------------------------------------------------------
 * API function inclusion (all enabled)
 *--------------------------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1

#endif /* FREERTOS_CONFIG_H */
