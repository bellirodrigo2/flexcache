#ifndef FLEXCACHE_POLICY_FIFO_H
#define FLEXCACHE_POLICY_FIFO_H

#include "flexcache.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file flexcache_policy_fifo.h
 * @brief FIFO (First In, First Out) eviction policy for flexcache.
 */

/**
 * @brief Initialize FIFO eviction policy.
 *
 * Injects FIFO-specific touch and pop callbacks into the cache.
 */
void flexcache_policy_fifo_init(flexcache *fc);

#ifdef __cplusplus
}
#endif

#endif /* FLEXCACHE_POLICY_FIFO_H */
