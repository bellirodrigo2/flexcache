#include "test_helpers.h"
#include "../bcache.h"

/* ============================================================
 *  Test: Initialization
 * ============================================================ */

static int
test_bcache_init(void)
{
    bcache c;

    bcache_init(&c);

    ASSERT_NULL(c.hashmap);
    ASSERT_NULL(c.list);
    ASSERT_EQ(c.item_count, 0);
    ASSERT_EQ(c.total_bytes, 0);

    return 0;
}

/* ============================================================
 *  Test: Insert single
 * ============================================================ */

static int
test_bcache_insert_single(void)
{
    bcache c;
    bcache_node *n;
    char key[] = "key1";
    char val[] = "value1";
    int rc;

    bcache_init(&c);

    n = bcache_node_new(key, strlen(key), val, 100);
    ASSERT_NOT_NULL(n);

    rc = bcache_insert(&c, n);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(c.item_count, 1);
    ASSERT_EQ(c.total_bytes, 100);
    ASSERT_NOT_NULL(c.list);
    ASSERT_NOT_NULL(c.hashmap);

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Insert duplicate
 * ============================================================ */

static int
test_bcache_insert_duplicate(void)
{
    bcache c;
    bcache_node *n1, *n2;
    char key[] = "same_key";
    int rc;

    bcache_init(&c);

    n1 = bcache_node_new(key, strlen(key), NULL, 50);
    n2 = bcache_node_new(key, strlen(key), NULL, 50);

    rc = bcache_insert(&c, n1);
    ASSERT_EQ(rc, 0);

    rc = bcache_insert(&c, n2);
    ASSERT_EQ(rc, -1);  /* duplicate */
    ASSERT_EQ(c.item_count, 1);  /* still 1 */

    /* cleanup */
    uthash_free(n2, sizeof(*n2));
    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Get found
 * ============================================================ */

static int
test_bcache_get_found(void)
{
    bcache c;
    bcache_node *n, *found;
    char key[] = "findme";
    char val[] = "gotcha";

    bcache_init(&c);

    n = bcache_node_new(key, strlen(key), val, 10);
    bcache_insert(&c, n);

    found = bcache_get(&c, key, strlen(key));
    ASSERT_NOT_NULL(found);
    ASSERT_EQ(found, n);
    ASSERT_EQ(found->value, val);

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Get not found
 * ============================================================ */

static int
test_bcache_get_not_found(void)
{
    bcache c;
    bcache_node *n, *found;
    char key[] = "exists";
    char other[] = "nope";

    bcache_init(&c);

    n = bcache_node_new(key, strlen(key), NULL, 10);
    bcache_insert(&c, n);

    found = bcache_get(&c, other, strlen(other));
    ASSERT_NULL(found);

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Remove node
 * ============================================================ */

static int
test_bcache_remove_node(void)
{
    bcache c;
    bcache_node *n, *found;
    char key[] = "removeme";

    bcache_init(&c);

    n = bcache_node_new(key, strlen(key), NULL, 200);
    bcache_insert(&c, n);
    ASSERT_EQ(c.item_count, 1);
    ASSERT_EQ(c.total_bytes, 200);

    bcache_remove_node(&c, n);
    ASSERT_EQ(c.item_count, 0);
    ASSERT_EQ(c.total_bytes, 0);

    found = bcache_get(&c, key, strlen(key));
    ASSERT_NULL(found);

    return 0;
}

/* ============================================================
 *  Test: Remove by key
 * ============================================================ */

static int
test_bcache_remove_key(void)
{
    bcache c;
    bcache_node *n;
    char key[] = "bykey";
    int rc;

    bcache_init(&c);

    n = bcache_node_new(key, strlen(key), NULL, 50);
    bcache_insert(&c, n);

    rc = bcache_remove_key(&c, key, strlen(key));
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(c.item_count, 0);

    rc = bcache_remove_key(&c, key, strlen(key));
    ASSERT_EQ(rc, -1);  /* not found */

    return 0;
}

/* ============================================================
 *  Test: Pop front
 * ============================================================ */

static int
test_bcache_pop_front(void)
{
    bcache c;
    bcache_node *n1, *n2, *n3;
    char k1[] = "first";
    char k2[] = "second";
    char k3[] = "third";

    bcache_init(&c);

    n1 = bcache_node_new(k1, strlen(k1), NULL, 10);
    n2 = bcache_node_new(k2, strlen(k2), NULL, 20);
    n3 = bcache_node_new(k3, strlen(k3), NULL, 30);

    bcache_insert(&c, n1);
    bcache_insert(&c, n2);
    bcache_insert(&c, n3);

    ASSERT_EQ(c.list, n1);  /* n1 is head */

    bcache_pop_front(&c);
    ASSERT_EQ(c.item_count, 2);
    ASSERT_EQ(c.list, n2);  /* n2 is new head */
    ASSERT_NULL(bcache_get(&c, k1, strlen(k1)));

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Pop back
 * ============================================================ */

static int
test_bcache_pop_back(void)
{
    bcache c;
    bcache_node *n1, *n2, *n3;
    char k1[] = "first";
    char k2[] = "second";
    char k3[] = "third";

    bcache_init(&c);

    n1 = bcache_node_new(k1, strlen(k1), NULL, 10);
    n2 = bcache_node_new(k2, strlen(k2), NULL, 20);
    n3 = bcache_node_new(k3, strlen(k3), NULL, 30);

    bcache_insert(&c, n1);
    bcache_insert(&c, n2);
    bcache_insert(&c, n3);

    bcache_pop_back(&c);
    ASSERT_EQ(c.item_count, 2);
    ASSERT_NULL(bcache_get(&c, k3, strlen(k3)));
    ASSERT_NOT_NULL(bcache_get(&c, k1, strlen(k1)));
    ASSERT_NOT_NULL(bcache_get(&c, k2, strlen(k2)));

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Move front
 * ============================================================ */

static int
test_bcache_move_front(void)
{
    bcache c;
    bcache_node *n1, *n2, *n3;
    char k1[] = "a";
    char k2[] = "b";
    char k3[] = "c";

    bcache_init(&c);

    n1 = bcache_node_new(k1, 1, NULL, 10);
    n2 = bcache_node_new(k2, 1, NULL, 10);
    n3 = bcache_node_new(k3, 1, NULL, 10);

    bcache_insert(&c, n1);
    bcache_insert(&c, n2);
    bcache_insert(&c, n3);

    ASSERT_EQ(c.list, n1);

    bcache_move_front(&c, n3);
    ASSERT_EQ(c.list, n3);  /* n3 is now head */

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Move back
 * ============================================================ */

static int
test_bcache_move_back(void)
{
    bcache c;
    bcache_node *n1, *n2, *n3;
    char k1[] = "a";
    char k2[] = "b";
    char k3[] = "c";

    bcache_init(&c);

    n1 = bcache_node_new(k1, 1, NULL, 10);
    n2 = bcache_node_new(k2, 1, NULL, 10);
    n3 = bcache_node_new(k3, 1, NULL, 10);

    bcache_insert(&c, n1);
    bcache_insert(&c, n2);
    bcache_insert(&c, n3);

    /* DL_ list: head->prev always points to tail */
    ASSERT_EQ(c.list->prev, n3);

    bcache_move_back(&c, n1);
    ASSERT_EQ(c.list->prev, n1);  /* n1 is now tail */
    ASSERT_EQ(c.list, n2);        /* n2 is now head */

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Clear
 * ============================================================ */

static int
test_bcache_clear(void)
{
    bcache c;
    bcache_node *n1, *n2;
    char k1[] = "x";
    char k2[] = "y";

    bcache_init(&c);

    n1 = bcache_node_new(k1, 1, NULL, 100);
    n2 = bcache_node_new(k2, 1, NULL, 200);

    bcache_insert(&c, n1);
    bcache_insert(&c, n2);

    ASSERT_EQ(c.item_count, 2);
    ASSERT_EQ(c.total_bytes, 300);

    bcache_clear(&c);

    ASSERT_NULL(c.hashmap);
    ASSERT_NULL(c.list);
    ASSERT_EQ(c.item_count, 0);
    ASSERT_EQ(c.total_bytes, 0);

    return 0;
}

/* ============================================================
 *  Test: Order preserved
 * ============================================================ */

static int
test_bcache_order_preserved(void)
{
    bcache c;
    bcache_node *n1, *n2, *n3, *cur;
    char k1[] = "1";
    char k2[] = "2";
    char k3[] = "3";

    bcache_init(&c);

    n1 = bcache_node_new(k1, 1, NULL, 10);
    n2 = bcache_node_new(k2, 1, NULL, 10);
    n3 = bcache_node_new(k3, 1, NULL, 10);

    bcache_insert(&c, n1);
    bcache_insert(&c, n2);
    bcache_insert(&c, n3);

    /* verify order: n1 -> n2 -> n3 */
    cur = c.list;
    ASSERT_EQ(cur, n1);
    cur = cur->next;
    ASSERT_EQ(cur, n2);
    cur = cur->next;
    ASSERT_EQ(cur, n3);

    bcache_clear(&c);
    return 0;
}

/* ============================================================
 *  Test: Single item operations
 * ============================================================ */

static int
test_bcache_single_item(void)
{
    bcache c;
    bcache_node *n;
    char key[] = "only";

    bcache_init(&c);

    n = bcache_node_new(key, strlen(key), NULL, 50);
    bcache_insert(&c, n);

    ASSERT_EQ(c.list, n);
    /* DL_ list: head->prev always points to self when single item */
    ASSERT_EQ(n->prev, n);
    /* next can be NULL (official utlist) or self (circular variant) */

    /* pop should not crash */
    bcache_pop_front(&c);
    ASSERT_EQ(c.item_count, 0);
    ASSERT_NULL(c.list);

    return 0;
}

/* ============================================================
 *  Runner
 * ============================================================ */

int
run_bcache_tests(void)
{
    int failed = 0;

    printf("\n[bcache tests]\n");

    RUN_TEST(test_bcache_init);
    RUN_TEST(test_bcache_insert_single);
    RUN_TEST(test_bcache_insert_duplicate);
    RUN_TEST(test_bcache_get_found);
    RUN_TEST(test_bcache_get_not_found);
    RUN_TEST(test_bcache_remove_node);
    RUN_TEST(test_bcache_remove_key);
    RUN_TEST(test_bcache_pop_front);
    RUN_TEST(test_bcache_pop_back);
    RUN_TEST(test_bcache_move_front);
    RUN_TEST(test_bcache_move_back);
    RUN_TEST(test_bcache_clear);
    RUN_TEST(test_bcache_order_preserved);
    RUN_TEST(test_bcache_single_item);

    return failed;
}