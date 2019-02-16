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

    m->buckets = malloc(sizeof(UDPC_HashMap_Node*) * m->capacity);
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
        m->buckets[x] = calloc(1, sizeof(UDPC_HashMap_Node));
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
                free(m->buckets[x]);
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
    UDPC_HashMap_Node *current;
    UDPC_HashMap_Node *next;
    for(int x = 0; x < hashMap->capacity; ++x)
    {
        current = hashMap->buckets[x];
        while(current)
        {
            next = current->next;
            if(current->data) { free(current->data); }
            free(current);
            current = next;
        }
    }
    free(hashMap->buckets);
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

    uint32_t hash = UDPC_HASHMAP_MOD(key, hm->capacity);

    UDPC_HashMap_Node *current = hm->buckets[hash];
    while(current->next)
    {
        current = current->next;
    }
    current->next = malloc(sizeof(UDPC_HashMap_Node));
    current->next->key = key;
    if(hm->unitSize != 0)
    {
        current->next->data = malloc(hm->unitSize);
        memcpy(current->next->data, data, hm->unitSize);
    }
    else
    {
        current->next->data = NULL;
    }
    current->next->next = NULL;
    current->next->prev = current;

    ++hm->size;
    return current->next->data;
}

int UDPC_HashMap_remove(UDPC_HashMap *hm, uint32_t key)
{
    if(hm->size == 0)
    {
        return 0;
    }

    uint32_t hash = UDPC_HASHMAP_MOD(key, hm->capacity);

    UDPC_HashMap_Node *current = hm->buckets[hash];
    while(current && (current == hm->buckets[hash] || current->key != key))
    {
        current = current->next;
    }

    if(!current) { return 0; }

    current->prev->next = current->next;
    if(current->next) { current->next->prev = current->prev; }

    if(current->data) { free(current->data); }
    free(current);

    return 1;
}

void* UDPC_HashMap_get(UDPC_HashMap *hm, uint32_t key)
{
    if(hm->size == 0)
    {
        return NULL;
    }

    uint32_t hash = UDPC_HASHMAP_MOD(key, hm->capacity);

    UDPC_HashMap_Node *current = hm->buckets[hash];
    while(current && (current == hm->buckets[hash] || current->key != key))
    {
        current = current->next;
    }

    if(!current) { return NULL; }

    return current->data;
}

int UDPC_HashMap_has(UDPC_HashMap *hm, uint32_t key)
{
    if(hm->size == 0)
    {
        return 0;
    }

    uint32_t hash = UDPC_HASHMAP_MOD(key, hm->capacity);

    UDPC_HashMap_Node *current = hm->buckets[hash];
    while(current && (current == hm->buckets[hash] || current->key != key))
    {
        current = current->next;
    }

    return current != NULL ? 1 : 0;
}

// TODO change to linkedList buckets up to this point
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
            hash = UDPC_HASHMAP_MOD(*((uint32_t*)data), newCapacity);
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
            hash = UDPC_HASHMAP_MOD(*((uint32_t*)data), newCapacity);
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

void UDPC_HashMap_itercall(UDPC_HashMap *hm, void (*fn)(void*, char*), void *userData)
{
    for(int x = 0; x < hm->capacity; ++x)
    {
        for(int y = 0; y * (4 + hm->unitSize) < hm->buckets[x]->size; ++y)
        {
            char *data = UDPC_Deque_index_ptr(
                hm->buckets[x], 4 + hm->unitSize, y);
            if(hm->unitSize > 0) { fn(userData, data + 4); }
            else { fn(userData, data); }
        }
    }
    for(int x = 0; x * (4 + hm->unitSize) < hm->overflow->size; ++x)
    {
        char *data = UDPC_Deque_index_ptr(
            hm->overflow, 4 + hm->unitSize, x);
        if(hm->unitSize > 0) { fn(userData, data + 4); }
        else { fn(userData, data); }
    }
}

void* UDPC_HashMap_INTERNAL_reinsert(UDPC_HashMap *hm, uint32_t key, void *data)
{
    if(hm->capacity <= hm->size)
    {
        return NULL;
    }

    uint32_t hash = UDPC_HASHMAP_MOD(key, hm->capacity);

    char *temp = malloc(4 + hm->unitSize);
    memcpy(temp, &key, 4);
    if(hm->unitSize > 0)
    {
        memcpy(temp + 4, data, hm->unitSize);
    }

    if(UDPC_Deque_get_available(hm->buckets[hash]) == 0)
    {
        if(UDPC_Deque_get_available(hm->overflow) == 0)
        {
            free(temp);
            return NULL;
        }
        else if(UDPC_Deque_push_back(hm->overflow, temp, 4 + hm->unitSize) == 0)
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
