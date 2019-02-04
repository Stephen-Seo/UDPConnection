#ifndef UDPC_HASHMAP_H
#define UDPC_HASHMAP_H

#include "UDPC_Deque.h"

typedef struct {
    uint32_t size;
    uint32_t capacity;
    uint32_t unitSize;
    UDPC_Deque *buckets;
} UDPC_HashMap;

UDPC_HashMap* UDPC_HashMap_init(uint32_t capacity, uint32_t unitSize);

void UDPC_HashMap_destroy(UDPC_HashMap *hashMap);

#endif
