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

int UDPC_HashMap_realloc(UDPC_HashMap *hm, uint32_t newCapacity)
{
    if(hm->size > newCapacity)
    {
        return 0;
    }

    // allocate newBuckets
    int fail = 0;
    UDPC_HashMap_Node **newBuckets = malloc(sizeof(UDPC_HashMap_Node*) * newCapacity);
    if(!newBuckets) { return 0; }
    for(int x = 0; x < newCapacity; ++x)
    {
        if(fail != 0) { newBuckets[x] = NULL; continue; }
        newBuckets[x] = calloc(1, sizeof(UDPC_HashMap_Node));
        if(!newBuckets[x]) { fail = 1; }
    }
    if(fail != 0)
    {
        for(int x = 0; x < newCapacity; ++x)
        {
            if(newBuckets[x]) { free(newBuckets[x]); }
        }
        free(newBuckets);
        return 0;
    }

    // rehash entries from hm->buckets to newBuckets
    uint32_t hash;
    UDPC_HashMap_Node *current;
    UDPC_HashMap_Node *next;
    UDPC_HashMap_Node *newCurrent;
    for(int x = 0; x < hm->capacity; ++x)
    {
        current = hm->buckets[x]->next;
        while(current)
        {
            next = current->next;
            hash = UDPC_HASHMAP_MOD(current->key, newCapacity);
            newCurrent = newBuckets[hash];
            while(newCurrent->next)
            {
                newCurrent = newCurrent->next;
            }
            newCurrent->next = malloc(sizeof(UDPC_HashMap_Node));
            if(!newCurrent->next)
            {
                fail = 1;
                break;
            }
            newCurrent->next->key = current->key;
            newCurrent->next->data = current->data;
            newCurrent->next->next = NULL;
            newCurrent->next->prev = newCurrent;
            current = next;
        }
        if(fail != 0)
        {
            break;
        }
    }
    if(fail != 0)
    {
        for(int x = 0; x < newCapacity; ++x)
        {
            current = newBuckets[x];
            while(current)
            {
                next = current->next;
                free(current);
                current = next;
            }
        }
        free(newBuckets);
        return 0;
    }

    // cleanup hm->buckets to be replaced by newBuckets
    for(int x = 0; x < hm->capacity; ++x)
    {
        current = hm->buckets[x];
        while(current)
        {
            next = current->next;
            // do not free current->data as it is now being pointed to by entries in newBuckets
            free(current);
            current = next;
        }
    }
    free(hm->buckets);

    hm->capacity = newCapacity;
    hm->buckets = newBuckets;

    return 1;
}

void UDPC_HashMap_clear(UDPC_HashMap *hm)
{
    UDPC_HashMap_Node *current;
    UDPC_HashMap_Node *next;
    for(int x = 0; x < hm->capacity; ++x)
    {
        current = hm->buckets[x]->next;
        while(current)
        {
            next = current->next;
            if(current->data) { free(current->data); }
            free(current);
            current = next;
        }
        hm->buckets[x]->next = NULL;
    }
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

void UDPC_HashMap_itercall(UDPC_HashMap *hm, void (*fn)(void*, uint32_t, char*), void *userData)
{
    UDPC_HashMap_Node *current;
    for(int x = 0; x < hm->capacity; ++x)
    {
        current = hm->buckets[x]->next;
        while(current)
        {
            fn(userData, current->key, current->data);
            current = current->next;
        }
    }
}
