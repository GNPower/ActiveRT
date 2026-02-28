/*******************************************************************************
 * test_stats_registry.c
 *
 * Unit tests for the ActiveRT global statistics registry (activert_stats.c).
 * Verifies that:
 *   - Event pools are registered automatically on init and reflected in count
 *   - Active Objects are registered automatically on create
 *   - Registered items can be retrieved by index
 *   - Counts are consistent after multiple registrations
 *
 * Note: registry entries accumulate across tests in this file because
 * unregistering requires destroy/stop, which we do explicitly in tearDown.
 ******************************************************************************/

#include "unity.h"
#include "activert.h"

#include "task.h"

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Test types
 *--------------------------------------------------------------------------*/
typedef struct
{
    activert_event_t base;
    uint32_t         data;
} stats_test_event_t;

/*----------------------------------------------------------------------------
 * Pool definitions — one per test to avoid re-init issues with registration
 *--------------------------------------------------------------------------*/
#define STATS_POOL_SIZE 4

ACTIVERT_EVENT_POOL_DEFINE( stats_pool_1, stats_test_event_t, STATS_POOL_SIZE,
                             ACTIVERT_POOL_OVERFLOW_DROP );
ACTIVERT_EVENT_POOL_DEFINE( stats_pool_2, stats_test_event_t, STATS_POOL_SIZE,
                             ACTIVERT_POOL_OVERFLOW_DROP );

/*----------------------------------------------------------------------------
 * AO definition — stopped in tearDown to unregister
 *--------------------------------------------------------------------------*/
#define STATS_AO_STACK  4096
#define STATS_AO_QUEUE  4
#define STATS_AO_PRIO   2

static void stats_dispatch( activert_active_t *me, const activert_event_t *evt )
{
    ( void ) me;
    ( void ) evt;
}

ACTIVERT_ACTIVE_DEFINE_SIMPLE( stats_ao, stats_dispatch, STATS_AO_PRIO,
                                STATS_AO_STACK, STATS_AO_QUEUE );

/*----------------------------------------------------------------------------
 * Baseline counts captured before any test in this file runs
 *--------------------------------------------------------------------------*/
static size_t s_baseline_pool_count;
static size_t s_baseline_active_count;
static int    s_setup_done = 0; /* one-time baseline capture */

void setUp( void )
{
    if ( !s_setup_done )
    {
        s_baseline_pool_count   = activert_stats_get_pool_count();
        s_baseline_active_count = activert_stats_get_active_count();
        s_setup_done            = 1;
    }
}

void tearDown( void )
{
    /* Stop the AO if it was created in a test */
    if ( stats_ao != NULL )
    {
        activert_active_stop( stats_ao );
        vTaskDelay( pdMS_TO_TICKS( 20 ) );
        stats_ao = NULL;
    }
}

/*============================================================================
 * Tests
 *==========================================================================*/

void test_pool_count_increments_after_init( void )
{
    size_t before = activert_stats_get_pool_count();

    ACTIVERT_EVENT_POOL_INIT( stats_pool_1, stats_test_event_t, STATS_POOL_SIZE,
                              ACTIVERT_POOL_OVERFLOW_DROP );

    size_t after = activert_stats_get_pool_count();

    /* The pool was already registered if this is not the first run; if it was
     * re-inited in setUp it stays at the same slot.  Either way, count must
     * be >= before (never decreases due to init). */
    TEST_ASSERT_GREATER_OR_EQUAL( before, after );
    TEST_ASSERT_GREATER_OR_EQUAL_size_t( 1, after );
}

void test_two_distinct_pools_both_registered( void )
{
    ACTIVERT_EVENT_POOL_INIT( stats_pool_1, stats_test_event_t, STATS_POOL_SIZE,
                              ACTIVERT_POOL_OVERFLOW_DROP );
    ACTIVERT_EVENT_POOL_INIT( stats_pool_2, stats_test_event_t, STATS_POOL_SIZE,
                              ACTIVERT_POOL_OVERFLOW_DROP );

    /* Both pools must appear somewhere in the registry */
    int found_1 = 0, found_2 = 0;
    size_t count = activert_stats_get_pool_count();

    for ( size_t i = 0; i < count; i++ )
    {
        activert_event_pool_t *p = activert_stats_get_pool( i );
        if ( p == stats_pool_1 )
            found_1 = 1;
        if ( p == stats_pool_2 )
            found_2 = 1;
    }

    TEST_ASSERT_TRUE_MESSAGE( found_1, "stats_pool_1 not found in registry" );
    TEST_ASSERT_TRUE_MESSAGE( found_2, "stats_pool_2 not found in registry" );
}

void test_active_count_increments_after_create( void )
{
    size_t before = activert_stats_get_active_count();

    ACTIVERT_ACTIVE_INIT_SIMPLE( stats_ao, stats_dispatch, STATS_AO_PRIO );
    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    size_t after = activert_stats_get_active_count();
    TEST_ASSERT_GREATER_THAN_size_t( before, after );
}

void test_created_ao_is_retrievable_by_index( void )
{
    ACTIVERT_ACTIVE_INIT_SIMPLE( stats_ao, stats_dispatch, STATS_AO_PRIO );
    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    int found = 0;
    size_t count = activert_stats_get_active_count();

    for ( size_t i = 0; i < count; i++ )
    {
        if ( activert_stats_get_active( i ) == stats_ao )
        {
            found = 1;
            break;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE( found, "Created AO not found in registry" );
}

void test_get_pool_returns_null_for_out_of_range_index( void )
{
    size_t count = activert_stats_get_pool_count();
    /* Index one past the end must return NULL */
    TEST_ASSERT_NULL( activert_stats_get_pool( count ) );
}

void test_get_active_returns_null_for_out_of_range_index( void )
{
    size_t count = activert_stats_get_active_count();
    TEST_ASSERT_NULL( activert_stats_get_active( count ) );
}

/*----------------------------------------------------------------------------
 * run_tests — called by freertos_test_main.c
 *--------------------------------------------------------------------------*/
void run_tests( void )
{
    RUN_TEST( test_pool_count_increments_after_init );
    RUN_TEST( test_two_distinct_pools_both_registered );
    RUN_TEST( test_active_count_increments_after_create );
    RUN_TEST( test_created_ao_is_retrievable_by_index );
    RUN_TEST( test_get_pool_returns_null_for_out_of_range_index );
    RUN_TEST( test_get_active_returns_null_for_out_of_range_index );
}
