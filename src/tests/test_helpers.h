#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 *  Test macros
 * ============================================================ */

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    } \
} while(0)

#define ASSERT_NULL(ptr) ASSERT((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)

#define RUN_TEST(fn) do { \
    printf("  %s ... ", #fn); \
    if (fn() == 0) { \
        printf("OK\n"); \
    } else { \
        failed++; \
    } \
} while(0)


/* ============================================================
 *  Mock time
 * ============================================================ */

static uint64_t g_mock_time_ms = 0;

__attribute__((unused))
static uint64_t
mock_now_fn(void *ctx)
{
    (void)ctx;
    return g_mock_time_ms;
}

__attribute__((unused))
static void
mock_time_set(uint64_t ms)
{
    g_mock_time_ms = ms;
}

__attribute__((unused))
static void
mock_time_advance(uint64_t ms)
{
    g_mock_time_ms += ms;
}


/* ============================================================
 *  Mock copy/free callbacks
 * ============================================================ */

__attribute__((unused))
static void *
test_copy_fn(const void *ptr, size_t len, void *ctx)
{
    void *copy;
    (void)ctx;

    if (!ptr || len == 0)
        return NULL;

    copy = malloc(len);
    if (copy)
        memcpy(copy, ptr, len);

    return copy;
}

__attribute__((unused))
static void
test_free_fn(void *ptr, void *ctx)
{
    (void)ctx;
    free(ptr);
}


/* ============================================================
 *  Mock ondelete callback
 * ============================================================ */

static int g_ondelete_count = 0;
static void *g_ondelete_last_key = NULL;
static void *g_ondelete_last_value = NULL;

__attribute__((unused))
static void
test_ondelete_fn(void *key, size_t key_len, void *value, int64_t byte_size, void *ctx)
{
    (void)key_len;
    (void)byte_size;
    (void)ctx;

    g_ondelete_count++;
    g_ondelete_last_key = key;
    g_ondelete_last_value = value;
}

__attribute__((unused))
static void
ondelete_reset(void)
{
    g_ondelete_count = 0;
    g_ondelete_last_key = NULL;
    g_ondelete_last_value = NULL;
}


/* ============================================================
 *  Mock RNG for random policy
 * ============================================================ */

static uint32_t g_mock_rng_value = 0;

__attribute__((unused))
static uint32_t
mock_rng_fn(void *ctx)
{
    (void)ctx;
    return g_mock_rng_value;
}

__attribute__((unused))
static void
mock_rng_set(uint32_t val)
{
    g_mock_rng_value = val;
}

#endif /* TEST_HELPERS_H */
