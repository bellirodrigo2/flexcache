#include "test_helpers.h"
#include "../flexcache.h"

/* ============================================================
 *  Test: Init with valid params
 * ============================================================ */

static int
test_flexcache_init(void)
{
    flexcache fc;
    int rc;

    mock_time_set(1000);

    rc = flexcache_init(
        &fc,
        mock_now_fn,
        100,        /* item_max */
        10000,      /* byte_max */
        5000,       /* scan_interval_ms */
        NULL, NULL, /* key copy/free */
        NULL, NULL, /* value copy/free */
        NULL,       /* ondelete */
        NULL        /* user_ctx */
    );

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 0);
    ASSERT_EQ(flexcache_total_bytes(&fc), 0);

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Init fails with NULL now_fn
 * ============================================================ */

static int
test_flexcache_init_null_now(void)
{
    flexcache fc;
    int rc;

    rc = flexcache_init(
        &fc,
        NULL,       /* now_fn = NULL should fail */
        100, 10000, 5000,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL
    );

    ASSERT_EQ(rc, -1);
    return 0;
}

/* ============================================================
 *  Test: Create and free (heap allocation)
 * ============================================================ */

static int
test_flexcache_create_free(void)
{
    flexcache *fc;

    mock_time_set(1000);

    fc = flexcache_create(
        mock_now_fn,
        100, 10000, 5000,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL
    );

    ASSERT_NOT_NULL(fc);
    ASSERT_EQ(flexcache_item_count(fc), 0);

    flexcache_free(fc);
    return 0;
}

/* ============================================================
 *  Test: Insert and get
 * ============================================================ */

static int
test_flexcache_insert_get(void)
{
    flexcache fc;
    char key[] = "mykey";
    char val[] = "myvalue";
    void *result;
    int rc;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    rc = flexcache_insert(&fc, key, strlen(key), val, strlen(val), 100, 0, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 1);
    ASSERT_EQ(flexcache_total_bytes(&fc), 100);

    result = flexcache_get(&fc, key, strlen(key));
    ASSERT_EQ(result, val);  /* without copy, returns same pointer */

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Insert duplicate key
 * ============================================================ */

static int
test_flexcache_insert_duplicate(void)
{
    flexcache fc;
    char key[] = "dupkey";
    char val1[] = "value1";
    char val2[] = "value2";
    int rc;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    rc = flexcache_insert(&fc, key, strlen(key), val1, strlen(val1), 50, 0, 0);
    ASSERT_EQ(rc, 0);

    rc = flexcache_insert(&fc, key, strlen(key), val2, strlen(val2), 50, 0, 0);
    ASSERT_EQ(rc, -1);  /* duplicate */
    ASSERT_EQ(flexcache_item_count(&fc), 1);  /* still 1 */

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Insert with NULL key
 * ============================================================ */

static int
test_flexcache_insert_null_key(void)
{
    flexcache fc;
    char val[] = "value";
    int rc;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    rc = flexcache_insert(&fc, NULL, 0, val, strlen(val), 50, 0, 0);
    ASSERT_EQ(rc, -2);  /* allocation/validation error */

    rc = flexcache_insert(&fc, "key", 0, val, strlen(val), 50, 0, 0);
    ASSERT_EQ(rc, -2);  /* key_len = 0 */

    ASSERT_EQ(flexcache_item_count(&fc), 0);

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Delete
 * ============================================================ */

static int
test_flexcache_delete(void)
{
    flexcache fc;
    char key[] = "delkey";
    char val[] = "delval";
    int rc;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    flexcache_insert(&fc, key, strlen(key), val, strlen(val), 100, 0, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 1);

    rc = flexcache_delete(&fc, key, strlen(key));
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 0);
    ASSERT_NULL(flexcache_get(&fc, key, strlen(key)));

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Delete not found
 * ============================================================ */

static int
test_flexcache_delete_not_found(void)
{
    flexcache fc;
    int rc;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    rc = flexcache_delete(&fc, "noexist", 7);
    ASSERT_EQ(rc, -1);

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: ondelete callback
 * ============================================================ */

static int
test_flexcache_ondelete_called(void)
{
    flexcache fc;
    char key[] = "cbkey";
    char val[] = "cbval";

    mock_time_set(1000);
    ondelete_reset();

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL,
                   test_ondelete_fn, NULL);

    flexcache_insert(&fc, key, strlen(key), val, strlen(val), 100, 0, 0);

    flexcache_delete(&fc, key, strlen(key));

    ASSERT_EQ(g_ondelete_count, 1);
    ASSERT_EQ(g_ondelete_last_key, key);
    ASSERT_EQ(g_ondelete_last_value, val);

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Copy callbacks
 * ============================================================ */

static int
test_flexcache_copy_callbacks(void)
{
    flexcache fc;
    char key[] = "copykey";
    char val[] = "copyval";
    void *result;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   test_copy_fn, test_free_fn,
                   test_copy_fn, test_free_fn,
                   NULL, NULL);

    flexcache_insert(&fc, key, strlen(key), val, strlen(val), 100, 0, 0);

    result = flexcache_get(&fc, key, strlen(key));
    ASSERT_NOT_NULL(result);
    ASSERT(result != val);  /* should be a copy, not same pointer */
    ASSERT(memcmp(result, val, strlen(val)) == 0);  /* but same content */

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: No copy stores pointer directly
 * ============================================================ */

static int
test_flexcache_no_copy_stores_pointer(void)
{
    flexcache fc;
    char key[] = "ptrkey";
    char val[] = "ptrval";
    void *result;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL,  /* no copy */
                   NULL, NULL,
                   NULL, NULL);

    flexcache_insert(&fc, key, strlen(key), val, strlen(val), 100, 0, 0);

    result = flexcache_get(&fc, key, strlen(key));
    ASSERT_EQ(result, val);  /* same pointer */

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Counters
 * ============================================================ */

static int
test_flexcache_counters(void)
{
    flexcache fc;
    char k1[] = "k1";
    char k2[] = "k2";
    char k3[] = "k3";

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    ASSERT_EQ(flexcache_item_count(&fc), 0);
    ASSERT_EQ(flexcache_total_bytes(&fc), 0);

    flexcache_insert(&fc, k1, 2, "v1", 2, 100, 0, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 1);
    ASSERT_EQ(flexcache_total_bytes(&fc), 100);

    flexcache_insert(&fc, k2, 2, "v2", 2, 200, 0, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 2);
    ASSERT_EQ(flexcache_total_bytes(&fc), 300);

    flexcache_insert(&fc, k3, 2, "v3", 2, 150, 0, 0);
    ASSERT_EQ(flexcache_item_count(&fc), 3);
    ASSERT_EQ(flexcache_total_bytes(&fc), 450);

    flexcache_delete(&fc, k2, 2);
    ASSERT_EQ(flexcache_item_count(&fc), 2);
    ASSERT_EQ(flexcache_total_bytes(&fc), 250);

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Test: Destroy cleans all
 * ============================================================ */

static int
test_flexcache_destroy_cleans_all(void)
{
    flexcache fc;
    char k1[] = "d1";
    char k2[] = "d2";
    char k3[] = "d3";

    mock_time_set(1000);
    ondelete_reset();

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL,
                   test_ondelete_fn, NULL);

    flexcache_insert(&fc, k1, 2, "v1", 2, 100, 0, 0);
    flexcache_insert(&fc, k2, 2, "v2", 2, 100, 0, 0);
    flexcache_insert(&fc, k3, 2, "v3", 2, 100, 0, 0);

    ASSERT_EQ(flexcache_item_count(&fc), 3);

    flexcache_destroy(&fc);

    ASSERT_EQ(g_ondelete_count, 3);  /* called for each item */

    return 0;
}

/* ============================================================
 *  Test: Get bcache accessor
 * ============================================================ */

static int
test_flexcache_get_bcache(void)
{
    flexcache fc;
    bcache *b;

    mock_time_set(1000);

    flexcache_init(&fc, mock_now_fn, 0, 0, 0,
                   NULL, NULL, NULL, NULL, NULL, NULL);

    b = flexcache_get_bcache(&fc);
    ASSERT_NOT_NULL(b);
    ASSERT_EQ(b->item_count, 0);

    flexcache_insert(&fc, "k", 1, "v", 1, 50, 0, 0);
    ASSERT_EQ(b->item_count, 1);

    flexcache_destroy(&fc);
    return 0;
}

/* ============================================================
 *  Runner
 * ============================================================ */

int
run_flexcache_core_tests(void)
{
    int failed = 0;

    printf("\n[flexcache core tests]\n");

    RUN_TEST(test_flexcache_init);
    RUN_TEST(test_flexcache_init_null_now);
    RUN_TEST(test_flexcache_create_free);
    RUN_TEST(test_flexcache_insert_get);
    RUN_TEST(test_flexcache_insert_duplicate);
    RUN_TEST(test_flexcache_insert_null_key);
    RUN_TEST(test_flexcache_delete);
    RUN_TEST(test_flexcache_delete_not_found);
    RUN_TEST(test_flexcache_ondelete_called);
    RUN_TEST(test_flexcache_copy_callbacks);
    RUN_TEST(test_flexcache_no_copy_stores_pointer);
    RUN_TEST(test_flexcache_counters);
    RUN_TEST(test_flexcache_destroy_cleans_all);
    RUN_TEST(test_flexcache_get_bcache);

    return failed;
}