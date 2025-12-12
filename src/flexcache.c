#include "flexcache.h"

#include <stdlib.h>
#include <limits.h>


/* ============================================================
 *  Internal helpers
 * ============================================================ */

/**
 * @brief Delete a node and invoke all required callbacks.
 */
static void
delete_node(flexcache *fc, bcache_node *n)
{
    void *key;
    size_t key_len;
    int64_t byte_sz;
    flexcache_entry *e;
    void *user_value;

    if (!fc || !n)
        return;

    key      = n->key;
    key_len  = n->key_len;
    byte_sz  = n->byte_size;
    e        = (flexcache_entry *)n->value;
    user_value = e ? e->user_value : NULL;

    if (fc->ondelete) {
        fc->ondelete(key, key_len, user_value, byte_sz, fc->user_ctx);
    }

    bcache_remove_node(&fc->base, n);

    if (fc->key_free && key)
        fc->key_free(key, fc->user_ctx);

    if (fc->value_free && user_value)
        fc->value_free(user_value, fc->user_ctx);

    free(e);
}

/**
 * @brief Remove all expired items.
 *
 * Handles circular list iteration safely by checking if list becomes empty.
 */
static void
remove_expired(flexcache *fc, uint64_t now_ms)
{
    bcache_node *n;
    bcache_node *next;
    bcache_node *head;
    flexcache_entry *e;
    int is_last;

    head = fc->base.list;
    if (!head)
        return;

    n = head;
    do {
        /* Check if this is the last element (circular: next points to head) */
        is_last = (n->next == head) || (fc->base.item_count == 1);
        next = n->next;

        e = (flexcache_entry *)n->value;
        if (e && e->expires_at_ms && e->expires_at_ms <= now_ms) {
            delete_node(fc, n);
            /* After deletion, list head may have changed */
            head = fc->base.list;
            if (!head)
                return;
        }

        if (is_last)
            break;

        n = next;
    } while (n != head);
}

/**
 * @brief Enforce item/byte limits using eviction policy.
 */
static void
enforce_limits(flexcache *fc)
{
    int over_items;
    int over_bytes;
    bcache_node *victim;

    for (;;) {
        over_items = (fc->item_max != 0 &&
                      fc->base.item_count > fc->item_max);

        over_bytes = (fc->byte_max != 0 &&
                      fc->base.total_bytes > fc->byte_max);

        if (!over_items && !over_bytes)
            break;

        victim = fc->pop_fn ? fc->pop_fn(&fc->base, fc->policy_ctx) : NULL;
        if (!victim)
            break;

        delete_node(fc, victim);
    }
}

/**
 * @brief Safely compute expiration time with overflow protection.
 *
 * @param now_ms   Current timestamp.
 * @param ttl_ms   Time-to-live.
 *
 * @return Expiration timestamp, or UINT64_MAX on overflow.
 */
static inline uint64_t
safe_expiration(uint64_t now_ms, uint64_t ttl_ms)
{
    if (ttl_ms == 0)
        return 0;

    /* Check for overflow */
    if (now_ms > UINT64_MAX - ttl_ms)
        return UINT64_MAX;

    return now_ms + ttl_ms;
}


/* ============================================================
 *  Public API
 * ============================================================ */

int
flexcache_init(
    flexcache *fc,
    flexcache_now_fn now_fn,
    size_t item_max,
    int64_t byte_max,
    uint64_t scan_interval_ms,
    flexcache_copy_fn key_copy,
    flexcache_free_fn key_free,
    flexcache_copy_fn value_copy,
    flexcache_free_fn value_free,
    flexcache_ondelete_fn ondelete,
    void *user_ctx
)
{
    if (!fc || !now_fn)
        return -1;

    bcache_init(&fc->base);

    fc->now_fn = now_fn;

    fc->key_copy   = key_copy;
    fc->key_free   = key_free;
    fc->value_copy = value_copy;
    fc->value_free = value_free;

    fc->ondelete = ondelete;

    fc->item_max = item_max;
    fc->byte_max = byte_max;

    fc->scan_interval_ms = scan_interval_ms;
    fc->last_scan_ms     = 0;

    fc->user_ctx   = user_ctx;
    fc->policy_ctx = NULL;

    fc->touch_fn = NULL;
    fc->pop_fn   = NULL;

    return 0;
}

flexcache *
flexcache_create(
    flexcache_now_fn now_fn,
    size_t item_max,
    int64_t byte_max,
    uint64_t scan_interval_ms,
    flexcache_copy_fn key_copy,
    flexcache_free_fn key_free,
    flexcache_copy_fn value_copy,
    flexcache_free_fn value_free,
    flexcache_ondelete_fn ondelete,
    void *user_ctx
)
{
    flexcache *fc;
    int rc;

    fc = (flexcache *)malloc(sizeof(*fc));
    if (!fc)
        return NULL;

    rc = flexcache_init(
        fc, now_fn, item_max, byte_max, scan_interval_ms,
        key_copy, key_free, value_copy, value_free,
        ondelete, user_ctx
    );

    if (rc != 0) {
        free(fc);
        return NULL;
    }

    return fc;
}

void
flexcache_set_policy_hooks(
    flexcache *fc,
    flexcache_touch_fn touch_fn,
    flexcache_pop_fn pop_fn,
    void *policy_ctx
)
{
    if (!fc)
        return;

    fc->touch_fn   = touch_fn;
    fc->pop_fn     = pop_fn;
    fc->policy_ctx = policy_ctx;
}

void
flexcache_destroy(flexcache *fc)
{
    bcache_node *n;
    bcache_node *next;
    bcache_node *head;
    int is_last;

    if (!fc)
        return;

    head = fc->base.list;
    if (!head)
        return;

    n = head;
    do {
        is_last = (n->next == head) || (fc->base.item_count == 1);
        next = n->next;

        delete_node(fc, n);

        /* Update head after deletion */
        head = fc->base.list;
        if (!head)
            return;

        if (is_last)
            break;

        n = next;
    } while (n != head);
}

void
flexcache_free(flexcache *fc)
{
    if (!fc)
        return;

    flexcache_destroy(fc);
    free(fc);
}

int
flexcache_insert(
    flexcache *fc,
    const void *key,
    size_t key_len,
    const void *value,
    size_t value_len,
    int64_t byte_size,
    uint64_t ttl_ms
)
{
    uint64_t now_ms;
    void *k;
    void *v;
    flexcache_entry *e;
    bcache_node *n;
    int rc;

    if (!fc || !key || key_len == 0 || byte_size < 0)
        return -2;

    now_ms = fc->now_fn(fc->user_ctx);

    k = fc->key_copy ? fc->key_copy(key, key_len, fc->user_ctx)
                     : (void *)key;
    if (!k)
        return -2;

    v = fc->value_copy ? fc->value_copy(value, value_len, fc->user_ctx)
                       : (void *)value;
    if (fc->value_copy && !v) {
        if (fc->key_free)
            fc->key_free(k, fc->user_ctx);
        return -2;
    }

    e = (flexcache_entry *)malloc(sizeof(*e));
    if (!e) {
        if (fc->key_free)
            fc->key_free(k, fc->user_ctx);
        if (fc->value_free && v)
            fc->value_free(v, fc->user_ctx);
        return -2;
    }

    e->user_value    = v;
    e->expires_at_ms = safe_expiration(now_ms, ttl_ms);

    n = bcache_node_new(k, key_len, e, byte_size);
    if (!n) {
        free(e);
        if (fc->key_free)
            fc->key_free(k, fc->user_ctx);
        if (fc->value_free && v)
            fc->value_free(v, fc->user_ctx);
        return -2;
    }

    rc = bcache_insert(&fc->base, n);
    if (rc != 0) {
        /* bcache_insert failed (duplicate key) - clean up */
        uthash_free(n, sizeof(*n));
        free(e);
        if (fc->key_free)
            fc->key_free(k, fc->user_ctx);
        if (fc->value_free && v)
            fc->value_free(v, fc->user_ctx);
        return -1;
    }

    enforce_limits(fc);
    return 0;
}

void *
flexcache_get(flexcache *fc, const void *key, size_t key_len)
{
    bcache_node *n;
    flexcache_entry *e;
    uint64_t now_ms;

    if (!fc || !key || key_len == 0)
        return NULL;

    n = bcache_get(&fc->base, (void *)key, key_len);
    if (!n)
        return NULL;

    e = (flexcache_entry *)n->value;
    now_ms = fc->now_fn(fc->user_ctx);

    if (e && e->expires_at_ms && e->expires_at_ms <= now_ms) {
        delete_node(fc, n);
        return NULL;
    }

    if (fc->touch_fn)
        fc->touch_fn(&fc->base, n, fc->policy_ctx);

    return e ? e->user_value : NULL;
}

int
flexcache_delete(flexcache *fc, const void *key, size_t key_len)
{
    bcache_node *n;

    if (!fc || !key || key_len == 0)
        return -1;

    n = bcache_get(&fc->base, (void *)key, key_len);
    if (!n)
        return -1;

    delete_node(fc, n);
    return 0;
}

void
flexcache_scan_and_clean(flexcache *fc)
{
    uint64_t now_ms;

    if (!fc)
        return;

    now_ms = fc->now_fn(fc->user_ctx);
    remove_expired(fc, now_ms);
    enforce_limits(fc);
}

void
flexcache_maybe_scan_and_clean(flexcache *fc)
{
    uint64_t now_ms;

    if (!fc)
        return;

    now_ms = fc->now_fn(fc->user_ctx);

    if (fc->scan_interval_ms == 0 ||
        fc->last_scan_ms == 0 ||
        (now_ms - fc->last_scan_ms) >= fc->scan_interval_ms)
    {
        fc->last_scan_ms = now_ms;
        remove_expired(fc, now_ms);
        enforce_limits(fc);
    }
}

size_t
flexcache_item_count(const flexcache *fc)
{
    return fc ? fc->base.item_count : 0;
}

int64_t
flexcache_total_bytes(const flexcache *fc)
{
    return fc ? fc->base.total_bytes : 0;
}

bcache *
flexcache_get_bcache(flexcache *fc)
{
    return fc ? &fc->base : NULL;
}
