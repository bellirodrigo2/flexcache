#include "flexcache_policy_lru.h"

/* ============================================================
 *  LRU policy implementation
 * ============================================================ */

/**
 * @brief LRU touch: move accessed node to the back of the list.
 */
static void
lru_touch(bcache *base, bcache_node *node, void *user_ctx)
{
    (void)user_ctx;
    bcache_move_back(base, node);
}

/**
 * @brief LRU pop: evict least recently used node.
 *
 * Assumes:
 *  - most recently used items are at the back
 *  - least recently used items are at the front
 */
static bcache_node *
lru_pop(bcache *base, void *user_ctx)
{
    (void)user_ctx;
    return base->list; /* head = LRU */
}

/**
 * @brief Initialize LRU policy.
 */
void
flexcache_policy_lru_init(flexcache *fc)
{
    if (!fc)
        return;

    flexcache_set_policy_hooks(fc, lru_touch, lru_pop,NULL);
}
