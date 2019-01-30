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

/*!
 * \return non-null on success
 */
UDPC_Deque* UDPC_Deque_init(uint32_t alloc_size);

/*!
 * Frees resources used by a UDPC_Deque
 */
void UDPC_Deque_destroy(UDPC_Deque *deque);

/*!
 * Fails if new_size is smaller than current size of Deque.
 * On failure, deque remains unchanged.
 * \return non-zero on success
 */
int UDPC_Deque_realloc(UDPC_Deque *deque, uint32_t new_size);

/*!
 * If there was not enough space in the Deque, then no data is inserted at all.
 * \return non-zero on success (there was enough size to insert data)
 */
int UDPC_Deque_push_back(UDPC_Deque *deque, const void *data, uint32_t size);

/*!
 * If there was not enough space in the Deque, then no data is inserted at all.
 * \return non-zero on success (there was enough size to insert data)
 */
int UDPC_Deque_push_front(UDPC_Deque *deque, const void *data, uint32_t size);

/*!
 * \return size in bytes of available data
 */
uint32_t UDPC_Deque_get_available(UDPC_Deque *deque);

/*!
 * \return size in bytes of used data
 */
uint32_t UDPC_Deque_get_used(UDPC_Deque *deque);

/*!
 * \brief Get data from back of deque
 * Data must be free'd after use as it was allocated with malloc.
 * When size is greater than deque size, partial data is allocated, size is
 * updated to allocated amount. If deque is empty, no data is allocated.
 * \return non-zero if full requested size was returned
 */
int UDPC_Deque_get_back(UDPC_Deque *deque, void **data, uint32_t *size);

/*!
 * \brief Get data from front of deque
 * Data must be free'd after use as it was allocated with malloc.
 * When size is greater than deque size, partial data is allocated, size is
 * updated to allocated amount. If deque is empty, no data is allocated.
 * \return non-zero if full requested size was returned
 */
int UDPC_Deque_get_front(UDPC_Deque *deque, void **data, uint32_t *size);

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

/*!
 * \brief Get a unitSize sized chunk of data at position unitSize * index
 * The data will be indexed relative to the head of the Deque.
 * The out pointer will be malloc'd with size unitSize and will have a copy of
 * the data at the specified unitSize * index.
 * Note that the out data must be free'd, but on fail nothing will be malloc'd
 * and *out will be set to NULL.
 * \return non-zero if unitSize * index < size
 */
int UDPC_Deque_index(UDPC_Deque *deque, uint32_t unitSize, uint32_t index, void **out);

/*!
 * \brief Get a unitSize sized chunk of data at position relative to tail
 * The out pointer will be malloc'd with size unitSize and will have a copy of
 * the data at the specified unitSize * index relative to tail in reverse
 * direction.
 * Note that the out data must be free'd, but on fail nothing will be malloc'd
 * and *out will be set to NULL.
 * \return non-zero if unitSize * index < size
 */
int UDPC_Deque_index_rev(UDPC_Deque *deque, uint32_t unitSize, uint32_t index, void **out);

/*!
 * \brief Replaces the data at index with data at the end (if exists)
 * Note this will reduce the size of the Deque by unitSize amount.
 * \return non-zero if data was removed
 */
int UDPC_Deque_remove(UDPC_Deque *deque, uint32_t unitSize, uint32_t index);

void UDPC_Deque_clear(UDPC_Deque *deque);

#endif
