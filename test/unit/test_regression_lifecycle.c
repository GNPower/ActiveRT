/*******************************************************************************
 * test_regression_lifecycle.c
 *
 * Regression tests for the medium-severity lifecycle / stats fixes. MUST pass.
 *
 *   - a double free is rejected and does not underflow
 *     the current_allocated / the free count.
 *   - re-initializing a static pool or AO does not add a
 *     duplicate registry entry.
 *   - health check flags any pool allocation failure, and flags a
 *     queue overflow only when a post actually failed (drop).
 *   - health check does not crash / mis-read when an AO has been
 *     stopped (thread == NULL).
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <string.h>

enum
{
    SIG_A = ACTIVERT_USER_SIG
};

typedef struct
{
    activert_event_t base;
    uint32_t v;
} ev_t;

void setUp(void)
{
}
void tearDown(void)
{
}

static int count_pool_occurrences(const activert_event_pool_t* pool)
{
    int n        = 0;
    size_t count = activert_stats_get_pool_count();
    for (size_t i = 0; i < count; i++)
    {
        if (activert_stats_get_pool(i) == pool)
        {
            n++;
        }
    }
    return n;
}

static int count_active_occurrences(const activert_active_t* ao)
{
    int n        = 0;
    size_t count = activert_stats_get_active_count();
    for (size_t i = 0; i < count; i++)
    {
        if (activert_stats_get_active(i) == ao)
        {
            n++;
        }
    }
    return n;
}

/*============================================================================
 * Test: double free is rejected, free count stays correct
 *==========================================================================*/
ACTIVERT_EVENT_POOL_DEFINE(df_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);

void test_double_free_is_rejected(void)
{
    ACTIVERT_EVENT_POOL_INIT(df_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);

    ev_t* e = (ev_t*)activert_event_pool_alloc(df_pool);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_size_t(3, activert_event_pool_get_free_count(df_pool));

    activert_event_pool_free(&e->base);
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(df_pool));

    activert_event_pool_free(&e->base); /* double free must be a safe no-op */
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(df_pool));

    /* The slot is still usable after the rejected double free. */
    ev_t* again = (ev_t*)activert_event_pool_alloc(df_pool);
    TEST_ASSERT_NOT_NULL(again);
    TEST_ASSERT_EQUAL_size_t(3, activert_event_pool_get_free_count(df_pool));
    activert_event_pool_free(&again->base);
    TEST_ASSERT_EQUAL_size_t(4, activert_event_pool_get_free_count(df_pool));
}

/*============================================================================
 * Test: re-init does not duplicate registry entries
 *==========================================================================*/
ACTIVERT_EVENT_POOL_DEFINE(re_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);

void test_pool_reinit_does_not_duplicate_registration(void)
{
    ACTIVERT_EVENT_POOL_INIT(re_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_EVENT_POOL_INIT(re_pool, ev_t, 4, ACTIVERT_POOL_OVERFLOW_DROP); /* re-init */
    TEST_ASSERT_EQUAL_INT(1, count_pool_occurrences(re_pool));
}

static void re_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    (void)e;
}
ACTIVERT_ACTIVE_DEFINE_SIMPLE(re_ao, re_dispatch, 2, 4096, 4);

void test_ao_reinit_does_not_duplicate_registration(void)
{
    ACTIVERT_ACTIVE_INIT_SIMPLE(re_ao, re_dispatch, 2);
    vTaskDelay(pdMS_TO_TICKS(10));
    activert_active_stop(re_ao);
    vTaskDelay(pdMS_TO_TICKS(10));

    ACTIVERT_ACTIVE_INIT_SIMPLE(re_ao, re_dispatch, 2); /* re-init same storage */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL_INT(1, count_active_occurrences(re_ao));

    activert_active_stop(re_ao);
    re_ao = NULL;
    vTaskDelay(pdMS_TO_TICKS(10));
}

/*============================================================================
 * Test: health check flags any pool failure
 *==========================================================================*/
ACTIVERT_EVENT_POOL_DEFINE(hp_pool, ev_t, 2, ACTIVERT_POOL_OVERFLOW_DROP);

void test_health_flags_any_pool_failure(void)
{
    ACTIVERT_EVENT_POOL_INIT(hp_pool, ev_t, 2, ACTIVERT_POOL_OVERFLOW_DROP);

    /* Exhaust the pool, then force exactly one allocation failure. */
    (void)activert_event_pool_alloc(hp_pool);
    (void)activert_event_pool_alloc(hp_pool);
    TEST_ASSERT_NULL(activert_event_pool_alloc(hp_pool)); /* allocs_failed -> 1 */

    activert_health_check_t h;
    TEST_ASSERT_EQUAL_INT(0, activert_stats_health_check(&h));
    TEST_ASSERT_TRUE(h.pool_exhaustion); /* any failure -> WARNING flag set */
}

/*============================================================================
 * Test: health check flags queue overflow only on an actual drop
 *==========================================================================*/
static void ov_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    (void)e;
}
ACTIVERT_EVENT_POOL_DEFINE(ov_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
ACTIVERT_ACTIVE_DEFINE_SIMPLE(ov_ao, ov_dispatch, 1, 4096, 2);

void test_health_flags_queue_overflow_on_drop(void)
{
    ACTIVERT_EVENT_POOL_INIT(ov_pool, ev_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
    ACTIVERT_ACTIVE_INIT_SIMPLE(ov_ao, ov_dispatch, 1);
    /* No delay: this high-priority runner keeps the CPU so the prio-1 AO never
     * drains its depth-2 queue. The third post fails (a real drop). */
    for (int i = 0; i < 3; i++)
    {
        ev_t* e = (ev_t*)activert_event_pool_alloc(ov_pool);
        TEST_ASSERT_NOT_NULL(e);
        e->base.sig = SIG_A;
        (void)activert_active_post(ov_ao, &e->base);
    }

    activert_health_check_t h;
    TEST_ASSERT_EQUAL_INT(0, activert_stats_health_check(&h));
    TEST_ASSERT_TRUE(h.queue_overflow); /* posts_failed > 0 -> CRITICAL flag set */

    activert_active_stop(ov_ao);
    ov_ao = NULL;
    vTaskDelay(pdMS_TO_TICKS(20));
}

/*============================================================================
 * Test: health check does not crash when an AO has been stopped (thread == NULL)
 *==========================================================================*/
static void st_dispatch(activert_active_t* me, const activert_event_t* e)
{
    (void)me;
    (void)e;
}
ACTIVERT_ACTIVE_DEFINE_SIMPLE(st_ao, st_dispatch, 2, 4096, 4);

void test_health_check_after_stop_does_not_crash(void)
{
    ACTIVERT_ACTIVE_INIT_SIMPLE(st_ao, st_dispatch, 2);
    vTaskDelay(pdMS_TO_TICKS(10));

    activert_active_stop(st_ao); /* thread becomes NULL, AO stays registered */
    vTaskDelay(pdMS_TO_TICKS(10));

    activert_health_check_t h;
    /* Must not dereference a NULL task handle (would read the caller's stack or
     * crash). It simply returns successfully. */
    TEST_ASSERT_EQUAL_INT(0, activert_stats_health_check(&h));

    st_ao = NULL;
}

/*============================================================================
 * Test: stats export works into a misaligned buffer and encodes the header
 *==========================================================================*/
void test_stats_export_into_misaligned_buffer(void)
{
    size_t size = activert_stats_get_export_size();
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(2U * sizeof(uint32_t), size);

    /* Over-allocate and export at a deliberately 1-byte-misaligned offset. The
     * header must be written with memcpy (no misaligned 32-bit store / UB) and
     * decode back to the live registry counts. */
    static uint8_t backing[1024];
    uint8_t* misaligned = &backing[1];

    int written = activert_stats_export(misaligned, sizeof(backing) - 1U);
    TEST_ASSERT_GREATER_THAN_INT(0, written);
    TEST_ASSERT_EQUAL_INT((int)size, written);

    uint32_t active_count = 0;
    uint32_t pool_count   = 0;
    memcpy(&active_count, &misaligned[0], sizeof(active_count));
    memcpy(&pool_count, &misaligned[sizeof(uint32_t)], sizeof(pool_count));

    TEST_ASSERT_EQUAL_UINT32((uint32_t)activert_stats_get_active_count(), active_count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)activert_stats_get_pool_count(), pool_count);
}

void run_tests(void)
{
    RUN_TEST(test_double_free_is_rejected);
    RUN_TEST(test_pool_reinit_does_not_duplicate_registration);
    RUN_TEST(test_ao_reinit_does_not_duplicate_registration);
    RUN_TEST(test_health_flags_any_pool_failure);
    RUN_TEST(test_health_flags_queue_overflow_on_drop);
    RUN_TEST(test_health_check_after_stop_does_not_crash);
    RUN_TEST(test_stats_export_into_misaligned_buffer);
}
