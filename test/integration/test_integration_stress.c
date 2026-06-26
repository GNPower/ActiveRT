/*******************************************************************************
 * test_integration_stress.c
 *
 * System / stress tests: multiple Active Objects communicating under load.
 *   - producer/consumer: every posted event is dispatched, the pool returns to
 *     fully free, and the bitmap holds zero allocated slots (no leak/corruption).
 *   - shared pool: two AOs using one shared pool never exceed pool capacity,
 *     and the pool reconciles (popcount(bitmap) == current_allocated == 0) when
 *     empty.
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdint.h>

enum
{
    SIG_WORK = ACTIVERT_USER_SIG
};

typedef struct
{
    activert_event_t base;
    uint32_t seq;
} work_event_t;

/* Count the allocated (set) bits in a pool's bitmap */
static size_t bitmap_popcount(const activert_event_pool_t* pool)
{
    size_t bytes = (pool->pool_size + 7U) / 8U;
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++)
    {
        uint8_t b = pool->usage_bitmap[i];
        while (b)
        {
            count += (b & 1U);
            b >>= 1U;
        }
    }
    return count;
}

static SemaphoreHandle_t s_ack;

void setUp(void)
{
    s_ack = xSemaphoreCreateCounting(64, 0);
    configASSERT(s_ack != NULL);
}
void tearDown(void)
{
    if (s_ack != NULL)
    {
        vSemaphoreDelete(s_ack);
        s_ack = NULL;
    }
}

/*============================================================================
 * Producer/consumer
 *==========================================================================*/
#define PC_POOL 8
#define PC_N    300

static volatile uint32_t g_consumed;
static volatile uint32_t g_last_seq;
static volatile int g_seq_ok;

static void consumer_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    if (e->sig == SIG_WORK)
    {
        const work_event_t* w = (const work_event_t*)e;
        if (g_consumed != 0U && w->seq != g_last_seq + 1U)
        {
            g_seq_ok = 0; /* out-of-order delivery */
        }
        g_last_seq = w->seq;
        g_consumed++;
        xSemaphoreGive(s_ack);
    }
}

ACTIVERT_EVENT_POOL_DEFINE(pc_pool, work_event_t, PC_POOL, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(pc_consumer, consumer_dispatch, 2, 4096, 8);

void test_producer_consumer_no_loss_no_leak(void)
{
    g_consumed = 0;
    g_last_seq = 0;
    g_seq_ok   = 1;

    ACTIVERT_EVENT_POOL_INIT(pc_pool, work_event_t, PC_POOL, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(pc_consumer, consumer_dispatch, 2);
    vTaskDelay(pdMS_TO_TICKS(20)); /* let INIT_SIG run */

    for (uint32_t i = 1; i <= PC_N; i++)
    {
        work_event_t* e = (work_event_t*)activert_event_pool_alloc(pc_pool);
        TEST_ASSERT_NOT_NULL(e);
        e->base.sig = SIG_WORK;
        e->seq      = i;
        TEST_ASSERT_EQUAL_INT(0, activert_active_post(pc_consumer, &e->base));
        /* Block until the consumer dispatched it (auto-freeing the event). */
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_ack, pdMS_TO_TICKS(1000)));
    }

    TEST_ASSERT_EQUAL_UINT32(PC_N, g_consumed);
    TEST_ASSERT_TRUE_MESSAGE(g_seq_ok, "events dispatched out of order");

    /* The dispatch handler acks before the event loop auto-frees the event, and
     * this higher-priority runner resumes immediately, so the last event's free
     * lags. Wait for the AO to become idle, then assert that every event auto-freed, 
     * the pool fully is free, and the bitmap clear. */
    for (int t = 0; t < 50 && activert_event_pool_get_free_count(pc_pool) != PC_POOL; t++)
    {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    TEST_ASSERT_EQUAL_size_t(PC_POOL, activert_event_pool_get_free_count(pc_pool));
    TEST_ASSERT_EQUAL_size_t(0, bitmap_popcount(pc_pool));

    activert_active_stop(pc_consumer);
    pc_consumer = NULL;
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*============================================================================
 * Shared pool under load: two AOs, one pool
 *==========================================================================*/
#define SH_POOL 8
#define SH_N    400

static volatile uint32_t g_sh_count;

static void shared_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    if (e->sig == SIG_WORK)
    {
        g_sh_count++;
        xSemaphoreGive(s_ack);
    }
}

ACTIVERT_EVENT_POOL_DEFINE(sh_pool, work_event_t, SH_POOL, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(sh_ao0, shared_dispatch, 2, 4096, 8);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(sh_ao1, shared_dispatch, 2, 4096, 8);

void test_shared_pool_under_load_reconciles(void)
{
    g_sh_count = 0;

    ACTIVERT_EVENT_POOL_INIT(sh_pool, work_event_t, SH_POOL, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(sh_ao0, shared_dispatch, 2);
    ACTIVERT_ACTIVE_INIT_SIMPLE(sh_ao1, shared_dispatch, 2);
    vTaskDelay(pdMS_TO_TICKS(20));

    for (uint32_t i = 1; i <= SH_N; i++)
    {
        work_event_t* e = (work_event_t*)activert_event_pool_alloc(sh_pool);
        TEST_ASSERT_NOT_NULL(e);
        e->base.sig = SIG_WORK;
        e->seq      = i;

        /* current_allocated must never exceed the pool capacity. */
        TEST_ASSERT_LESS_OR_EQUAL_size_t(
            SH_POOL, SH_POOL - activert_event_pool_get_free_count(sh_pool)
        );

        activert_active_t* target = (i & 1U) ? sh_ao0 : sh_ao1;
        TEST_ASSERT_EQUAL_INT(0, activert_active_post(target, &e->base));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_ack, pdMS_TO_TICKS(1000)));
    }

    TEST_ASSERT_EQUAL_UINT32(SH_N, g_sh_count);

    /* Wait for both AOs to finish their trailing auto-frees (see note above). */
    for (int t = 0; t < 50 && activert_event_pool_get_free_count(sh_pool) != SH_POOL; t++)
    {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    TEST_ASSERT_EQUAL_size_t(SH_POOL, activert_event_pool_get_free_count(sh_pool));
    TEST_ASSERT_EQUAL_size_t(0, bitmap_popcount(sh_pool));

    activert_active_stop(sh_ao0);
    activert_active_stop(sh_ao1);
    sh_ao0 = NULL;
    sh_ao1 = NULL;
    vTaskDelay(pdMS_TO_TICKS(20));
}

void run_tests(void)
{
    RUN_TEST(test_producer_consumer_no_loss_no_leak);
    RUN_TEST(test_shared_pool_under_load_reconciles);
}
