#ifndef FLEXCACHE_POLICY_RANDOM_H
#define FLEXCACHE_POLICY_RANDOM_H

#include "flexcache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flexcache_random_policy flexcache_random_policy;

typedef uint32_t (*flexcache_rng_fn)(void *rng_ctx);

flexcache_random_policy *
flexcache_policy_random_create(
    flexcache_rng_fn rng_fn,
    void *rng_ctx
);

void
flexcache_policy_random_destroy(
    flexcache_random_policy *policy
);

void
flexcache_policy_random_init(
    flexcache *fc,
    flexcache_random_policy *policy
);

#ifdef __cplusplus
}
#endif

#endif /* FLEXCACHE_POLICY_RANDOM_H */
