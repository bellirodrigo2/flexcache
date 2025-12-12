#include "flexcache_policy_fifo.h"

/* FIFO: no touch, evict oldest */

static void
fifo_touch(bcache *base, bcache_node *node, void *user_ctx)
{
    (void)base;
    (void)node;
    (void)user_ctx;
}

static bcache_node *
fifo_pop(bcache *base, void *user_ctx)
{
    (void)user_ctx;
    return base->list; /* oldest */
}

void
flexcache_policy_fifo_init(flexcache *fc)
{
    if (!fc)
        return;

    flexcache_set_policy_hooks(fc, fifo_touch, fifo_pop,NULL);
}
