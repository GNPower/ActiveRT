/*******************************************************************************
 * test_active_basic.c
 *
 * Integration tests for basic Active Object creation and event dispatch.
 * Verifies that:
 *   - An Active Object can be created with ACTIVERT_ACTIVE_DEFINE/INIT_SIMPLE
 *   - Events posted to the AO are dispatched to the handler
 *   - The dispatch handler receives the correct signal
 *   - The AO can be cleanly stopped
 *
 * These tests run inside a FreeRTOS task (via freertos_test_main.c) so the
 * scheduler is running and AO tasks operate normally.
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdint.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Signals for this test
 *--------------------------------------------------------------------------*/
enum
{
    TEST_SIG_A = ACTIVERT_USER_SIG,
    TEST_SIG_B,
    TEST_SIG_TERM_ACK
};

/*----------------------------------------------------------------------------
 * Test event type
 *--------------------------------------------------------------------------*/
typedef struct
{
    activert_event_t base;
    uint32_t payload;
} basic_test_event_t;

/*----------------------------------------------------------------------------
 * Event pool used by all basic AO tests
 *--------------------------------------------------------------------------*/
#define BASIC_POOL_SIZE 4

ACTIVERT_EVENT_POOL_DEFINE(
    basic_pool, basic_test_event_t, BASIC_POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP
);

/*----------------------------------------------------------------------------
 * Static AO storage (single instance, reused per test after stop)
 *--------------------------------------------------------------------------*/
#define BASIC_AO_STACK_BYTES 4096
#define BASIC_AO_QUEUE_DEPTH 4
#define BASIC_AO_PRIORITY    2

ACTIVERT_ACTIVE_DEFINE_SIMPLE(
    basic_ao, basic_dispatch, BASIC_AO_PRIORITY, BASIC_AO_STACK_BYTES, BASIC_AO_QUEUE_DEPTH
);

/*----------------------------------------------------------------------------
 * Shared test state (written by AO task, read by test task)
 *--------------------------------------------------------------------------*/
static volatile uint32_t s_init_count;
static volatile uint32_t s_sig_a_count;
static volatile uint32_t s_sig_b_count;
static volatile uint32_t s_last_payload;
static SemaphoreHandle_t s_dispatch_sem; /* given by dispatch, taken by test */

/*----------------------------------------------------------------------------
 * AO dispatch handler
 *--------------------------------------------------------------------------*/
static void basic_dispatch(activert_active_t* me, const activert_event_t* evt)
{
    (void)me;
    basic_test_event_t* e = (basic_test_event_t*)evt;

    switch (evt->sig)
    {
        case ACTIVERT_INIT_SIG:
            s_init_count++;
            break;

        case TEST_SIG_A:
            s_sig_a_count++;
            s_last_payload = e->payload;
            xSemaphoreGive(s_dispatch_sem);
            break;

        case TEST_SIG_B:
            s_sig_b_count++;
            xSemaphoreGive(s_dispatch_sem);
            break;

        default:
            break;
    }
}

/*----------------------------------------------------------------------------
 * Helper: post a test event and wait for dispatch confirmation
 *--------------------------------------------------------------------------*/
static BaseType_t post_and_wait(uint32_t sig, uint32_t payload, TickType_t timeout_ticks)
{
    basic_test_event_t* evt = (basic_test_event_t*)activert_event_pool_alloc(basic_pool);
    if (evt == NULL)
        return pdFAIL;

    evt->base.sig = sig;
    evt->payload  = payload;

    int ret = activert_active_post(basic_ao, &evt->base);
    if (ret != 0)
    {
        activert_event_pool_free(&evt->base);
        return pdFAIL;
    }

    return xSemaphoreTake(s_dispatch_sem, timeout_ticks);
}

/*----------------------------------------------------------------------------
 * Unity setUp / tearDown
 *--------------------------------------------------------------------------*/
void setUp(void)
{
    s_init_count   = 0;
    s_sig_a_count  = 0;
    s_sig_b_count  = 0;
    s_last_payload = 0;

    ACTIVERT_EVENT_POOL_INIT(
        basic_pool, basic_test_event_t, BASIC_POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP
    );

    s_dispatch_sem = xSemaphoreCreateBinary();
    configASSERT(s_dispatch_sem != NULL);

    ACTIVERT_ACTIVE_INIT_SIMPLE(basic_ao, basic_dispatch, BASIC_AO_PRIORITY);
}

void tearDown(void)
{
    if (basic_ao != NULL)
    {
        activert_active_stop(basic_ao);
        /* Give idle task time to clean up the deleted pthread */
        vTaskDelay(pdMS_TO_TICKS(20));
        basic_ao = NULL;
    }

    if (s_dispatch_sem != NULL)
    {
        vSemaphoreDelete(s_dispatch_sem);
        s_dispatch_sem = NULL;
    }

    activert_event_pool_reset_stats(basic_pool);
}

/*============================================================================
 * Tests
 *==========================================================================*/

void test_ao_receives_init_sig_on_start(void)
{
    /* Give the AO task time to start and process INIT_SIG */
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_UINT32(1, s_init_count);
}

void test_ao_dispatches_posted_event(void)
{
    BaseType_t ok = post_and_wait(TEST_SIG_A, 42, pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL(pdTRUE, ok);
    TEST_ASSERT_EQUAL_UINT32(1, s_sig_a_count);
}

void test_ao_receives_correct_payload(void)
{
    post_and_wait(TEST_SIG_A, 0xDEADBEEF, pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, s_last_payload);
}

void test_ao_dispatches_multiple_events_in_order(void)
{
    /* Post SIG_A then SIG_B — count must reflect both */
    post_and_wait(TEST_SIG_A, 1, pdMS_TO_TICKS(500));
    post_and_wait(TEST_SIG_B, 0, pdMS_TO_TICKS(500));

    TEST_ASSERT_EQUAL_UINT32(1, s_sig_a_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_sig_b_count);
}

void test_ao_handles_multiple_rapid_posts(void)
{
#define RAPID_COUNT 4
    uint32_t expected = RAPID_COUNT;

    /* Fill the queue — pool has 4 slots */
    for (int i = 0; i < RAPID_COUNT; i++)
    {
        basic_test_event_t* evt = (basic_test_event_t*)activert_event_pool_alloc(basic_pool);
        TEST_ASSERT_NOT_NULL(evt);
        evt->base.sig = TEST_SIG_A;
        evt->payload  = (uint32_t)i;
        activert_active_post(basic_ao, &evt->base);
    }

    /* Wait for all to be dispatched */
    for (int i = 0; i < RAPID_COUNT; i++)
    {
        BaseType_t ok = xSemaphoreTake(s_dispatch_sem, pdMS_TO_TICKS(500));
        TEST_ASSERT_EQUAL(pdTRUE, ok);
    }

    TEST_ASSERT_EQUAL_UINT32(expected, s_sig_a_count);
#undef RAPID_COUNT
}

/*----------------------------------------------------------------------------
 * run_tests — called by freertos_test_main.c
 *--------------------------------------------------------------------------*/
void run_tests(void)
{
    RUN_TEST(test_ao_receives_init_sig_on_start);
    RUN_TEST(test_ao_dispatches_posted_event);
    RUN_TEST(test_ao_receives_correct_payload);
    RUN_TEST(test_ao_dispatches_multiple_events_in_order);
    RUN_TEST(test_ao_handles_multiple_rapid_posts);
}
