#ifndef UDPC_DEQUE_H
#define UDPC_DEQUE_H

#include <stdint.h>

typedef struct
{
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t alloc_size;
    char *buf;
} UDPC_Deque;

void UDPC_Deque_init(UDPC_Deque *deque, uint32_t alloc_size);

/*!
 * If there was not enough space in the Deque, then no data is inserted at all.
 * \return non-zero on success (there was enough size to insert data)
 */
int UDPC_Deque_push_back(UDPC_Deque *deque, const char *data, uint32_t size);

/*!
 * If there was not enough space in the Deque, then no data is inserted at all.
 * \return non-zero on success (there was enough size to insert data)
 */
int UDPC_Deque_push_front(UDPC_Deque *deque, const char *data, uint32_t size);

uint32_t UDPC_Deque_get_available(UDPC_Deque *deque);

/*!
 * \brief Get data from back of deque
 * Data must be free'd after use as it was allocated with malloc.
 * When size is greater than deque size, partial data is allocated, size is
 * updated to allocated amount. If deque is empty, no data is allocated.
 * \return non-zero if full requested size was returned
 */
int UDPC_Deque_get_back(UDPC_Deque *deque, char **data, uint32_t *size);

/*!
 * \brief Get data from front of deque
 * Data must be free'd after use as it was allocated with malloc.
 * When size is greater than deque size, partial data is allocated, size is
 * updated to allocated amount. If deque is empty, no data is allocated.
 * \return non-zero if full requested size was returned
 */
int UDPC_Deque_get_front(UDPC_Deque *deque, char **data, uint32_t *size);

/*!
 * \brief "free" data from the back of the deque
 * If size is greater than data used, then all data will be "free"d.
 * Note that this doesn't actually deallocate data, but changes internal values
 * that keep track of data positions in the internal buffer. The data will only
 * actually be removed when it is overwritten or the Deque is free'd.
 */
void UDPC_Deque_pop_back(UDPC_Deque *deque, uint32_t size);

/*!
 * \brief "free" data from the front of the deque
 * If size is greater than data used, then all data will be "free"d.
 * Note that this doesn't actually deallocate data, but changes internal values
 * that keep track of data positions in the internal buffer. The data will only
 * actually be removed when it is overwritten or the Deque is free'd.
 */
void UDPC_Deque_pop_front(UDPC_Deque *deque, uint32_t size);

#endif
