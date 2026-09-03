/*******************************************************************************
 * test_enable_disable.c
 *
 * Unit tests for activert_active_set_enabled() / activert_active_is_enabled().
 * Verifies that:
 *   - Active Objects start enabled
 *   - A disabled Active Object rejects posts and leaves the event with the caller
 *   - An event queued before the disable is discarded rather than dispatched
 *   - Re-enabling restores dispatch
 *   - Notification handlers keep running while event dispatch is disabled
 *   - Loop tasks and notification-only Active Objects reject the call
 *
 * The Active Objects here run at a lower priority than the Unity runner task,
 * so the test task can post and then disable before the AO has had a chance to
 * dequeue anything.
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
    ED_SIG_A = ACTIVERT_USER_SIG,
    ED_SIG_B
};

/*----------------------------------------------------------------------------
 * Event type and pool
 *--------------------------------------------------------------------------*/
typedef struct
{
    activert_event_t base;
    uint32_t payload;
} ed_event_t;

#define ED_POOL_SIZE 4

ACTIVERT_EVENT_POOL_DEFINE(ed_pool, ed_event_t, ED_POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP);

/*----------------------------------------------------------------------------
 * Main Active Object under test
 *--------------------------------------------------------------------------*/
#define ED_AO_STACK_BYTES 4096
#define ED_AO_QUEUE_DEPTH 4
#define ED_AO_PRIORITY    2

ACTIVERT_ACTIVE_DEFINE_SIMPLE(
    ed_ao, ed_dispatch, ED_AO_PRIORITY, ED_AO_STACK_BYTES, ED_AO_QUEUE_DEPTH
);

static volatile uint32_t s_dispatch_count;
static SemaphoreHandle_t s_dispatch_sem;

static void ed_dispatch(activert_active_t* me, const activert_event_t* evt)
{
    (void)me;

    if ((evt->sig == ED_SIG_A) || (evt->sig == ED_SIG_B))
    {
        s_dispatch_count++;
        (void)xSemaphoreGive(s_dispatch_sem);
    }
}

/*----------------------------------------------------------------------------
 * Helper: allocate and post one event, returning the post result
 *--------------------------------------------------------------------------*/
static int post_one(activert_active_t* ao, activert_signal_t sig, ed_event_t** out)
{
    ed_event_t* evt = (ed_event_t*)activert_event_pool_alloc(ed_pool);
    TEST_ASSERT_NOT_NULL(evt);

    evt->base.sig = sig;
    evt->payload  = 0U;

    if (out != NULL)
    {
        *out = evt;
    }

    return activert_active_post(ao, &evt->base);
}

/*----------------------------------------------------------------------------
 * Unity setUp / tearDown
 *--------------------------------------------------------------------------*/
void setUp(void)
{
    s_dispatch_count = 0;

    ACTIVERT_EVENT_POOL_INIT(ed_pool, ed_event_t, ED_POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP);

    s_dispatch_sem = xSemaphoreCreateBinary();
    configASSERT(s_dispatch_sem != NULL);

    ACTIVERT_ACTIVE_INIT_SIMPLE(ed_ao, ed_dispatch, ED_AO_PRIORITY);
}

void tearDown(void)
{
    if (ed_ao != NULL)
    {
        activert_active_stop(ed_ao);
        vTaskDelay(pdMS_TO_TICKS(20));
        ed_ao = NULL;
    }

    if (s_dispatch_sem != NULL)
    {
        vSemaphoreDelete(s_dispatch_sem);
        s_dispatch_sem = NULL;
    }
}

/*============================================================================
 * Tests
 *==========================================================================*/

void test_ao_starts_enabled(void)
{
    TEST_ASSERT_TRUE(activert_active_is_enabled(ed_ao));
}

void test_disabled_ao_rejects_post(void)
{
    ed_event_t* evt = NULL;

    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(ed_ao, false));
    TEST_ASSERT_FALSE(activert_active_is_enabled(ed_ao));

    TEST_ASSERT_EQUAL_INT(-1, post_one(ed_ao, ED_SIG_A, &evt));

    /* A failed post leaves the event with the caller, same as a full queue
     * does. So freeing it here must return the slot to the pool. */
    activert_event_pool_free(&evt->base);
    TEST_ASSERT_EQUAL_size_t(ED_POOL_SIZE, activert_event_pool_get_free_count(ed_pool));

    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_UINT32(0, s_dispatch_count);
}

#if ACTIVERT_ENABLE_STATS
void test_disabled_post_increments_events_dropped(void)
{
    ed_event_t* evt = NULL;
    uint32_t before;

    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(ed_ao, false));

    before = ed_ao->stats.events_dropped;
    TEST_ASSERT_EQUAL_INT(-1, post_one(ed_ao, ED_SIG_A, &evt));
    TEST_ASSERT_EQUAL_UINT32(before + 1U, ed_ao->stats.events_dropped);

    activert_event_pool_free(&evt->base);
}
#endif /* ACTIVERT_ENABLE_STATS */

void test_event_queued_before_disable_is_discarded_not_dispatched(void)
{
    /* The runner task has higher priority than the AO, so the event 
     * is queued and then the AO is disabled before it ever wakes. */
    TEST_ASSERT_EQUAL_INT(0, post_one(ed_ao, ED_SIG_A, NULL));
    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(ed_ao, false));

    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL_UINT32(0, s_dispatch_count);

    /* Discarded, not leaked: the event loop freed it back to the pool. */
    TEST_ASSERT_EQUAL_size_t(ED_POOL_SIZE, activert_event_pool_get_free_count(ed_pool));
}

void test_reenabled_ao_dispatches_again(void)
{
    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(ed_ao, false));
    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(ed_ao, true));
    TEST_ASSERT_TRUE(activert_active_is_enabled(ed_ao));

    TEST_ASSERT_EQUAL_INT(0, post_one(ed_ao, ED_SIG_A, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_dispatch_sem, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_UINT32(1, s_dispatch_count);
}

/*----------------------------------------------------------------------------
 * Notification Active Object: disable must not silence notifications
 *--------------------------------------------------------------------------*/
#define ED_NQ 2

static volatile int s_notify_count;
static volatile int s_notify_dispatch_count;

static void ed_notify_dispatch(activert_active_t* me, const activert_event_t* evt)
{
    (void)me;
    if (evt->sig == ED_SIG_A)
    {
        s_notify_dispatch_count++;
    }
}

static void ed_notify_handler(activert_active_t* me, uint32_t bits)
{
    (void)me;
    (void)bits;
    s_notify_count++;
}

static StackType_t ed_n_stack[4096 / sizeof(StackType_t)];
static StaticTask_t ed_n_tcb;
static activert_event_t* ed_n_qstore[ED_NQ];
static StaticQueue_t ed_n_qcb;
static StaticSemaphore_t ed_n_sem_cb;
static StaticQueue_t ed_n_set_cb;
static uint8_t ed_n_set_store[ACTIVERT_NOTIFY_QUEUE_SET_STORAGE_BYTES(ED_NQ)];
static activert_active_t ed_n_ao_storage;
static activert_queue_t ed_n_qstruct;

void test_disabled_ao_still_runs_notification_handler(void)
{
    ed_event_t* evt = NULL;

    s_notify_count          = 0;
    s_notify_dispatch_count = 0;

    activert_queue_config_t cfg = {
        .signal_base = 0, .signal_count = 0, .queue_length = ED_NQ, .event_pool = ed_pool
    };
    activert_event_t** qsa[1] = {ed_n_qstore};

    activert_active_t* ao = activert_active_create_with_notification_static(
        "ed_notify",
        ed_notify_dispatch,
        ed_notify_handler,
        ED_AO_PRIORITY,
        ed_n_stack,
        sizeof(ed_n_stack),
        &ed_n_tcb,
        &cfg,
        1,
        &ed_n_qcb,
        qsa,
        &ed_n_set_cb,
        ed_n_set_store,
        &ed_n_sem_cb,
        &ed_n_ao_storage,
        &ed_n_qstruct
    );
    TEST_ASSERT_NOT_NULL(ao);

    TEST_ASSERT_EQUAL_INT(0, activert_active_set_enabled(ao, false));

    /* Events are rejected ... */
    evt = (ed_event_t*)activert_event_pool_alloc(ed_pool);
    TEST_ASSERT_NOT_NULL(evt);
    evt->base.sig = ED_SIG_A;
    TEST_ASSERT_EQUAL_INT(-1, activert_active_post(ao, &evt->base));
    activert_event_pool_free(&evt->base);

    /* ... but notifications still reach the handler. */
    activert_active_notify(ao, 0x1U);
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, s_notify_count);
    TEST_ASSERT_EQUAL_INT(0, s_notify_dispatch_count);

    activert_active_stop(ao);
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*----------------------------------------------------------------------------
 * Active Objects with no event dispatch to gate
 *--------------------------------------------------------------------------*/
static volatile int s_loop_ticks;

static void ed_loop_fn(activert_active_t* me)
{
    (void)me;
    s_loop_ticks++;
    vTaskDelay(pdMS_TO_TICKS(10));
}

ACTIVERT_ACTIVE_DEFINE_LOOP(ed_loop_ao, 4096);

void test_set_enabled_rejects_loop_task(void)
{
    s_loop_ticks = 0;

    ACTIVERT_ACTIVE_INIT_LOOP(ed_loop_ao, NULL, ed_loop_fn, ED_AO_PRIORITY);
    TEST_ASSERT_NOT_NULL(ed_loop_ao);

    TEST_ASSERT_EQUAL_INT(-1, activert_active_set_enabled(ed_loop_ao, false));

    TEST_ASSERT_TRUE(activert_active_is_enabled(ed_loop_ao));

    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_GREATER_THAN_INT(0, s_loop_ticks);

    activert_active_stop(ed_loop_ao);
    vTaskDelay(pdMS_TO_TICKS(20));
    ed_loop_ao = NULL;
}

static void ed_qless_notify(activert_active_t* me, uint32_t bits)
{
    (void)me;
    (void)bits;
    s_notify_count++;
}

static StackType_t ed_q_stack[4096 / sizeof(StackType_t)];
static StaticTask_t ed_q_tcb;
static StaticSemaphore_t ed_q_sem_cb;
static activert_active_t ed_q_ao_storage;

void test_set_enabled_rejects_notification_only_ao(void)
{
    s_notify_count = 0;

    activert_active_t* ao = activert_active_create_with_notification_static(
        "ed_qless",
        NULL,
        ed_qless_notify,
        ED_AO_PRIORITY,
        ed_q_stack,
        sizeof(ed_q_stack),
        &ed_q_tcb,
        NULL,
        0, /* notification-only */
        NULL,
        NULL,
        NULL,
        NULL,
        &ed_q_sem_cb,
        &ed_q_ao_storage,
        NULL
    );
    TEST_ASSERT_NOT_NULL(ao);

    TEST_ASSERT_EQUAL_INT(-1, activert_active_set_enabled(ao, false));
    TEST_ASSERT_TRUE(activert_active_is_enabled(ao));

    activert_active_notify(ao, 0x2U);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, s_notify_count);

    activert_active_stop(ao);
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*----------------------------------------------------------------------------
 * run_tests, called by freertos_test_main.c
 *--------------------------------------------------------------------------*/
void run_tests(void)
{
    RUN_TEST(test_ao_starts_enabled);
    RUN_TEST(test_disabled_ao_rejects_post);
#if ACTIVERT_ENABLE_STATS
    RUN_TEST(test_disabled_post_increments_events_dropped);
#endif
    RUN_TEST(test_event_queued_before_disable_is_discarded_not_dispatched);
    RUN_TEST(test_reenabled_ao_dispatches_again);
    RUN_TEST(test_disabled_ao_still_runs_notification_handler);
    RUN_TEST(test_set_enabled_rejects_loop_task);
    RUN_TEST(test_set_enabled_rejects_notification_only_ao);
}
