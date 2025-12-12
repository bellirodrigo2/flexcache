#include "flexcache_policy_random.h"

#include <stdlib.h>

struct flexcache_random_policy {
    flexcache_rng_fn rng_fn;
    void *rng_ctx;
};

static void
random_touch(bcache *base, bcache_node *node, void *ctx)
{
    (void)base;
    (void)node;
    (void)ctx;
}

static bcache_node *
random_pop(bcache *base, void *ctx)
{
    struct flexcache_random_policy *p;
    size_t idx;
    bcache_node *n;

    if (!base || !ctx || base->item_count == 0)
        return NULL;

    p = (struct flexcache_random_policy *)ctx;

    idx = (size_t)(p->rng_fn(p->rng_ctx) % base->item_count);
    n = base->list;

    while (idx > 0 && n) {
        n = n->next;
        idx--;
    }
    return n;
}

flexcache_random_policy *
flexcache_policy_random_create(
    flexcache_rng_fn rng_fn,
    void *rng_ctx
)
{
    struct flexcache_random_policy *p;

    if (!rng_fn)
        return NULL;

    p = (struct flexcache_random_policy *)
        malloc(sizeof *p);
    if (!p) return NULL;

    p->rng_fn = rng_fn;
    p->rng_ctx = rng_ctx;
    return p;
}

void
flexcache_policy_random_destroy(
    flexcache_random_policy *policy
)
{
    if (policy)
        free(policy);
}

void
flexcache_policy_random_init(
    flexcache *fc,
    flexcache_random_policy *policy
)
{
    if (!fc || !policy)
        return;

    flexcache_set_policy_hooks(
        fc,
        random_touch,
        random_pop,
        policy
    );
}
