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
    m->capacity = (capacity > 10 ? capacity : 10);
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
        m->buckets[x] = UDPC_Deque_init(unitSize * UDPC_HASHMAP_BUCKET_SIZE);
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

    m->overflow = UDPC_Deque_init(unitSize * UDPC_HASHMAP_BUCKET_SIZE);
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
    free(hashMap->overflow);
    free(hashMap);
}

void* UDPC_HashMap_insert(UDPC_HashMap *hm, uint32_t key, void *data)
{
    uint32_t hash = UDPC_HASH32(key) % hm->capacity;

    char *temp = malloc(sizeof(uint32_t) + hm->unitSize);
    memcpy(temp, &key, sizeof(uint32_t));
    memcpy(temp + sizeof(uint32_t), data, hm->unitSize);

    if(UDPC_Deque_get_available(hm->buckets[hash]) == 0)
    {
        if(UDPC_Deque_push_back(hm->overflow, temp, sizeof(uint32_t) + hm->unitSize) == 0)
        {
            free(temp);
            return NULL;
        }
    }
    else if(UDPC_Deque_push_back(hm->buckets[hash], temp, sizeof(uint32_t) + hm->unitSize) == 0)
    {
        free(temp);
        return NULL;
    }

    free(temp);
    ++hm->size;
    temp = UDPC_Deque_get_back_ptr(hm->buckets[hash], sizeof(uint32_t) + hm->unitSize);
    return temp + sizeof(uint32_t);
}

int UDPC_HashMap_remove(UDPC_HashMap *hm, uint32_t key)
{
    if(hm->size == 0)
    {
        return 0;
    }

    uint32_t hash = UDPC_HASH32(key) % hm->capacity;

    for(int x = 0; x * (sizeof(uint32_t) + hm->unitSize) < hm->buckets[hash]->size; ++x)
    {
        if(memcmp(
            UDPC_Deque_index_ptr(hm->buckets[hash], sizeof(uint32_t) + hm->unitSize, x),
            &key,
            sizeof(uint32_t)) == 0)
        {
            int result = UDPC_Deque_remove(hm->buckets[hash], sizeof(uint32_t) + hm->unitSize, x);
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

    for(int x = 0; x * (sizeof(uint32_t) + hm->unitSize) < hm->overflow->size; ++x)
    {
        if(memcmp(
            UDPC_Deque_index_ptr(hm->overflow, sizeof(uint32_t) + hm->unitSize, x),
            &key,
            sizeof(uint32_t)) == 0)
        {
            int result = UDPC_Deque_remove(hm->overflow, sizeof(uint32_t) + hm->unitSize, x);
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

    for(int x = 0; x * (sizeof(uint32_t) + hm->unitSize) < hm->buckets[hash]->size; ++x)
    {
        char *ptr = UDPC_Deque_index_ptr(hm->buckets[hash], sizeof(uint32_t) + hm->unitSize, x);
        if(memcmp(
            ptr,
            &key,
            sizeof(uint32_t)) == 0)
        {
            return ptr + sizeof(uint32_t);
        }
    }

    for(int x = 0; x * (sizeof(uint32_t) + hm->unitSize) < hm->overflow->size; ++x)
    {
        char *ptr = UDPC_Deque_index_ptr(hm->overflow, sizeof(uint32_t) + hm->unitSize, x);
        if(memcmp(
            ptr,
            &key,
            sizeof(uint32_t)) == 0)
        {
            return ptr + sizeof(uint32_t);
        }
    }

    return NULL;
}
