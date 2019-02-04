#ifndef UDPC_HASHMAP_H
#define UDPC_HASHMAP_H

// 5 8 2 7 3 6 1
// 3 2 5 1 8 7 6
#define UDPC_HASH32(x) ( \
    ( \
      ((x & 0xF8000000) >> 5) | \
      ((x & 0x07F80000) >> 6) | \
      ((x & 0x00060000) << 10) | \
      ((x & 0x0001FC00) >> 4) | \
      ((x & 0x00000380) << 22) | \
      ((x & 0x0000007E) >> 1) | \
      ((x & 0x00000001) << 21) \
    ) ^ 0x96969696 \
)

#include "UDPC_Deque.h"

typedef struct {
    uint32_t size;
    uint32_t capacity;
    uint32_t unitSize;
    UDPC_Deque *buckets;
} UDPC_HashMap;

/*!
 * \brief Creates a HashMap structure
 * Note that UDPC_HashMap_destroy must be called on the returned ptr to free
 * resources to avoid a memory leak.
 * \return non-null if creating the HashMap was successful
 */
UDPC_HashMap* UDPC_HashMap_init(uint32_t capacity, uint32_t unitSize);

/*!
 * \brief Releases resources used by a HashMap structure
 */
void UDPC_HashMap_destroy(UDPC_HashMap *hashMap);

/*!
 * \brief Inserts a copy of data pointed to by given pointer
 * \return Internally managed pointer to inserted data
 */
void* UDPC_HashMap_insert(UDPC_HashMap *hm, uint32_t key, void *data);

int UDPC_HashMap_remove(UDPC_HashMap *hm, uint32_t key);

void* UDPC_HashMap_get(UDPC_HashMap *hm, uint32_t key);

#endif
