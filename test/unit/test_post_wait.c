/*******************************************************************************
 * test_post_wait.c
 *
 * Unit tests for activert_active_post_wait().
 * Verifies that:
 *   - The call returns only after the dispatch handler has returned
 *   - The calling task really blocks for the handler duration
 *   - A failed post leaves the event with the caller
 *   - A disabled Active Object and a post from the target's own task both fail
 *     immediately rather than blocking
 *   - A timeout before dispatch begins reports TIMEOUT and leaves the event in
 *     flight, owned by the Active Object
 *   - An event discarded because the Active Object disabled itself reports
 *     DROPPED rather than hanging the caller
 *   - Repeated calls return every event to the pool
 *
 * The Active Objects here run at a lower priority than the Unity runner task,
 * so the runner can queue several events before the AO gets to dequeue any of them.
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdint.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Signals
 *--------------------------------------------------------------------------*/
enum
{
    PW_SIG_FAST = ACTIVERT_USER_SIG,
    PW_SIG_SLOW,
    PW_SIG_SELF_POST,
    PW_SIG_DISABLE_SELF
};

/*----------------------------------------------------------------------------
 * Event type and pool
 *--------------------------------------------------------------------------*/
typedef struct
{
    activert_event_t base;
    uint32_t payload;
} pw_event_t;

#define PW_POOL_SIZE 6

ACTIVERT_EVENT_POOL_DEFINE(pw_pool, pw_event_t, PW_POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP);

/*----------------------------------------------------------------------------
 * Active Object under test
 *
 * Queue depth 2 so the full-queue path can be reached with three posts.
 *--------------------------------------------------------------------------*/
#define PW_AO_STACK_BYTES 8192
#define PW_AO_QUEUE_DEPTH 2
#define PW_AO_PRIORITY    2

ACTIVERT_ACTIVE_DEFINE_SIMPLE(
    pw_ao, pw_dispatch, PW_AO_PRIORITY, PW_AO_STACK_BYTES, PW_AO_QUEUE_DEPTH
);

static volatile uint32_t s_fast_count;
static volatile uint32_t s_slow_count;
static volatile uint32_t s_slow_ms;
static volatile int s_self_post_rc;
static volatile int s_self_post_done;

static void pw_dispatch(activert_active_t* me, const activert_event_t* evt)
{
    switch (evt->sig)
    {
        case PW_SIG_FAST:
            s_fast_count++;
            break;

        case PW_SIG_SLOW:
            vTaskDelay(pdMS_TO_TICKS(s_slow_ms));
            s_slow_count++;
            break;

        case PW_SIG_SELF_POST:
        {
            /* Posting synchronously to the Active Object currently dispatching
             * can never complete, so the library must reject it. */
            pw_event_t* inner = (pw_event_t*)activert_event_pool_alloc(pw_pool);
            if (inner != NULL)
            {
                inner->base.sig = PW_SIG_FAST;
                s_self_post_rc  = activert_active_post_wait(me, &inner->base, pdMS_TO_TICKS(50));

                if (s_self_post_rc == ACTIVERT_POST_WAIT_FAILED)
                {
                    /* Rejected, so the event never left this handler. */
                    activert_event_pool_free(&inner->base);
                }
            }
            s_self_post_done = 1;
            break;
        }

        case PW_SIG_DISABLE_SELF:
            (void)activert_active_set_enabled(me, false);
            break;

        default:
            break;
    }
}

/*----------------------------------------------------------------------------
 * Helpers
 *--------------------------------------------------------------------------*/
static pw_event_t* alloc_event(activert_signal_t sig)
{
    pw_event_t* evt = (pw_event_t*)activert_event_pool_alloc(pw_pool);
    TEST_ASSERT_NOT_NULL(evt);
    evt->base.sig = sig;
    evt->payload  = 0U;
    return evt;
}

static int post_plain(activert_signal_t sig)
{
    return activert_active_post(pw_ao, &alloc_event(sig)->base);
}

/*----------------------------------------------------------------------------
 * Unity setUp / tearDown
 *--------------------------------------------------------------------------*/
void setUp(void)
{
    s_fast_count     = 0;
    s_slow_count     = 0;
    s_slow_ms        = 100;
    s_self_post_rc   = 0;
    s_self_post_done = 0;

    ACTIVERT_EVENT_POOL_INIT(pw_pool, pw_event_t, PW_POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(pw_ao, pw_dispatch, PW_AO_PRIORITY);
}

void tearDown(void)
{
    if (pw_ao != NULL)
    {
        activert_active_stop(pw_ao);
        vTaskDelay(pdMS_TO_TICKS(20));
        pw_ao = NULL;
    }
}

/*============================================================================
 * Tests
 *==========================================================================*/

void test_post_wait_returns_only_after_dispatch_returns(void)
{
    pw_event_t* evt = alloc_event(PW_SIG_SLOW);

    int rc = activert_active_post_wait(pw_ao, &evt->base, portMAX_DELAY);

    TEST_ASSERT_EQUAL_INT(ACTIVERT_POST_WAIT_OK, rc);

    /* The handler increments this last, so seeing it here without any
     * delay in the test proves the call waited for dispatch to return. */
    TEST_ASSERT_EQUAL_UINT32(1, s_slow_count);

    /* The Active Object owns and frees a dispatched event. */
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));
}

void test_post_wait_blocks_for_handler_duration(void)
{
    TickType_t start;
    TickType_t elapsed;
    pw_event_t* evt;

    s_slow_ms = 200;
    evt       = alloc_event(PW_SIG_SLOW);

    start = xTaskGetTickCount();
    TEST_ASSERT_EQUAL_INT(
        ACTIVERT_POST_WAIT_OK, activert_active_post_wait(pw_ao, &evt->base, portMAX_DELAY)
    );
    elapsed = xTaskGetTickCount() - start;

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(pdMS_TO_TICKS(200), elapsed);
}

void test_post_wait_to_full_queue_returns_failed_and_caller_owns_event(void)
{
    pw_event_t* evt;

    /* The runner has higher priority than the AO, so these two stay queued. */
    TEST_ASSERT_EQUAL_INT(0, post_plain(PW_SIG_FAST));
    TEST_ASSERT_EQUAL_INT(0, post_plain(PW_SIG_FAST));

    evt = alloc_event(PW_SIG_FAST);
    TEST_ASSERT_EQUAL_INT(
        ACTIVERT_POST_WAIT_FAILED, activert_active_post_wait(pw_ao, &evt->base, pdMS_TO_TICKS(500))
    );

    /* Failed means the caller still owns it. Freeing it must succeed. */
    activert_event_pool_free(&evt->base);

    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL_UINT32(2, s_fast_count);
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));
}

void test_post_wait_on_disabled_ao_returns_failed(void)
{
    pw_event_t* evt = alloc_event(PW_SIG_FAST);

    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(pw_ao, false));

    /* Fails immediately instead of blocking on an Active Object that will
     * never dispatch it. */
    TEST_ASSERT_EQUAL_INT(
        ACTIVERT_POST_WAIT_FAILED, activert_active_post_wait(pw_ao, &evt->base, pdMS_TO_TICKS(500))
    );

    activert_event_pool_free(&evt->base);
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));

    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(pw_ao, true));
}

void test_post_wait_from_ao_own_task_returns_failed(void)
{
    TEST_ASSERT_EQUAL_INT(0, post_plain(PW_SIG_SELF_POST));

    vTaskDelay(pdMS_TO_TICKS(300));

    TEST_ASSERT_EQUAL_INT(1, s_self_post_done);
    TEST_ASSERT_EQUAL_INT(ACTIVERT_POST_WAIT_FAILED, s_self_post_rc);
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));
}

void test_post_wait_timeout_returns_timeout_with_event_still_queued(void)
{
    pw_event_t* evt;

    s_slow_ms = 400;
    TEST_ASSERT_EQUAL_INT(0, post_plain(PW_SIG_SLOW));

    evt = alloc_event(PW_SIG_FAST);
    TEST_ASSERT_EQUAL_INT(
        ACTIVERT_POST_WAIT_TIMEOUT, activert_active_post_wait(pw_ao, &evt->base, pdMS_TO_TICKS(30))
    );

    /* TIMEOUT means the Active Object still owns the event. Freeing it here
     * would be a double free, so the test waits for the AO to dispatch it and
     * checks that the library frees it on its own. */
    TEST_ASSERT_EQUAL_UINT32(0, s_fast_count);

    vTaskDelay(pdMS_TO_TICKS(700));

    TEST_ASSERT_EQUAL_UINT32(1, s_slow_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_fast_count);
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));
}

void test_post_wait_timeout_during_dispatch_waits_for_handler(void)
{
    TickType_t start;
    TickType_t elapsed;
    pw_event_t* evt;

    s_slow_ms = 300;
    evt       = alloc_event(PW_SIG_SLOW);

    start = xTaskGetTickCount();
    TEST_ASSERT_EQUAL_INT(
        ACTIVERT_POST_WAIT_OK, activert_active_post_wait(pw_ao, &evt->base, pdMS_TO_TICKS(30))
    );
    elapsed = xTaskGetTickCount() - start;

    TEST_ASSERT_EQUAL_UINT32(1, s_slow_count);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(pdMS_TO_TICKS(300), elapsed);
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));
}

void test_post_wait_discarded_by_disable_returns_dropped(void)
{
    pw_event_t* evt;

    TEST_ASSERT_EQUAL_INT(0, post_plain(PW_SIG_DISABLE_SELF));

    evt = alloc_event(PW_SIG_FAST);
    TEST_ASSERT_EQUAL_INT(
        ACTIVERT_POST_WAIT_DROPPED,
        activert_active_post_wait(pw_ao, &evt->base, pdMS_TO_TICKS(2000))
    );

    TEST_ASSERT_FALSE(activert_active_is_enabled(pw_ao));
    TEST_ASSERT_EQUAL_UINT32(0, s_fast_count);

    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));

    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(pw_ao, true));
}

void test_post_wait_does_not_leak_pool_events(void)
{
    for (int i = 0; i < 10; i++)
    {
        pw_event_t* evt = alloc_event(PW_SIG_FAST);
        TEST_ASSERT_EQUAL_INT(
            ACTIVERT_POST_WAIT_OK, activert_active_post_wait(pw_ao, &evt->base, pdMS_TO_TICKS(1000))
        );
    }

    TEST_ASSERT_EQUAL_UINT32(10, s_fast_count);
    TEST_ASSERT_EQUAL_size_t(PW_POOL_SIZE, activert_event_pool_get_free_count(pw_pool));
}

/*----------------------------------------------------------------------------
 * run_tests, called by freertos_test_main.c
 *--------------------------------------------------------------------------*/
void run_tests(void)
{
    RUN_TEST(test_post_wait_returns_only_after_dispatch_returns);
    RUN_TEST(test_post_wait_blocks_for_handler_duration);
    RUN_TEST(test_post_wait_to_full_queue_returns_failed_and_caller_owns_event);
    RUN_TEST(test_post_wait_on_disabled_ao_returns_failed);
    RUN_TEST(test_post_wait_from_ao_own_task_returns_failed);
    RUN_TEST(test_post_wait_timeout_returns_timeout_with_event_still_queued);
    RUN_TEST(test_post_wait_timeout_during_dispatch_waits_for_handler);
    RUN_TEST(test_post_wait_discarded_by_disable_returns_dropped);
    RUN_TEST(test_post_wait_does_not_leak_pool_events);
}
