#include "Deque.h"

#include <stdlib.h>
#include <string.h>

void UDPC_Deque_init(UDPC_Deque *deque, uint32_t alloc_size)
{
    deque->head = 0;
    deque->tail = 0;
    deque->size = 0;
    deque->alloc_size = alloc_size;
    deque->buf = malloc(alloc_size);
}

int UDPC_Deque_push_back(UDPC_Deque *deque, const char *data, uint32_t size)
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
        memcpy(&deque->buf[deque->tail], &data[temp], size);
        deque->tail += size;
        deque->size += size;
    }
    return 1;
}

int UDPC_Deque_push_front(UDPC_Deque *deque, const char *data, uint32_t size)
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
        memcpy(deque->buf, &data[size - deque->head], deque->head);
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

int UDPC_Deque_get_back(UDPC_Deque *deque, char **data, uint32_t *size)
{
    int returnValue = 1;
    if(deque->size == 0)
    {
        *size = 0;
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

int UDPC_Deque_get_front(UDPC_Deque *deque, char **data, uint32_t *size)
{
    int returnValue = 1;
    if(deque->size == 0)
    {
        *size = 0;
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
        deque->head = 0;
        deque->tail = 0;
        deque->size = 0;
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
        deque->head = 0;
        deque->tail = 0;
        deque->size = 0;
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
