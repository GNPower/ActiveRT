/*******************************************************************************
 * test_event_pool.c
 *
 * Unit tests for the ActiveRT event pool (activert_event.c).
 * Exercises static pool creation, alloc/free, exhaustion behaviour,
 * and statistics tracking.
 *
 * Runs inside a FreeRTOS task via freertos_test_main.c so that FreeRTOS
 * APIs (mutexes, etc.) are fully operational.
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include <stdint.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Test event type
 *--------------------------------------------------------------------------*/
typedef struct
{
    activert_event_t base; /* Must be first member */
    uint32_t data;
} test_pool_event_t;

/*----------------------------------------------------------------------------
 * Static pool storage — defined once, re-initialized in setUp()
 *--------------------------------------------------------------------------*/
#define POOL_SIZE 8

ACTIVERT_EVENT_POOL_DEFINE(test_pool, test_pool_event_t, POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP);

/* Track events allocated by each test so tearDown() can free stragglers */
static activert_event_t* s_held_events[POOL_SIZE];
static int s_held_count;

/*----------------------------------------------------------------------------
 * Unity setUp / tearDown
 *--------------------------------------------------------------------------*/
void setUp(void)
{
    /* Reinitialise the pool — clears bitmap, resets stats, recreates mutex */
    ACTIVERT_EVENT_POOL_INIT(test_pool, test_pool_event_t, POOL_SIZE, ACTIVERT_POOL_OVERFLOW_DROP);
    s_held_count = 0;
    memset(s_held_events, 0, sizeof(s_held_events));
}

void tearDown(void)
{
    /* Free any events the test left allocated so the pool is clean */
    for (int i = 0; i < s_held_count; i++)
    {
        if (s_held_events[i] != NULL)
        {
            activert_event_pool_free(s_held_events[i]);
            s_held_events[i] = NULL;
        }
    }
    activert_event_pool_reset_stats(test_pool);
}

/*----------------------------------------------------------------------------
 * Helper: allocate and track an event
 *--------------------------------------------------------------------------*/
static activert_event_t* alloc_tracked(void)
{
    activert_event_t* e = activert_event_pool_alloc(test_pool);
    if (e && s_held_count < POOL_SIZE)
    {
        s_held_events[s_held_count++] = e;
    }
    return e;
}

static void free_tracked(activert_event_t* e)
{
    for (int i = 0; i < s_held_count; i++)
    {
        if (s_held_events[i] == e)
        {
            s_held_events[i] = NULL;
            break;
        }
    }
    activert_event_pool_free(e);
}

/*============================================================================
 * Tests
 *==========================================================================*/

void test_pool_init_all_slots_free(void)
{
    TEST_ASSERT_EQUAL_size_t(POOL_SIZE, activert_event_pool_get_free_count(test_pool));
}

void test_alloc_returns_non_null(void)
{
    activert_event_t* e = alloc_tracked();
    TEST_ASSERT_NOT_NULL(e);
}

void test_alloc_decrements_free_count(void)
{
    alloc_tracked();
    TEST_ASSERT_EQUAL_size_t(POOL_SIZE - 1, activert_event_pool_get_free_count(test_pool));
}

void test_free_restores_free_count(void)
{
    activert_event_t* e = alloc_tracked();
    TEST_ASSERT_EQUAL_size_t(POOL_SIZE - 1, activert_event_pool_get_free_count(test_pool));

    free_tracked(e);
    TEST_ASSERT_EQUAL_size_t(POOL_SIZE, activert_event_pool_get_free_count(test_pool));
}

void test_exhaust_pool_then_next_alloc_returns_null(void)
{
    /* Allocate all slots */
    for (int i = 0; i < POOL_SIZE; i++)
    {
        activert_event_t* e = alloc_tracked();
        TEST_ASSERT_NOT_NULL_MESSAGE(e, "Expected valid pointer before pool exhausted");
    }

    TEST_ASSERT_EQUAL_size_t(0, activert_event_pool_get_free_count(test_pool));

    /* Next alloc must fail (DROP policy) */
    activert_event_t* overflow = activert_event_pool_alloc(test_pool);
    TEST_ASSERT_NULL_MESSAGE(overflow, "Expected NULL when pool exhausted");
}

void test_peak_usage_tracks_high_water_mark(void)
{
    activert_event_pool_reset_stats(test_pool);

    activert_event_t* e1 = alloc_tracked();
    activert_event_t* e2 = alloc_tracked();
    activert_event_t* e3 = alloc_tracked();

    TEST_ASSERT_EQUAL_size_t(3, activert_event_pool_get_peak_usage(test_pool));

    /* Free all three — peak must remain at 3 */
    free_tracked(e1);
    free_tracked(e2);
    free_tracked(e3);

    TEST_ASSERT_EQUAL_size_t(3, activert_event_pool_get_peak_usage(test_pool));
}

void test_alloc_failure_count_increments(void)
{
    activert_event_pool_reset_stats(test_pool);

    /* Fill pool completely */
    for (int i = 0; i < POOL_SIZE; i++)
    {
        alloc_tracked();
    }

    /* Generate 3 failures */
    activert_event_pool_alloc(test_pool);
    activert_event_pool_alloc(test_pool);
    activert_event_pool_alloc(test_pool);

    TEST_ASSERT_EQUAL_UINT32(3, activert_event_pool_get_alloc_failures(test_pool));
}

void test_alloc_attempts_counts_all_including_failures(void)
{
    activert_event_pool_reset_stats(test_pool);

    /* 3 successful allocs */
    activert_event_t* e1 = alloc_tracked();
    activert_event_t* e2 = alloc_tracked();
    activert_event_t* e3 = alloc_tracked();

    /* 2 failures after full fill */
    for (int i = 3; i < POOL_SIZE; i++)
        alloc_tracked();
    activert_event_pool_alloc(test_pool);
    activert_event_pool_alloc(test_pool);

    /* Total attempts = POOL_SIZE successful + 2 failures */
    uint32_t attempts = activert_event_pool_get_alloc_attempts(test_pool);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(POOL_SIZE + 2), attempts);

    (void)e1;
    (void)e2;
    (void)e3;
}

void test_event_pool_pointer_set_correctly(void)
{
    activert_event_t* e = alloc_tracked();
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_PTR(test_pool, e->pool);
}

void test_reset_stats_clears_peak_and_failures(void)
{
    /* Generate some stats */
    for (int i = 0; i < POOL_SIZE; i++)
        alloc_tracked();
    activert_event_pool_alloc(test_pool); /* failure */

    /* Free all events before reset — peak cannot go below current_allocated,
     * so stats can only fully clear once there are no outstanding events. */
    for (int i = 0; i < s_held_count; i++)
    {
        if (s_held_events[i] != NULL)
        {
            activert_event_pool_free(s_held_events[i]);
            s_held_events[i] = NULL;
        }
    }
    s_held_count = 0;

    activert_event_pool_reset_stats(test_pool);

    TEST_ASSERT_EQUAL_size_t(0, activert_event_pool_get_peak_usage(test_pool));
    TEST_ASSERT_EQUAL_UINT32(0, activert_event_pool_get_alloc_failures(test_pool));
    TEST_ASSERT_EQUAL_UINT32(0, activert_event_pool_get_alloc_attempts(test_pool));
}

/*----------------------------------------------------------------------------
 * run_tests — called by freertos_test_main.c
 *--------------------------------------------------------------------------*/
void run_tests(void)
{
    RUN_TEST(test_pool_init_all_slots_free);
    RUN_TEST(test_alloc_returns_non_null);
    RUN_TEST(test_alloc_decrements_free_count);
    RUN_TEST(test_free_restores_free_count);
    RUN_TEST(test_exhaust_pool_then_next_alloc_returns_null);
    RUN_TEST(test_peak_usage_tracks_high_water_mark);
    RUN_TEST(test_alloc_failure_count_increments);
    RUN_TEST(test_alloc_attempts_counts_all_including_failures);
    RUN_TEST(test_event_pool_pointer_set_correctly);
    RUN_TEST(test_reset_stats_clears_peak_and_failures);
}
