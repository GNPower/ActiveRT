/*******************************************************************************
 * freertos_test_main.c
 *
 * Shared entry point for all ActiveRT host-side unit tests. Starts the
 * FreeRTOS POSIX scheduler and launches the Unity test runner inside a
 * FreeRTOS task so that FreeRTOS APIs (queues, semaphores, task creation)
 * are fully operational during testing.
 *
 * Each unit test file must define:
 *   void run_tests(void);  -- calls RUN_TEST() for each test case
 ******************************************************************************/

#include "FreeRTOS.h"
#include "task.h"

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>

/* Declared by each unit test file */
extern void run_tests( void );

/*----------------------------------------------------------------------------
 * Static idle task memory (required when configSUPPORT_STATIC_ALLOCATION=1)
 *--------------------------------------------------------------------------*/
static StaticTask_t  s_idle_task_tcb;
static StackType_t   s_idle_task_stack[ configMINIMAL_STACK_SIZE ];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t  **ppxIdleTaskStackBuffer,
                                    uint32_t      *pulIdleTaskStackSize )
{
    *ppxIdleTaskTCBBuffer   = &s_idle_task_tcb;
    *ppxIdleTaskStackBuffer = s_idle_task_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/*----------------------------------------------------------------------------
 * Test runner task
 *--------------------------------------------------------------------------*/
static void test_runner_task( void *pvParameters )
{
    ( void ) pvParameters;

    UNITY_BEGIN();
    run_tests();
    int result = UNITY_END();

    /* Exit the process with Unity's failure count as the exit code.
     * ctest interprets non-zero as failure. */
    exit( result );
}

/*----------------------------------------------------------------------------
 * main — start the FreeRTOS POSIX scheduler
 *--------------------------------------------------------------------------*/
int main( void )
{
    /* Use a large stack: tests create FreeRTOS tasks internally */
    BaseType_t ret = xTaskCreate( test_runner_task,
                                  "TestRunner",
                                  configMINIMAL_STACK_SIZE * 16,
                                  NULL,
                                  configMAX_PRIORITIES - 1,
                                  NULL );

    if ( ret != pdPASS )
    {
        fprintf( stderr, "Failed to create test runner task\n" );
        return 1;
    }

    vTaskStartScheduler();

    /* Should never reach here */
    fprintf( stderr, "Scheduler exited unexpectedly\n" );
    return 1;
}
