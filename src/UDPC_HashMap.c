#include "UDPC_HashMap.h"

#include <stdlib.h>

UDPC_HashMap* UDPC_HashMap_init(uint32_t capacity, uint32_t unitSize)
{
    UDPC_HashMap *m = malloc(sizeof(UDPC_HashMap));
    if(!m)
    {
        return NULL;
    }

    int fail = 0;
    m->size = 0;
    m->capacity = (capacity > 9 ? capacity : 10);
    m->unitSize = unitSize;
    m->buckets = malloc(sizeof(UDPC_Deque) * m->capacity);
    if(!m->buckets)
    {
        free(m);
        return NULL;
    }

    for(int x = 0; x < m->capacity; ++x)
    {
        if(fail != 0)
        {
            (&m->buckets[x])->buf = NULL;
            continue;
        }

        UDPC_Deque_clear(m->buckets + x);
        (m->buckets + x)->alloc_size = 8 * (sizeof(uint32_t) + unitSize);
        (m->buckets + x)->buf = malloc(8 * (sizeof(uint32_t) + unitSize));
        if(!(m->buckets + x)->buf)
        {
            fail = 1;
        }
    }

    if(fail != 0)
    {
        for(int x = 0; x < m->capacity; ++x)
        {
            if((m->buckets + x)->buf)
            {
                free((m->buckets + x)->buf);
            }
        }
        free(m->buckets);
        free(m);
        return NULL;
    }

    return m;
}

void UDPC_HashMap_destroy(UDPC_HashMap *hashMap)
{
    for(int x = 0; x < hashMap->capacity; ++x)
    {
        while((hashMap->buckets + x)->size > 0)
        {
            UDPC_Deque_pop_back(hashMap->buckets + x, sizeof(uint32_t) + hashMap->unitSize);
        }
    }
    free(hashMap->buckets);
    free(hashMap);
}
