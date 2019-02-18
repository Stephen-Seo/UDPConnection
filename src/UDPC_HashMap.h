#ifndef UDPC_HASHMAP_H
#define UDPC_HASHMAP_H

#include <stdint.h>

// 5 8 2 7 3 6 1
// 3 2 5 1 8 7 6
#define UDPC_HASH32(x) ( \
    ( \
      (((x) & 0xF8000000) >> 5)  | \
      (((x) & 0x07F80000) >> 6)  | \
      (((x) & 0x00060000) << 10) | \
      (((x) & 0x0001FC00) >> 4)  | \
      (((x) & 0x00000380) << 22) | \
      (((x) & 0x0000007E) >> 1)  | \
      (((x) & 0x00000001) << 21) \
    ) ^ 0x96969696 \
)

#define UDPC_HASHMAP_INIT_CAPACITY 13

#define UDPC_HASHMAP_MOD(k, m) ((UDPC_HASH32(k) % (m * 2 + 1)) % m)

struct UDPC_HashMap_Node {
    uint32_t key;
    char *data;
    struct UDPC_HashMap_Node *next;
    struct UDPC_HashMap_Node *prev;
};
typedef struct UDPC_HashMap_Node UDPC_HashMap_Node;

typedef struct {
    uint32_t size;
    uint32_t capacity;
    uint32_t unitSize;
    UDPC_HashMap_Node **buckets;
} UDPC_HashMap;

/*!
 * \brief Creates a HashMap structure
 * Note that UDPC_HashMap_destroy must be called on the returned ptr to free
 * resources to avoid a memory leak.
 * \return non-null if creating the HashMap was successful
 */
UDPC_HashMap* UDPC_HashMap_init(uint32_t capacity, uint32_t unitSize);

/*!
 * \brief Releases resources used by a HashMap structure
 */
void UDPC_HashMap_destroy(UDPC_HashMap *hashMap);

/*!
 * \brief Inserts a copy of data pointed to by given pointer
 * Note if size already equals capacity, the hash map's capacity is doubled
 * with UDPC_HashMap_realloc(). realloc requires rehashing of all items which
 * may be costly.
 * Also, if the hash map runs out of space for a specific key to insert, it will
 * also invoke realloc() with double the previous capacity and will attempt to
 * insert again afterwards.
 * It is possible to insert items with duplicate keys. In that case, the first
 * duplicate inserted will be the first returned with get() and first removed
 * with remove().
 * \return Pointer to inserted data, NULL on fail or unitSize = 0
 */
void* UDPC_HashMap_insert(UDPC_HashMap *hm, uint32_t key, void *data);

/*!
 * \brief Removes data with the given key
 * \return non-zero if data was successfully removed
 */
int UDPC_HashMap_remove(UDPC_HashMap *hm, uint32_t key);

/*!
 * \brief Returns a pointer to data with the given key
 * Note if unitSize == 0, then the returned pointer will point to a copy of
 * its integer key, which should not be changed manually (otherwise, the hash
 * map would not be able to find it).
 * \return non-NULL if data was found and unitSize != 0
 */
void* UDPC_HashMap_get(UDPC_HashMap *hm, uint32_t key);

/*!
 * \return non-zero if item with specified key is in the hash map
 */
int UDPC_HashMap_has(UDPC_HashMap *hm, uint32_t key);

/*!
 * \brief Resizes the maximum capacity of a hash map
 * Note on fail, the hash map is unchanged.
 * If newCapacity is less than the current size of the hash map, this function
 * will fail.
 * \return non-zero if resizing was successful
 */
int UDPC_HashMap_realloc(UDPC_HashMap *hm, uint32_t newCapacity);

/*!
 * \brief Empties the hash map
 */
void UDPC_HashMap_clear(UDPC_HashMap *hm);

uint32_t UDPC_HashMap_get_size(UDPC_HashMap *hm);

uint32_t UDPC_HashMap_get_capacity(UDPC_HashMap *hm);

/*!
 * \brief Calls a fn with a ptr to each entry in the hash map
 * The fn is called with userData, entry key, and entry data.
 */
void UDPC_HashMap_itercall(UDPC_HashMap *hm, void (*fn)(void*, uint32_t, char*), void *userData);

#endif
