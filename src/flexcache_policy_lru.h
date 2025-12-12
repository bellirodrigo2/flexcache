#ifndef FLEXCACHE_POLICY_LRU_H
#define FLEXCACHE_POLICY_LRU_H

#include "flexcache.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file flexcache_policy_lru.h
 * @brief LRU (Least Recently Used) eviction policy for flexcache.
 */

/**
 * @brief Initialize LRU eviction policy.
 *
 * Injects LRU-specific touch and pop callbacks into the cache.
 */
void flexcache_policy_lru_init(flexcache *fc);

#ifdef __cplusplus
}
#endif

#endif /* FLEXCACHE_POLICY_LRU_H */
