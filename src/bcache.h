#ifndef BCACHE_H
#define BCACHE_H

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include "lib/uthash.h"
#include "lib/utlist.h"

/**
 * @file bcache.h
 * @brief Intrusive hash table + doubly linked list with counters
 *
 * bcache combines:
 *  - uthash for O(1) lookup
 *  - utlist for ordered iteration / LRU-style policies
 *
 * Keys are generic (void*) and compared by raw bytes using an explicit size.
 * No automatic memory management is performed for keys or values.
 *
 * The cache tracks:
 *  - number of items
 *  - total byte size (user-defined)
 *
 * IMPORTANT: This is a circular doubly-linked list (utlist DL_*).
 * When iterating and removing nodes, special care must be taken
 * to handle the case where the list becomes empty or has only one element.
 */


/* ============================================================
 *  Intrusive node
 * ============================================================ */

/**
 * @struct bcache_node
 * @brief Intrusive node stored in hash table and linked list.
 */
typedef struct bcache_node {
    void   *key;        /**< Pointer to key data (not copied) */
    size_t  key_len;    /**< Key size in bytes */

    void   *value;      /**< Associated value (opaque) */

    int64_t byte_size;  /**< Size in bytes accounted for this node */

    UT_hash_handle hh;  /**< uthash internal handle */

    struct bcache_node *prev; /**< Previous node in list */
    struct bcache_node *next; /**< Next node in list */
} bcache_node;


/* ============================================================
 *  bcache container
 * ============================================================ */

/**
 * @struct bcache
 * @brief Cache container holding hash, list, and counters.
 */
typedef struct bcache {
    bcache_node *hashmap;   /**< Hash table head */
    bcache_node *list;      /**< List head */

    size_t  item_count;     /**< Number of items in cache */
    int64_t total_bytes;    /**< Total byte size of all items */
} bcache;


/* ============================================================
 *  Initialization
 * ============================================================ */

/**
 * @brief Initialize a bcache instance.
 *
 * @param c Pointer to bcache.
 */
static inline void
bcache_init(bcache *c)
{
    c->hashmap     = NULL;
    c->list        = NULL;
    c->item_count  = 0;
    c->total_bytes = 0;
}


/* ============================================================
 *  Node creation
 * ============================================================ */

/**
 * @brief Allocate and initialize a new bcache node.
 *
 * No copies of key or value are performed.
 *
 * @param key Pointer to key data.
 * @param key_len Size of the key in bytes.
 * @param value Pointer to value.
 * @param byte_size Size in bytes to account for this node (must be >= 0).
 *
 * @return Pointer to new node, or NULL on error.
 */
static inline bcache_node *
bcache_node_new(void *key, size_t key_len, void *value, int64_t byte_size)
{
    bcache_node *n;

    if (!key || key_len == 0 || byte_size < 0)
        return NULL;

    n = (bcache_node *)uthash_malloc(sizeof *n);
    if (!n)
        return NULL;

    n->key       = key;
    n->key_len   = key_len;
    n->value     = value;
    n->byte_size = byte_size;

    n->prev = n->next = NULL;
    return n;
}


/* ============================================================
 *  Insertion
 * ============================================================ */

/**
 * @brief Insert a node into the bcache.
 *
 * The node is added to:
 *  - the hash table
 *  - the end of the linked list
 *
 * Counters (item_count and total_bytes) are updated.
 *
 * @param c Pointer to bcache.
 * @param n Node to insert.
 *
 * @return 0 on success, -1 if key already exists or arguments are invalid.
 */
static inline int
bcache_insert(bcache *c, bcache_node *n)
{
    bcache_node *existing;

    if (!c || !n)
        return -1;

    existing = NULL;
    HASH_FIND(hh, c->hashmap, n->key, n->key_len, existing);
    if (existing)
        return -1;

    HASH_ADD_KEYPTR(hh, c->hashmap, n->key, n->key_len, n);
    DL_APPEND(c->list, n);

    c->item_count++;
    c->total_bytes += n->byte_size;

    return 0;
}


/* ============================================================
 *  Lookup
 * ============================================================ */

/**
 * @brief Lookup a node by key.
 *
 * @param c Pointer to bcache.
 * @param key Pointer to key data.
 * @param key_len Size of the key in bytes.
 *
 * @return Matching node or NULL.
 */
static inline bcache_node *
bcache_get(bcache *c, void *key, size_t key_len)
{
    bcache_node *n;

    if (!c || !key || key_len == 0)
        return NULL;

    n = NULL;
    HASH_FIND(hh, c->hashmap, key, key_len, n);
    return n;
}


/* ============================================================
 *  Removal
 * ============================================================ */

/**
 * @brief Remove a node from the bcache.
 *
 * Updates counters and frees the node.
 * Key and value memory are NOT freed.
 *
 * @param c Pointer to bcache.
 * @param n Node to remove.
 */
static inline void
bcache_remove_node(bcache *c, bcache_node *n)
{
    if (!c || !n)
        return;

    HASH_DEL(c->hashmap, n);
    DL_DELETE(c->list, n);

    c->item_count--;
    c->total_bytes -= n->byte_size;

    uthash_free(n, sizeof(*n));
}


/**
 * @brief Remove a node by key.
 *
 * @param c Pointer to bcache.
 * @param key Pointer to key data.
 * @param key_len Size of the key in bytes.
 *
 * @return 0 if removed, -1 if not found.
 */
static inline int
bcache_remove_key(bcache *c, void *key, size_t key_len)
{
    bcache_node *n = bcache_get(c, key, key_len);
    if (!n)
        return -1;

    bcache_remove_node(c, n);
    return 0;
}


/* ============================================================
 *  List-based removals
 * ============================================================ */

/**
 * @brief Remove the first node in the list.
 */
static inline void
bcache_pop_front(bcache *c)
{
    if (!c || !c->list)
        return;

    bcache_remove_node(c, c->list);
}


/**
 * @brief Remove the last node in the list.
 */
static inline void
bcache_pop_back(bcache *c)
{
    if (!c || !c->list)
        return;

    bcache_remove_node(c, c->list->prev);
}


/* ============================================================
 *  Reordering
 * ============================================================ */

/**
 * @brief Move a node to the front of the list.
 */
static inline void
bcache_move_front(bcache *c, bcache_node *n)
{
    if (!c || !n || c->list == n)
        return;

    DL_DELETE(c->list, n);
    DL_PREPEND(c->list, n);
}


/**
 * @brief Move a node to the back of the list.
 */
static inline void
bcache_move_back(bcache *c, bcache_node *n)
{
    if (!c || !n)
        return;

    DL_DELETE(c->list, n);
    DL_APPEND(c->list, n);
}


/* ============================================================
 *  Clear
 * ============================================================ */

/**
 * @brief Remove all nodes from the bcache.
 *
 * All nodes are freed.
 * Counters are reset to zero.
 */
static inline void
bcache_clear(bcache *c)
{
    bcache_node *cur, *tmp;

    if (!c)
        return;

    HASH_ITER(hh, c->hashmap, cur, tmp) {
        HASH_DEL(c->hashmap, cur);
        uthash_free(cur, sizeof(*cur));
    }

    c->hashmap     = NULL;
    c->list        = NULL;
    c->item_count  = 0;
    c->total_bytes = 0;
}

#endif /* BCACHE_H */
