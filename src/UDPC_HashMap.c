#include "UDPC_HashMap.h"

#include <stdlib.h>
#include <string.h>

UDPC_HashMap* UDPC_HashMap_init(uint32_t capacity, uint32_t unitSize)
{
    UDPC_HashMap *m = malloc(sizeof(UDPC_HashMap));
    if(!m)
    {
        return NULL;
    }

    int fail = 0;
    m->size = 0;
    m->capacity = (capacity > UDPC_HASHMAP_INIT_CAPACITY ? capacity : UDPC_HASHMAP_INIT_CAPACITY);
    m->unitSize = unitSize;

    m->buckets = malloc(sizeof(UDPC_Deque*) * m->capacity);
    if(!m->buckets)
    {
        free(m);
        return NULL;
    }

    for(int x = 0; x < m->capacity; ++x)
    {
        if(fail != 0)
        {
            m->buckets[x] = NULL;
            continue;
        }
        m->buckets[x] = UDPC_Deque_init(UDPC_HASHMAP_BUCKET_SIZE * (4 + unitSize));
        if(!m->buckets[x])
        {
            fail = 1;
        }
    }

    if(fail != 0)
    {
        for(int x = 0; x < m->capacity; ++x)
        {
            if(m->buckets[x])
            {
                UDPC_Deque_destroy(m->buckets[x]);
            }
        }
        free(m->buckets);
        free(m);
        return NULL;
    }

    m->overflow = UDPC_Deque_init(UDPC_HASHMAP_BUCKET_SIZE * (4 + unitSize));
    if(!m->overflow)
    {
        for(int x = 0; x < m->capacity; ++x)
        {
            if(m->buckets[x])
            {
                UDPC_Deque_destroy(m->buckets[x]);
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
        UDPC_Deque_destroy(hashMap->buckets[x]);
    }
    free(hashMap->buckets);
    UDPC_Deque_destroy(hashMap->overflow);
    free(hashMap);
}

void* UDPC_HashMap_insert(UDPC_HashMap *hm, uint32_t key, void *data)
{
    if(hm->capacity <= hm->size)
    {
        if(UDPC_HashMap_realloc(hm, hm->capacity * 2) == 0)
        {
            return NULL;
        }
    }

    uint32_t hash = UDPC_HASH32(key) % hm->capacity;

    char *temp = malloc(4 + hm->unitSize);
    memcpy(temp, &key, 4);
    if(hm->unitSize > 0)
    {
        memcpy(temp + 4, data, hm->unitSize);
    }

    if(UDPC_Deque_get_available(hm->buckets[hash]) == 0)
    {
        if(UDPC_Deque_push_back(hm->overflow, temp, 4 + hm->unitSize) == 0)
        {
            free(temp);
            return NULL;
        }
    }
    else if(UDPC_Deque_push_back(hm->buckets[hash], temp, 4 + hm->unitSize) == 0)
    {
        free(temp);
        return NULL;
    }

    free(temp);
    ++hm->size;
    temp = UDPC_Deque_get_back_ptr(hm->buckets[hash], 4 + hm->unitSize);
    return temp + 4;
}

int UDPC_HashMap_remove(UDPC_HashMap *hm, uint32_t key)
{
    if(hm->size == 0)
    {
        return 0;
    }

    uint32_t hash = UDPC_HASH32(key) % hm->capacity;

    for(int x = 0; x * (4 + hm->unitSize) < hm->buckets[hash]->size; ++x)
    {
        if(memcmp(
            UDPC_Deque_index_ptr(hm->buckets[hash], 4 + hm->unitSize, x),
            &key,
            4) == 0)
        {
            int result = UDPC_Deque_remove(hm->buckets[hash], 4 + hm->unitSize, x);
            if(result != 0)
            {
                --hm->size;
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }

    for(int x = 0; x * (4 + hm->unitSize) < hm->overflow->size; ++x)
    {
        if(memcmp(
            UDPC_Deque_index_ptr(hm->overflow, 4 + hm->unitSize, x),
            &key,
            4) == 0)
        {
            int result = UDPC_Deque_remove(hm->overflow, 4 + hm->unitSize, x);
            if(result != 0)
            {
                --hm->size;
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }

    return 0;
}

void* UDPC_HashMap_get(UDPC_HashMap *hm, uint32_t key)
{
    if(hm->size == 0)
    {
        return NULL;
    }

    uint32_t hash = UDPC_HASH32(key) % hm->capacity;

    for(int x = 0; x * (4 + hm->unitSize) < hm->buckets[hash]->size; ++x)
    {
        char *ptr = UDPC_Deque_index_ptr(hm->buckets[hash], 4 + hm->unitSize, x);
        if(memcmp(ptr, &key, 4) == 0)
        {
            if(hm->unitSize > 0)
            {
                return ptr + 4;
            }
            else
            {
                return ptr;
            }
        }
    }

    for(int x = 0; x * (4 + hm->unitSize) < hm->overflow->size; ++x)
    {
        char *ptr = UDPC_Deque_index_ptr(hm->overflow, 4 + hm->unitSize, x);
        if(memcmp(ptr, &key, 4) == 0)
        {
            if(hm->unitSize > 0)
            {
                return ptr + 4;
            }
            else
            {
                return ptr;
            }
        }
    }

    return NULL;
}

int UDPC_HashMap_realloc(UDPC_HashMap *hm, uint32_t newCapacity)
{
    if(hm->size > newCapacity)
    {
        return 0;
    }

    UDPC_Deque **newBuckets = malloc(sizeof(UDPC_Deque*) * newCapacity);
    UDPC_Deque *newOverflow = UDPC_Deque_init(UDPC_HASHMAP_BUCKET_SIZE
        * (4 + hm->unitSize));
    for(int x = 0; x < newCapacity; ++x)
    {
        newBuckets[x] = UDPC_Deque_init(UDPC_HASHMAP_BUCKET_SIZE
            * (4 + hm->unitSize));
    }

    uint32_t hash;
    char *data;
    int fail = 0;
    for(int x = 0; x < hm->capacity; ++x)
    {
        for(int y = 0; y * (4 + hm->unitSize) < hm->buckets[x]->size; ++y)
        {
            data = UDPC_Deque_index_ptr(hm->buckets[x], 4 + hm->unitSize, y);
            hash = UDPC_HASH32(*((uint32_t*)data)) % newCapacity;
            if(newBuckets[hash]->size < newBuckets[hash]->alloc_size)
            {
                if(UDPC_Deque_push_back(newBuckets[hash], data, 4 + hm->unitSize) == 0)
                {
                    fail = 1;
                    break;
                }
            }
            else if(UDPC_Deque_push_back(newOverflow, data, 4 + hm->unitSize) == 0)
            {
                fail = 1;
                break;
            }
        }
        if(fail != 0)
        {
            break;
        }
    }

    if(fail == 0)
    {
        for(int x = 0; x * (4 + hm->unitSize) < hm->overflow->size; ++x)
        {
            data = UDPC_Deque_index_ptr(hm->overflow, 4 + hm->unitSize, x);
            hash = UDPC_HASH32(*((uint32_t*)data)) % newCapacity;
            if(newBuckets[hash]->size < newBuckets[hash]->alloc_size)
            {
                if(UDPC_Deque_push_back(newBuckets[hash], data, 4 + hm->unitSize) == 0)
                {
                    fail = 1;
                    break;
                }
            }
            else if(UDPC_Deque_push_back(newOverflow, data, 4 + hm->unitSize) == 0)
            {
                fail = 1;
                break;
            }
        }
    }

    if(fail != 0)
    {
        for(int x = 0; x < newCapacity; ++x)
        {
            UDPC_Deque_destroy(newBuckets[x]);
        }
        free(newBuckets);
        UDPC_Deque_destroy(newOverflow);
        return 0;
    }
    else
    {
        for(int x = 0; x < hm->capacity; ++x)
        {
            UDPC_Deque_destroy(hm->buckets[x]);
        }
        free(hm->buckets);
        UDPC_Deque_destroy(hm->overflow);

        hm->buckets = newBuckets;
        hm->overflow = newOverflow;

        hm->capacity = newCapacity;
        return 1;
    }
}

void UDPC_HashMap_clear(UDPC_HashMap *hm)
{
    for(int x = 0; x < hm->capacity; ++x)
    {
        UDPC_Deque_clear(hm->buckets[x]);
    }
    UDPC_Deque_clear(hm->overflow);
    hm->size = 0;
}

uint32_t UDPC_HashMap_get_size(UDPC_HashMap *hm)
{
    return hm->size;
}

uint32_t UDPC_HashMap_get_capacity(UDPC_HashMap *hm)
{
    return hm->capacity;
}
