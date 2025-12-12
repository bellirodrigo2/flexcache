#ifndef FLEXCACHE_H
#define FLEXCACHE_H

#include <stddef.h>
#include <stdint.h>

#include "bcache.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file flexcache.h
 * @brief Single-thread cache layer built on top of bcache with TTL and pluggable eviction policies.
 *
 * flexcache provides:
 *  - TTL-based expiration
 *  - size-based eviction (items / bytes)
 *  - pluggable eviction policies (LRU, FIFO, RANDOM, extensible)
 *  - user-defined memory management callbacks
 *
 * The cache is strictly single-threaded.
 * Time is injected via a user-provided function (now_fn).
 */

/* ============================================================
 *  Time source
 * ============================================================ */

/**
 * @brief Time provider callback.
 *
 * Must return a monotonic-like timestamp in milliseconds.
 */
typedef uint64_t (*flexcache_now_fn)(void *user_ctx);


/* ============================================================
 *  User memory management callbacks
 * ============================================================ */

/**
 * @brief Copy callback.
 *
 * Must return an owned copy of the input buffer.
 */
typedef void *(*flexcache_copy_fn)(const void *ptr, size_t len, void *user_ctx);

/**
 * @brief Free callback.
 *
 * Frees memory previously returned by a copy callback.
 */
typedef void  (*flexcache_free_fn)(void *ptr, void *user_ctx);


/* ============================================================
 *  Eviction policy hooks
 * ============================================================ */

/**
 * @brief Touch callback.
 *
 * Called inside flexcache_get() after a successful (non-expired) hit.
 * Used by eviction policies such as LRU.
 */
typedef void (*flexcache_touch_fn)(
    bcache *base,
    bcache_node *node,
    void *user_ctx
);

/**
 * @brief Pop callback.
 *
 * Selects a victim node when eviction is required.
 */
typedef bcache_node *(*flexcache_pop_fn)(
    bcache *base,
    void *user_ctx
);


/* ============================================================
 *  Deletion hook
 * ============================================================ */

/**
 * @brief ondelete callback.
 *
 * Called exactly once for each removed item (TTL expiration or eviction).
 * Called before key/value memory is freed.
 */
typedef void (*flexcache_ondelete_fn)(
    void   *key,
    size_t  key_len,
    void   *value,
    int64_t byte_size,
    void   *user_ctx
);


/* ============================================================
 *  Internal entry (exposed for size calculation)
 * ============================================================ */

/**
 * @brief Internal value wrapper stored in bcache.
 */
typedef struct flexcache_entry {
    void    *user_value;      /**< User value */
    uint64_t expires_at_ms;   /**< Absolute expiration time (0 = never) */
} flexcache_entry;


/* ============================================================
 *  flexcache structure (exposed for stack allocation)
 * ============================================================ */

/**
 * @struct flexcache
 * @brief Cache container with TTL and eviction support.
 *
 * Can be allocated on stack or heap. Use flexcache_init() to initialize
 * or flexcache_create() for heap allocation.
 */
typedef struct flexcache {
    bcache base;              /**< Underlying intrusive cache */

    flexcache_now_fn now_fn;  /**< Time provider */

    flexcache_copy_fn key_copy;
    flexcache_free_fn key_free;
    flexcache_copy_fn value_copy;
    flexcache_free_fn value_free;

    flexcache_ondelete_fn ondelete;

    flexcache_touch_fn touch_fn;
    flexcache_pop_fn   pop_fn;

    size_t  item_max;
    int64_t byte_max;

    uint64_t scan_interval_ms;
    uint64_t last_scan_ms;

    void *user_ctx;
    void *policy_ctx;
} flexcache;


/* ============================================================
 *  Lifecycle
 * ============================================================ */

/**
 * @brief Initialize a flexcache instance (stack or pre-allocated).
 *
 * @param fc                Cache instance to initialize.
 * @param now_fn            Time provider callback.
 * @param item_max          Maximum number of items (0 = disabled).
 * @param byte_max          Maximum total bytes (0 = disabled).
 * @param scan_interval_ms  Minimum interval between automatic scans (0 = always).
 * @param key_copy          Key copy callback (NULL = store pointer).
 * @param key_free          Key free callback.
 * @param value_copy        Value copy callback (NULL = store pointer).
 * @param value_free        Value free callback.
 * @param ondelete          Deletion hook (optional).
 * @param user_ctx          Opaque pointer passed to callbacks.
 *
 * @return 0 on success, -1 on error.
 */
int flexcache_init(
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
);

/**
 * @brief Allocate and initialize a new flexcache instance.
 *
 * @param now_fn            Time provider callback.
 * @param item_max          Maximum number of items (0 = disabled).
 * @param byte_max          Maximum total bytes (0 = disabled).
 * @param scan_interval_ms  Minimum interval between automatic scans (0 = always).
 * @param key_copy          Key copy callback (NULL = store pointer).
 * @param key_free          Key free callback.
 * @param value_copy        Value copy callback (NULL = store pointer).
 * @param value_free        Value free callback.
 * @param ondelete          Deletion hook (optional).
 * @param user_ctx          Opaque pointer passed to callbacks.
 *
 * @return Pointer to new flexcache, or NULL on error.
 */
flexcache *flexcache_create(
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
);

/**
 * @brief Destroy all items in the cache.
 *
 * Does NOT free the flexcache structure itself.
 * Use flexcache_free() for heap-allocated caches.
 */
void flexcache_destroy(flexcache *fc);

/**
 * @brief Destroy all items and free the flexcache structure.
 *
 * Use only for caches created with flexcache_create().
 */
void flexcache_free(flexcache *fc);


/* ============================================================
 *  Operations
 * ============================================================ */

/**
 * @brief Insert an item into the cache.
 *
 * @param fc         Cache instance.
 * @param key        Pointer to key data.
 * @param key_len    Key size in bytes.
 * @param value      Pointer to value data.
 * @param value_len  Value size in bytes (for copy callback).
 * @param byte_size  Accounted size for eviction limits.
 * @param ttl_ms     Time-to-live in milliseconds (0 = no expiration).
 * @param expires_at_ms Absolute expiration timestamp (0 = no expiration, ignored if ttl_ms > 0).
 *
 * @return 0 on success, -1 on duplicate key, -2 on allocation error.
 */
int flexcache_insert(
    flexcache *fc,
    const void *key,
    size_t key_len,
    const void *value,
    size_t value_len,
    int64_t byte_size,
    uint64_t ttl_ms,
    uint64_t expires_at_ms
);

/**
 * @brief Retrieve a value from the cache.
 *
 * Calls the policy-specific touch callback on hit.
 *
 * @param fc       Cache instance.
 * @param key      Pointer to key data.
 * @param key_len  Key size in bytes.
 *
 * @return Pointer to value, or NULL if not found or expired.
 */
void *flexcache_get(flexcache *fc, const void *key, size_t key_len);

/**
 * @brief Remove an item by key.
 *
 * @param fc       Cache instance.
 * @param key      Pointer to key data.
 * @param key_len  Key size in bytes.
 *
 * @return 0 if removed, -1 if not found.
 */
int flexcache_delete(flexcache *fc, const void *key, size_t key_len);


/* ============================================================
 *  Maintenance
 * ============================================================ */

/**
 * @brief Run expiration scan and enforce size limits.
 */
void flexcache_scan_and_clean(flexcache *fc);

/**
 * @brief Conditionally run scan_and_clean() based on scan interval.
 */
void flexcache_maybe_scan_and_clean(flexcache *fc);


/* ============================================================
 *  Advanced configuration
 * ============================================================ */

/**
 * @brief Override eviction policy hooks.
 *
 * @param fc         Cache instance.
 * @param touch_fn   Touch callback (called on cache hit).
 * @param pop_fn     Pop callback (selects eviction victim).
 * @param policy_ctx Opaque pointer passed to policy callbacks.
 */
void flexcache_set_policy_hooks(
    flexcache *fc,
    flexcache_touch_fn touch_fn,
    flexcache_pop_fn pop_fn,
    void *policy_ctx
);


/* ============================================================
 *  Statistics
 * ============================================================ */

/**
 * @brief Get current item count.
 */
size_t flexcache_item_count(const flexcache *fc);

/**
 * @brief Get total accounted bytes.
 */
int64_t flexcache_total_bytes(const flexcache *fc);

/**
 * @brief Get access to underlying bcache (for iteration, etc).
 */
bcache *flexcache_get_bcache(flexcache *fc);

#ifdef __cplusplus
}
#endif

#endif /* FLEXCACHE_H */
