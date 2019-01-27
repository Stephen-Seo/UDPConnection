#include "UDPC_Deque.h"

#include <stdlib.h>
#include <string.h>

UDPC_Deque* UDPC_Deque_init(uint32_t alloc_size)
{
    UDPC_Deque *deque = malloc(sizeof(UDPC_Deque));
    UDPC_Deque_clear(deque);
    deque->alloc_size = alloc_size;
    deque->buf = malloc(alloc_size);
    if(deque->buf)
    {
        return deque;
    }
    else
    {
        free(deque);
        return NULL;
    }
}

void UDPC_Deque_destroy(UDPC_Deque **deque)
{
    free((*deque)->buf);
    free(*deque);
    *deque = NULL;
}

int UDPC_Deque_realloc(UDPC_Deque *deque, uint32_t new_size)
{
    if(new_size < deque->size)
    {
        return 0;
    }
    else if(deque->size != 0 && deque->tail <= deque->head)
    {
        char *buf = malloc(new_size);
        memcpy(buf, &deque->buf[deque->head], deque->alloc_size - deque->head);
        if(deque->tail != 0)
        {
            memcpy(&buf[deque->alloc_size - deque->head], deque->buf, deque->tail);
        }
        free(deque->buf);
        deque->buf = buf;
        deque->alloc_size = new_size;
        deque->head = 0;
        deque->tail = deque->size;
        if(deque->tail == deque->alloc_size)
        {
            deque->tail = 0;
        }
        return 1;
    }
    else
    {
        void *buf = realloc(deque->buf, new_size);
        if(buf)
        {
            deque->buf = buf;
            deque->alloc_size = new_size;
            return 1;
        }
        else
        {
            return 0;
        }
    }
}

int UDPC_Deque_push_back(UDPC_Deque *deque, const void *data, uint32_t size)
{
    if(deque->size + size > deque->alloc_size)
    {
        return 0;
    }
    else if(deque->tail + size <= deque->alloc_size)
    {
        memcpy(&deque->buf[deque->tail], data, size);
        deque->tail += size;
        if(deque->tail == deque->alloc_size)
        {
            deque->tail = 0;
        }
        deque->size += size;

        return 1;
    }

    uint32_t temp;

    if(deque->tail < deque->alloc_size)
    {
        memcpy(&deque->buf[deque->tail], data, deque->alloc_size - deque->tail);
        temp = deque->alloc_size - deque->tail;
        deque->size += temp;
        size -= temp;
        deque->tail = 0;
    }
    if(size > 0)
    {
        memcpy(&deque->buf[deque->tail], &((const char*)data)[temp], size);
        deque->tail += size;
        deque->size += size;
    }
    return 1;
}

int UDPC_Deque_push_front(UDPC_Deque *deque, const void *data, uint32_t size)
{
    if(deque->size + size > deque->alloc_size)
    {
        return 0;
    }
    else if(size <= deque->head)
    {
        memcpy(&deque->buf[deque->head - size], data, size);
        deque->head -= size;
        deque->size += size;

        return 1;
    }

    if(deque->head > 0)
    {
        memcpy(deque->buf, &((const char*)data)[size - deque->head], deque->head);
        deque->size += deque->head;
        size -= deque->head;
        deque->head = 0;
    }
    if(size > 0)
    {
        memcpy(&deque->buf[deque->alloc_size - size], data, size);
        deque->head = deque->alloc_size - size;
        deque->size += size;
    }
    return 1;
}

uint32_t UDPC_Deque_get_available(UDPC_Deque *deque)
{
    return deque->alloc_size - deque->size;
}

uint32_t UDPC_Deque_get_used(UDPC_Deque *deque)
{
    return deque->size;
}

int UDPC_Deque_get_back(UDPC_Deque *deque, void **data, uint32_t *size)
{
    int returnValue = 1;
    if(deque->size == 0)
    {
        *size = 0;
        *data = NULL;
        return 0;
    }
    else if(*size > deque->size)
    {
        *size = deque->size;
        returnValue = 0;
    }

    *data = malloc(*size);

    if(deque->tail < *size)
    {
        memcpy(data[*size - deque->tail], deque->buf, deque->tail);
        memcpy(
            *data,
            &deque->buf[deque->alloc_size - (*size - deque->tail)],
            *size - deque->tail);
        return returnValue;
    }

    memcpy(*data, &deque->buf[deque->tail - *size], *size);
    return returnValue;
}

int UDPC_Deque_get_front(UDPC_Deque *deque, void **data, uint32_t *size)
{
    int returnValue = 1;
    if(deque->size == 0)
    {
        *size = 0;
        *data = NULL;
        return 0;
    }
    else if(*size > deque->size)
    {
        *size = deque->size;
        returnValue = 0;
    }

    *data = malloc(*size);

    if(deque->head + *size > deque->alloc_size)
    {
        memcpy(*data, &deque->buf[deque->head], deque->alloc_size - deque->head);
        memcpy(
            data[deque->alloc_size - deque->head],
            deque->buf,
            *size - (deque->alloc_size - deque->head));
        return returnValue;
    }

    memcpy(*data, &deque->buf[deque->head], *size);
    return returnValue;
}

void UDPC_Deque_pop_back(UDPC_Deque *deque, uint32_t size)
{
    if(deque->size == 0)
    {
        return;
    }
    else if(deque->size <= size)
    {
        UDPC_Deque_clear(deque);
        return;
    }

    deque->size -= size;

    if(deque->tail < size)
    {
        deque->tail = deque->alloc_size - (size - deque->tail);
    }
    else
    {
        deque->tail -= size;
    }
}

void UDPC_Deque_pop_front(UDPC_Deque *deque, uint32_t size)
{
    if(deque->size == 0)
    {
        return;
    }
    else if(deque->size <= size)
    {
        UDPC_Deque_clear(deque);
        return;
    }

    deque->size -= size;

    if(deque->head + size > deque->alloc_size)
    {
        deque->head = deque->head + size - deque->alloc_size;
    }
    else
    {
        deque->head += size;
        if(deque->head == deque->alloc_size)
        {
            deque->head = 0;
        }
    }
}

int UDPC_Deque_index(UDPC_Deque *deque, uint32_t unitSize, uint32_t index, void **out)
{
    uint32_t pos = unitSize * index;
    uint32_t abspos;
    if(pos >= deque->size)
    {
        *out = NULL;
        return 0;
    }

    *out = malloc(unitSize);

    if(pos + deque->head >= deque->alloc_size)
    {
        abspos = pos + deque->head - deque->alloc_size;
    }
    else
    {
        abspos = pos + deque->head;
    }

    if(abspos + unitSize >= deque->alloc_size)
    {
        memcpy(*out, &deque->buf[abspos], deque->alloc_size - abspos);
        memcpy(*out, deque->buf, unitSize - (deque->alloc_size - abspos));
    }
    else
    {
        memcpy(*out, &deque->buf[abspos], unitSize);
    }

    return 1;
}

int UDPC_Deque_index_rev(UDPC_Deque *deque, uint32_t unitSize, uint32_t index, void **out)
{
    uint32_t pos = unitSize * (index + 1);
    uint32_t abspos;
    if(pos >= deque->size + unitSize)
    {
        *out = NULL;
        return 0;
    }

    *out = malloc(unitSize);

    if(pos > deque->tail)
    {
        abspos = deque->alloc_size - (pos - deque->tail);
    }
    else
    {
        abspos = deque->tail - pos;
    }

    if(abspos + unitSize >= deque->alloc_size)
    {
        memcpy(*out, &deque->buf[abspos], deque->alloc_size - abspos);
        memcpy(*out, deque->buf, unitSize - (deque->alloc_size - abspos));
    }
    else
    {
        memcpy(*out, &deque->buf[abspos], unitSize);
    }

    return 1;
}

void UDPC_Deque_clear(UDPC_Deque *deque)
{
    deque->head = 0;
    deque->tail = 0;
    deque->size = 0;
}
