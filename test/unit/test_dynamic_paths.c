/*******************************************************************************
 * test_dynamic_paths.c
 *
 * Regression tests for the dynamic-allocation paths. Compiled with
 * ACTIVERT_ENABLE_DYNAMIC_ALLOCATION=1.
 *
 * Covered fixes:
 *   - activert_active_create_dynamic() now allocates the
 *     me->queues array before writing through it (single and
 *     multi-queue), instead of dereferencing a NULL array.
 *   - activert_active_destroy() and activert_event_pool_destroy()
 *     unregister from the global stats registry (no dangling
 *     pointer / inflated count).
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdint.h>

enum
{
    SIG_A = ACTIVERT_USER_SIG,
    SIG_B
};

typedef struct
{
    activert_event_t base;
    uint32_t v;
} ev_t;

static SemaphoreHandle_t s_sync;

void setUp(void)
{
    s_sync = xSemaphoreCreateBinary();
    configASSERT(s_sync != NULL);
}
void tearDown(void)
{
    if (s_sync != NULL)
    {
        vSemaphoreDelete(s_sync);
        s_sync = NULL;
    }
}

ACTIVERT_EVENT_POOL_DEFINE(dp_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);

static volatile int g_disp_a;
static volatile int g_disp_b;
static void dp_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    if (e->sig == SIG_A)
    {
        g_disp_a++;
        xSemaphoreGive(s_sync);
    }
    else if (e->sig == SIG_B)
    {
        g_disp_b++;
        xSemaphoreGive(s_sync);
    }
}

void test_dynamic_single_queue_create_dispatch_destroy(void)
{
    g_disp_a = 0;
    ACTIVERT_EVENT_POOL_INIT(dp_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);

    activert_queue_config_t cfg = {
        .signal_base = 0, .signal_count = 0, .queue_length = 4, .event_pool = dp_pool
    };

    size_t a0 = activert_stats_get_active_count();
    activert_active_t* ao = activert_active_create_dynamic("dyn1", dp_dispatch, 2, 4096, &cfg, 1);
    TEST_ASSERT_NOT_NULL(ao);
    TEST_ASSERT_NOT_NULL(ao->queues);
    TEST_ASSERT_EQUAL_size_t(a0 + 1, activert_stats_get_active_count());

    ev_t* e = (ev_t*)activert_event_pool_alloc(dp_pool);
    TEST_ASSERT_NOT_NULL(e);
    e->base.sig = SIG_A;
    TEST_ASSERT_EQUAL_INT(0, activert_active_post(ao, &e->base));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_sync, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_INT(1, g_disp_a);

    activert_active_destroy(ao);
    TEST_ASSERT_EQUAL_size_t(a0, activert_stats_get_active_count());
}

void test_dynamic_multi_queue_create_routes_and_destroys(void)
{
    g_disp_a = 0;
    g_disp_b = 0;
    ACTIVERT_EVENT_POOL_INIT(dp_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);

    activert_queue_config_t cfgs[2] = {
        {.signal_base = SIG_A, .signal_count = 1, .queue_length = 4, .event_pool = dp_pool},
        {.signal_base = SIG_B, .signal_count = 1, .queue_length = 4, .event_pool = dp_pool},
    };

    size_t a0 = activert_stats_get_active_count();
    activert_active_t* ao = activert_active_create_dynamic("dyn2", dp_dispatch, 2, 4096, cfgs, 2);
    TEST_ASSERT_NOT_NULL(ao);
    TEST_ASSERT_NOT_NULL(ao->queues);
    TEST_ASSERT_EQUAL_UINT8(2, ao->queue_count);

    ev_t* ea = (ev_t*)activert_event_pool_alloc(dp_pool);
    ev_t* eb = (ev_t*)activert_event_pool_alloc(dp_pool);
    ea->base.sig = SIG_A;
    eb->base.sig = SIG_B;
    TEST_ASSERT_EQUAL_INT(0, activert_active_post(ao, &ea->base)); /* -> queue 0 */
    TEST_ASSERT_EQUAL_INT(0, activert_active_post(ao, &eb->base)); /* -> queue 1 */

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_sync, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_sync, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_INT(1, g_disp_a);
    TEST_ASSERT_EQUAL_INT(1, g_disp_b);

    activert_active_destroy(ao);
    TEST_ASSERT_EQUAL_size_t(a0, activert_stats_get_active_count());
}

void test_dynamic_pool_destroy_unregisters(void)
{
    size_t p0 = activert_stats_get_pool_count();

    activert_event_pool_t* pool =
        activert_event_pool_create_dynamic("dynpool", sizeof(ev_t), 8, ACTIVERT_POOL_OVERFLOW_DROP);
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_size_t(p0 + 1, activert_stats_get_pool_count());

    activert_event_pool_destroy(pool);
    TEST_ASSERT_EQUAL_size_t(p0, activert_stats_get_pool_count());
}

ACTIVERT_EVENT_POOL_DEFINE(of_pool, ev_t, 1, ACTIVERT_POOL_OVERFLOW_DYNAMIC);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(of_ao, of_dispatch, 2, 4096, 8);

static volatile uint32_t g_of_dispatched;
void of_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    if (e->sig == SIG_A)
    {
        g_of_dispatched++;
    }
}

void test_dynamic_overflow_events_do_not_leak(void)
{
    g_of_dispatched = 0;
    ACTIVERT_EVENT_POOL_INIT(of_pool, ev_t, 1, ACTIVERT_POOL_OVERFLOW_DYNAMIC);
    ACTIVERT_ACTIVE_INIT_SIMPLE(of_ao, of_dispatch, 2);
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Hold the single static slot so every posted event is a malloc'd overflow
     * event (event->pool == NULL). */
    ev_t* held = (ev_t*)activert_event_pool_alloc(of_pool);
    TEST_ASSERT_NOT_NULL(held);

    const int N = 50;
    size_t heap_before = xPortGetFreeHeapSize();

    for (int i = 0; i < N; i++)
    {
        ev_t* e = (ev_t*)activert_event_pool_alloc(of_pool); /* overflow -> malloc */
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_NULL(e->base.pool); /* confirm it is a dynamic-overflow event */
        e->base.sig = SIG_A;
        TEST_ASSERT_EQUAL_INT(0, activert_active_post(of_ao, &e->base));
        vTaskDelay(pdMS_TO_TICKS(2)); /* let the AO dispatch + auto-free */
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL_UINT32((uint32_t)N, g_of_dispatched);

    size_t heap_after = xPortGetFreeHeapSize();

    activert_event_pool_free(&held->base);
    activert_active_stop(of_ao);
    of_ao = NULL;
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Each dispatched overflow event is now freed, so the heap returns to its
     * baseline (no leak). */
    TEST_ASSERT_EQUAL_size_t(heap_before, heap_after);
}

void run_tests(void)
{
    RUN_TEST(test_dynamic_single_queue_create_dispatch_destroy);
    RUN_TEST(test_dynamic_multi_queue_create_routes_and_destroys);
    RUN_TEST(test_dynamic_pool_destroy_unregisters);
    RUN_TEST(test_dynamic_overflow_events_do_not_leak);
}
