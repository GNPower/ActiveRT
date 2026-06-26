/*******************************************************************************
 * test_regression_core.c
 *
 * Regression tests for high-severity bugs.
 *
 * Covered fixes:
 *   - queue-less AO with a non-NULL dispatch handler no longer
 *     NULL-derefs me->queues, it routes to the notification path.
 *   - ACTIVERT_QUEUE_SET_STORAGE_BYTES / ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES
 *     and a notification AO whose set storage is sized with the macro
 *     survives a full queue + notification without corruption.
 *   - pool alloc/free still work after the mutex->critical-section change,
 *     including the *_from_isr variants.
 *   - A failed post (queue full) returns -1 and leaves ownership of
 *     the event with the caller as intended.
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdint.h>
#include <string.h>

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

/*============================================================================
 * Test: queue-less AO with a non-NULL dispatch handler
 *==========================================================================*/
static volatile int g_qless_init;
static volatile int g_qless_notify;
static void qless_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    if (e->sig == ACTIVERT_INIT_SIG)
    {
        g_qless_init++;
    }
}
static void qless_notify(activert_active_t* me, uint32_t bits)
{
    (void)me;
    (void)bits;
    g_qless_notify++;
    xSemaphoreGive(s_sync);
}
static StackType_t qless_stack[4096 / sizeof(StackType_t)];
static StaticTask_t qless_tcb;
static StaticSemaphore_t qless_sem_cb;
static activert_active_t qless_ao_storage;

void test_queue_less_ao_with_dispatch_does_not_crash(void)
{
    g_qless_init   = 0;
    g_qless_notify = 0;

    activert_active_t* ao = activert_active_create_with_notification_static(
        "qless", qless_dispatch, qless_notify, 3,
        qless_stack, sizeof(qless_stack), &qless_tcb,
        NULL, 0,                 /* num_queues == 0 (notification-only) */
        NULL, NULL, NULL, NULL, &qless_sem_cb,
        &qless_ao_storage, NULL);

    TEST_ASSERT_NOT_NULL(ao);

    /* The task starts, dispatches INIT_SIG, then reaches the loop. Before the
     * fix this dereferenced me->queues == NULL and crashed here. */
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_INT(1, g_qless_init);

    activert_active_notify(ao, 0x5);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_sync, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_INT(1, g_qless_notify);

    activert_active_stop(ao);
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*============================================================================
 * Test: queue-set storage sizing macros
 *==========================================================================*/
void test_queue_set_storage_macros(void)
{
    TEST_ASSERT_EQUAL_size_t(4U * sizeof(void*), ACTIVERT_QUEUE_SET_STORAGE_BYTES(4));
    TEST_ASSERT_EQUAL_size_t(5U * sizeof(void*), ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(4));
    TEST_ASSERT_EQUAL_size_t(1U * sizeof(void*), ACTIVERT_QUEUE_SET_STORAGE_BYTES(1));
    TEST_ASSERT_EQUAL_size_t(2U * sizeof(void*), ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(1));
}

/*============================================================================
 * Test: notification AO with macro-sized set storage under load
 *==========================================================================*/
#define NQ 4
static volatile int g_n_dispatch;
static volatile int g_n_notify;
static void n_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    if (e->sig == SIG_A)
    {
        g_n_dispatch++;
    }
}
static void n_notify(activert_active_t* me, uint32_t bits)
{
    (void)me;
    (void)bits;
    g_n_notify++;
}
static StackType_t n_stack[4096 / sizeof(StackType_t)];
static StaticTask_t n_tcb;
static activert_event_t* n_qstore[NQ];
static StaticQueue_t n_qcb;
static StaticSemaphore_t n_sem_cb;
static StaticQueue_t n_set_cb;
static uint8_t n_set_store[ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(NQ)];
static activert_active_t n_ao_storage;
static activert_queue_t n_qstruct;
ACTIVERT_EVENT_POOL_DEFINE(n_pool, ev_t, NQ, ACTIVERT_POOL_OVERFLOW_DROP);

void test_notification_ao_macro_sized_storage_no_corruption(void)
{
    g_n_dispatch = 0;
    g_n_notify   = 0;
    ACTIVERT_EVENT_POOL_INIT(n_pool, ev_t, NQ, ACTIVERT_POOL_OVERFLOW_DROP);

    activert_queue_config_t cfg = {
        .signal_base = 0, .signal_count = 0, .queue_length = NQ, .event_pool = n_pool
    };
    activert_event_t** qsa[1] = {n_qstore};

    activert_active_t* ao = activert_active_create_with_notification_static(
        "nao", n_dispatch, n_notify, 3,
        n_stack, sizeof(n_stack), &n_tcb,
        &cfg, 1, &n_qcb, qsa, &n_set_cb, n_set_store, &n_sem_cb,
        &n_ao_storage, &n_qstruct);
    TEST_ASSERT_NOT_NULL(ao);

    for (int i = 0; i < NQ; i++)
    {
        ev_t* e = (ev_t*)activert_event_pool_alloc(n_pool);
        TEST_ASSERT_NOT_NULL(e);
        e->base.sig = SIG_A;
        TEST_ASSERT_EQUAL_INT(0, activert_active_post(ao, &e->base));
    }
    activert_active_notify(ao, 0x1);

    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL_INT(NQ, g_n_dispatch);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, g_n_notify);

    activert_active_stop(ao);
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*============================================================================
 * Test: pool alloc/free (task + ISR variants) after lock change
 *==========================================================================*/
ACTIVERT_EVENT_POOL_DEFINE(rc_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);

void test_pool_alloc_free_basic(void)
{
    ACTIVERT_EVENT_POOL_INIT(rc_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(rc_pool));

    ev_t* a = (ev_t*)activert_event_pool_alloc(rc_pool);
    ev_t* b = (ev_t*)activert_event_pool_alloc(rc_pool);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_size_t(2, activert_event_pool_get_free_count(rc_pool));

    activert_event_pool_free(&a->base);
    activert_event_pool_free(&b->base);
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(rc_pool));
}

void test_isr_alloc_free_variants(void)
{
    ACTIVERT_EVENT_POOL_INIT(rc_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);

    ev_t* e = (ev_t*)activert_event_pool_alloc_from_isr(rc_pool);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_size_t(3, activert_event_pool_get_free_count(rc_pool));

    activert_event_pool_free_from_isr(&e->base);
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(rc_pool));
}

/*============================================================================
 * Test: failed post returns -1 and the caller still owns the event
 *==========================================================================*/
static void fp_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    (void)e;
}
ACTIVERT_EVENT_POOL_DEFINE(fp_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(fp_ao, fp_dispatch, 1, 4096, 2);

void test_failed_post_returns_error_and_caller_owns_event(void)
{
    ACTIVERT_EVENT_POOL_INIT(fp_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(fp_ao, fp_dispatch, 1);
    /* No vTaskDelay: this (high-priority) runner keeps the CPU so the prio-1 AO
     * never drains the queue, making the queue-full condition deterministic. */

    ev_t* e1 = (ev_t*)activert_event_pool_alloc(fp_pool);
    ev_t* e2 = (ev_t*)activert_event_pool_alloc(fp_pool);
    ev_t* e3 = (ev_t*)activert_event_pool_alloc(fp_pool);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_NOT_NULL(e2);
    TEST_ASSERT_NOT_NULL(e3);
    e1->base.sig = SIG_A;
    e2->base.sig = SIG_A;
    e3->base.sig = SIG_A;

    TEST_ASSERT_EQUAL_INT(0, activert_active_post(fp_ao, &e1->base));  /* queue depth 2 */
    TEST_ASSERT_EQUAL_INT(0, activert_active_post(fp_ao, &e2->base));
    TEST_ASSERT_EQUAL_INT(-1, activert_active_post(fp_ao, &e3->base)); /* full */

    /* Per the API, e3 is NOT freed by a failed post, instead the caller owns
     * it and must free it. Verify that freeing it returns it to the pool. */
    size_t free_before = activert_event_pool_get_free_count(fp_pool);
    activert_event_pool_free(&e3->base);
    TEST_ASSERT_EQUAL_size_t(free_before + 1, activert_event_pool_get_free_count(fp_pool));

    activert_active_stop(fp_ao);
    fp_ao = NULL;
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*============================================================================
 * Test: notify(ao, 0) is delivered on the semaphore (queue+notify) path
 *==========================================================================*/
#define NZQ 4
static volatile int g_nz_calls;
static volatile uint32_t g_nz_value;
static void nz_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    (void)e;
}
static void nz_notify(activert_active_t* me, uint32_t bits)
{
    (void)me;
    g_nz_calls++;
    g_nz_value = bits;
    xSemaphoreGive(s_sync);
}
static StackType_t nz_stack[4096 / sizeof(StackType_t)];
static StaticTask_t nz_tcb;
static activert_event_t* nz_qstore[NZQ];
static StaticQueue_t nz_qcb;
static StaticSemaphore_t nz_sem_cb;
static StaticQueue_t nz_set_cb;
static uint8_t nz_set_store[ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(NZQ)];
static activert_active_t nz_ao_storage;
static activert_queue_t nz_qstruct;
ACTIVERT_EVENT_POOL_DEFINE(nz_pool, ev_t, NZQ, ACTIVERT_POOL_OVERFLOW_DROP);

void test_notify_zero_delivered_on_semaphore_path(void)
{
    g_nz_calls = 0;
    g_nz_value = 0xFFFFFFFFu;
    ACTIVERT_EVENT_POOL_INIT(nz_pool, ev_t, NZQ, ACTIVERT_POOL_OVERFLOW_DROP);

    activert_queue_config_t cfg = {
        .signal_base = 0, .signal_count = 0, .queue_length = NZQ, .event_pool = nz_pool
    };
    activert_event_t** qsa[1] = {nz_qstore};

    activert_active_t* ao = activert_active_create_with_notification_static(
        "nzao", nz_dispatch, nz_notify, 3,
        nz_stack, sizeof(nz_stack), &nz_tcb,
        &cfg, 1, &nz_qcb, qsa, &nz_set_cb, nz_set_store, &nz_sem_cb,
        &nz_ao_storage, &nz_qstruct);
    TEST_ASSERT_NOT_NULL(ao);
    vTaskDelay(pdMS_TO_TICKS(20));

    activert_active_notify(ao, 0x0); /* a notify with value 0 must still fire */
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_sync, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_INT(1, g_nz_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_nz_value);

    activert_active_stop(ao);
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*============================================================================
 * Test: post_to_queue rejects an out-of-range index with -1
 *==========================================================================*/
static void bi_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    (void)e;
}
ACTIVERT_EVENT_POOL_DEFINE(bi_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(bi_ao, bi_dispatch, 2, 4096, 4);

void test_post_to_queue_rejects_out_of_range_index(void)
{
    ACTIVERT_EVENT_POOL_INIT(bi_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(bi_ao, bi_dispatch, 2);
    vTaskDelay(pdMS_TO_TICKS(10));

    ev_t* e = (ev_t*)activert_event_pool_alloc(bi_pool);
    TEST_ASSERT_NOT_NULL(e);
    e->base.sig = SIG_A;

    /* bi_ao has a single queue (index 0). Index 5 is out of range and the guard
     * must return -1 rather than indexing past me->queues. */
    TEST_ASSERT_EQUAL_INT(-1, activert_active_post_to_queue(bi_ao, 5, &e->base));

    /* The event was not consumed by a failed post, so free it. */
    activert_event_pool_free(&e->base);
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(bi_pool));

    activert_active_stop(bi_ao);
    bi_ao = NULL;
    vTaskDelay(pdMS_TO_TICKS(10));
}

void run_tests(void)
{
    RUN_TEST(test_queue_less_ao_with_dispatch_does_not_crash);
    RUN_TEST(test_queue_set_storage_macros);
    RUN_TEST(test_notification_ao_macro_sized_storage_no_corruption);
    RUN_TEST(test_pool_alloc_free_basic);
    RUN_TEST(test_isr_alloc_free_variants);
    RUN_TEST(test_failed_post_returns_error_and_caller_owns_event);
    RUN_TEST(test_notify_zero_delivered_on_semaphore_path);
    RUN_TEST(test_post_to_queue_rejects_out_of_range_index);
}
