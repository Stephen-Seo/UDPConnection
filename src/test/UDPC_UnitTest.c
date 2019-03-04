#include "UDPC_UnitTest.h"

#include <stdlib.h>
#include <UDPC_Deque.h>
#include <UDPC_HashMap.h>
#include <UDPConnection.h>

static UnitTestState UDPC_uts = {0, 0};

void TEST_DEQUE()
{
    int arr[32];
    char *temp = NULL;
    uint32_t size;
    for(int x = 0; x < 32; ++x)
    {
        arr[x] = x;
    }
    UDPC_Deque *deque;

    // init
    deque = UDPC_Deque_init(sizeof(int) * 32);
    ASSERT_TRUE(deque);
    ASSERT_TRUE(deque->buf);
    ASSERT_EQ(deque->head, 0);
    ASSERT_EQ(deque->tail, 0);
    ASSERT_EQ(deque->size, 0);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 32);

    // realloc smaller success
    ASSERT_TRUE(UDPC_Deque_realloc(deque, sizeof(int) * 16));
    ASSERT_TRUE(deque->buf);
    ASSERT_EQ(deque->head, 0);
    ASSERT_EQ(deque->tail, 0);
    ASSERT_EQ(deque->size, 0);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 16);

    // push back success
    ASSERT_TRUE(UDPC_Deque_push_back(deque, arr, sizeof(int) * 4));
    ASSERT_EQ_MEM(arr, deque->buf, sizeof(int) * 4);
    ASSERT_EQ_MEM(arr, UDPC_Deque_get_back_ptr(deque, sizeof(int) * 4), sizeof(int) * 4);
    ASSERT_EQ_MEM(arr, UDPC_Deque_get_front_ptr(deque, sizeof(int) * 4), sizeof(int) * 4);
    for(int x = 0; x < 4; ++x)
    {
        ASSERT_EQ_MEM(&arr[x], UDPC_Deque_index_ptr(deque, sizeof(int), x), sizeof(int));
        ASSERT_EQ_MEM(&arr[3 - x], UDPC_Deque_index_rev_ptr(deque, sizeof(int), x), sizeof(int));
    }
    ASSERT_EQ(deque->size, sizeof(int) * 4);

    // push front success
    ASSERT_TRUE(UDPC_Deque_push_front(deque, &arr[4], sizeof(int) * 4));
    ASSERT_EQ_MEM(&arr[4], &deque->buf[sizeof(int) * 12], sizeof(int) * 4);
    ASSERT_EQ_MEM(arr, UDPC_Deque_get_back_ptr(deque, sizeof(int) * 4), sizeof(int) * 4);
    ASSERT_EQ_MEM(&arr[4], UDPC_Deque_get_front_ptr(deque, sizeof(int) * 4), sizeof(int) * 4);
    for(int x = 0; x < 4; ++x)
    {
        ASSERT_EQ_MEM(&arr[x + 4], UDPC_Deque_index_ptr(deque, sizeof(int), x), sizeof(int));
        ASSERT_EQ_MEM(&arr[x], UDPC_Deque_index_ptr(deque, sizeof(int), x + 4), sizeof(int));
        ASSERT_EQ_MEM(&arr[3 - x], UDPC_Deque_index_rev_ptr(deque, sizeof(int), x), sizeof(int));
        ASSERT_EQ_MEM(&arr[7 - x], UDPC_Deque_index_rev_ptr(deque, sizeof(int), x + 4), sizeof(int));
    }
    ASSERT_EQ(deque->size, sizeof(int) * 8);

    // realloc bigger success
    ASSERT_TRUE(UDPC_Deque_realloc(deque, sizeof(int) * 32));
    ASSERT_EQ_MEM(&arr[4], deque->buf, sizeof(int) * 4);
    ASSERT_EQ_MEM(arr, &deque->buf[sizeof(int) * 4], sizeof(int) * 4);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 32);

    // pop front success
    UDPC_Deque_pop_front(deque, sizeof(int) * 4);
    ASSERT_EQ(deque->size, sizeof(int) * 4);

    // get front success
    size = sizeof(int) * 4;
    ASSERT_TRUE(UDPC_Deque_get_front(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 4);
    ASSERT_EQ_MEM(arr, temp, size);
    free(temp);

    // pop back success
    UDPC_Deque_pop_back(deque, sizeof(int) * 4);
    ASSERT_EQ(deque->size, 0);

    // push front success
    ASSERT_TRUE(UDPC_Deque_push_front(deque, arr, sizeof(int) * 16));
    ASSERT_EQ(deque->size, sizeof(int) * 16);

    // get front success
    size = sizeof(int) * 16;
    ASSERT_TRUE(UDPC_Deque_get_front(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 16);
    ASSERT_EQ_MEM(arr, temp, size);
    free(temp);

    // get back success
    size = sizeof(int) * 16;
    ASSERT_TRUE(UDPC_Deque_get_back(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 16);
    ASSERT_EQ_MEM(arr, temp, size);
    free(temp);

    // realloc smaller fail
    ASSERT_FALSE(UDPC_Deque_realloc(deque, sizeof(int) * 8));
    ASSERT_EQ(deque->size, sizeof(int) * 16);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 32);

    // realloc smaller success
    ASSERT_TRUE(UDPC_Deque_realloc(deque, sizeof(int) * 16));
    ASSERT_EQ(deque->size, sizeof(int) * 16);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 16);
    ASSERT_EQ_MEM(deque->buf, arr, sizeof(int) * 16);

    // push back fail
    ASSERT_FALSE(UDPC_Deque_push_back(deque, arr, sizeof(int) * 16));
    ASSERT_EQ(deque->size, sizeof(int) * 16);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 16);
    ASSERT_EQ_MEM(deque->buf, arr, sizeof(int) * 16);

    // push front fail
    ASSERT_FALSE(UDPC_Deque_push_back(deque, arr, sizeof(int) * 16));
    ASSERT_EQ(deque->size, sizeof(int) * 16);
    ASSERT_EQ(deque->alloc_size, sizeof(int) * 16);
    ASSERT_EQ_MEM(deque->buf, arr, sizeof(int) * 16);

    // pop back
    UDPC_Deque_pop_back(deque, sizeof(int) * 8);

    // get front success
    size = sizeof(int) * 8;
    ASSERT_TRUE(UDPC_Deque_get_front(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 8);
    ASSERT_EQ_MEM(temp, arr, sizeof(int) * 8);
    free(temp);

    // get front fail
    size = sizeof(int) * 16;
    ASSERT_FALSE(UDPC_Deque_get_front(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 8);
    ASSERT_EQ_MEM(temp, arr, sizeof(int) * 8);
    free(temp);

    // get back success
    size = sizeof(int) * 8;
    ASSERT_TRUE(UDPC_Deque_get_back(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 8);
    ASSERT_EQ_MEM(temp, arr, sizeof(int) * 8);
    free(temp);

    // get back fail
    size = sizeof(int) * 16;
    ASSERT_FALSE(UDPC_Deque_get_back(deque, (void**)&temp, &size));
    ASSERT_EQ(size, sizeof(int) * 8);
    ASSERT_EQ_MEM(temp, arr, sizeof(int) * 8);
    free(temp);

    // index success
    for(int x = 0; x < 8; ++x)
    {
        ASSERT_TRUE(UDPC_Deque_index(deque, sizeof(int), x, (void**)&temp));
        if(temp)
        {
            ASSERT_EQ_MEM(temp, &arr[x], sizeof(int));
            free(temp);
        }
    }

    // index fail
    ASSERT_FALSE(UDPC_Deque_index(deque, sizeof(int), 8, (void**)&temp));
    ASSERT_FALSE(temp);

    // index_rev success
    for(int x = 0; x < 8; ++x)
    {
        ASSERT_TRUE(UDPC_Deque_index_rev(deque, sizeof(int), x, (void**)&temp));
        if(temp)
        {
            ASSERT_EQ_MEM(temp, &arr[7 - x], sizeof(int));
            free(temp);
        }
    }

    // index_rev fail
    ASSERT_FALSE(UDPC_Deque_index_rev(deque, sizeof(int), 8, (void**)&temp));
    ASSERT_FALSE(temp);

    // remove success front
    ASSERT_TRUE(UDPC_Deque_remove(deque, sizeof(int), 0));
    ASSERT_EQ(sizeof(int) * 7, deque->size);
    ASSERT_EQ_MEM(deque->buf, &arr[7], sizeof(int));

    // remove success end
    ASSERT_TRUE(UDPC_Deque_remove(deque, sizeof(int), 6));
    ASSERT_EQ(sizeof(int) * 6, deque->size);
    ASSERT_EQ_MEM(&deque->buf[deque->tail - sizeof(int)], &arr[5], sizeof(int));

    // remove success middle
    ASSERT_TRUE(UDPC_Deque_remove(deque, sizeof(int), 2));
    ASSERT_EQ(sizeof(int) * 5, deque->size);
    ASSERT_EQ_MEM(&deque->buf[deque->head + sizeof(int) * 2], &arr[5], sizeof(int));

    // remove success until empty
    while(deque->size > 0)
    {
        ASSERT_TRUE(UDPC_Deque_remove(deque, sizeof(int), 0));
    }
    ASSERT_EQ(deque->size, 0);

    // test push_back_realloc
    ASSERT_EQ(deque->alloc_size, 16 * sizeof(int));
    for(int x = 0; x < 16; ++x)
    {
        ASSERT_NEQ(UDPC_Deque_push_back(deque, &x, sizeof(int)), 0);
    }
    int tempInt = 20;
    ASSERT_EQ(UDPC_Deque_push_back(deque, &tempInt, sizeof(int)), 0);
    ASSERT_NEQ(UDPC_Deque_push_back_realloc(deque, &tempInt, sizeof(int)), 0);
    ASSERT_EQ(deque->alloc_size, 32 * sizeof(int));
    ASSERT_EQ_MEM(UDPC_Deque_get_back_ptr(deque, sizeof(int)), &tempInt, sizeof(int));

    UDPC_Deque_pop_back(deque, sizeof(int));
    ASSERT_NEQ(UDPC_Deque_realloc(deque, 16 * sizeof(int)), 0);

    // test push_front_realloc
    ASSERT_EQ(UDPC_Deque_push_front(deque, &tempInt, sizeof(int)), 0);
    ASSERT_NEQ(UDPC_Deque_push_front_realloc(deque, &tempInt, sizeof(int)), 0);
    ASSERT_EQ(deque->alloc_size, 32 * sizeof(int));
    ASSERT_EQ_MEM(UDPC_Deque_get_front_ptr(deque, sizeof(int)), &tempInt, sizeof(int));

    /*
    for(int x = 0; x < deque->tail / sizeof(int); ++x)
    {
        temp = &deque->buf[x * sizeof(int)];
        printf("%d: %d   ", x, *((int*)temp));
    }
    printf("\n");
    printf("asize %d, size %d, head %d, tail %d\n",
        deque->alloc_size, deque->size, deque->head, deque->tail);
    */

    UDPC_Deque_destroy(deque);
    UNITTEST_REPORT(DEQUE)
}

void TEST_ATOSTR()
{
    UDPC_Context ctx;
    ASSERT_EQ_MEM(
        UDPC_INTERNAL_atostr(&ctx, (0xFF << 24) | (0x1 << 16) | (0x1E << 8) | 0xAC),
        "172.30.1.255",
        13);
    UNITTEST_REPORT(ATOSTR);
}

void TEST_HASHMAP_itercall_comp(void *userData, uint32_t key, char *data)
{
    *((int*)userData) += 1;
    int temp = *((int*)(data)) / 100;
    ASSERT_EQ(temp, key);
}

void TEST_HASHMAP()
{
    UDPC_HashMap *hm = UDPC_HashMap_init(0, sizeof(int));
    int temp;

    temp = 1333;
    ASSERT_NEQ(UDPC_HashMap_insert(hm, 0, &temp), NULL);
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 0), &temp, sizeof(int));
    ASSERT_NEQ(UDPC_HashMap_has(hm, 0), 0);
    ASSERT_EQ(UDPC_HashMap_has(hm, 1), 0);

    temp = 9999;
    ASSERT_NEQ(UDPC_HashMap_insert(hm, 1, &temp), NULL);
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 1), &temp, sizeof(int));
    ASSERT_NEQ(UDPC_HashMap_has(hm, 0), 0);
    ASSERT_NEQ(UDPC_HashMap_has(hm, 1), 0);
    ASSERT_EQ(UDPC_HashMap_has(hm, 2), 0);

    temp = 1235987;
    ASSERT_NEQ(UDPC_HashMap_insert(hm, 2, &temp), NULL);
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 2), &temp, sizeof(int));
    ASSERT_NEQ(UDPC_HashMap_has(hm, 0), 0);
    ASSERT_NEQ(UDPC_HashMap_has(hm, 1), 0);
    ASSERT_NEQ(UDPC_HashMap_has(hm, 2), 0);
    ASSERT_EQ(UDPC_HashMap_has(hm, 3), 0);

    ASSERT_NEQ(UDPC_HashMap_remove(hm, 1), 0);
    temp = 1333;
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 0), &temp, sizeof(int));
    temp = 1235987;
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 2), &temp, sizeof(int));
    ASSERT_NEQ(UDPC_HashMap_has(hm, 0), 0);
    ASSERT_EQ(UDPC_HashMap_has(hm, 1), 0);
    ASSERT_NEQ(UDPC_HashMap_has(hm, 2), 0);

    ASSERT_EQ(UDPC_HashMap_realloc(hm, 0), 0);
    ASSERT_NEQ(UDPC_HashMap_realloc(hm, 16), 0);

    temp = 1333;
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 0), &temp, sizeof(int));
    temp = 1235987;
    ASSERT_EQ_MEM(UDPC_HashMap_get(hm, 2), &temp, sizeof(int));
    ASSERT_NEQ(UDPC_HashMap_has(hm, 0), 0);
    ASSERT_EQ(UDPC_HashMap_has(hm, 1), 0);
    ASSERT_NEQ(UDPC_HashMap_has(hm, 2), 0);

    UDPC_HashMap_clear(hm);
    ASSERT_EQ(hm->size, 0);
    ASSERT_EQ(hm->capacity, 16);

    ASSERT_NEQ(UDPC_HashMap_realloc(hm, 8), 0);
    ASSERT_EQ(hm->size, 0);
    ASSERT_EQ(hm->capacity, 8);

    for(int x = 0; x < 8; ++x)
    {
        temp = x * 100;
        ASSERT_NEQ(UDPC_HashMap_insert(hm, x, &temp), NULL);
    }
    for(int x = 0; x < 8; ++x)
    {
        temp = x * 100;
        ASSERT_EQ_MEM(UDPC_HashMap_get(hm, x), &temp, sizeof(int));
        ASSERT_NEQ(UDPC_HashMap_has(hm, x), 0);
    }
    ASSERT_GTE(hm->capacity, 8);

    temp = 800;
    ASSERT_NEQ(UDPC_HashMap_insert(hm, 8, &temp), NULL);
    for(int x = 0; x < 9; ++x)
    {
        temp = x * 100;
        ASSERT_EQ_MEM(UDPC_HashMap_get(hm, x), &temp, sizeof(int));
        ASSERT_NEQ(UDPC_HashMap_has(hm, x), 0);
    }
    ASSERT_GTE(hm->capacity, 16);

    for(int x = 0; x < 9; ++x)
    {
        ASSERT_NEQ(UDPC_HashMap_remove(hm, x), 0);
        ASSERT_EQ(UDPC_HashMap_has(hm, x), 0);
    }
    ASSERT_EQ(hm->size, 0);
    ASSERT_GTE(hm->capacity, 16);

    for(int x = 0; x < 32; ++x)
    {
        temp = x * 100;
        ASSERT_NEQ(UDPC_HashMap_insert(hm, x, &temp), NULL);
    }
    ASSERT_EQ(hm->size, 32);

    for(int x = 0; x < 32; ++x)
    {
        temp = x * 100;
        ASSERT_EQ_MEM(UDPC_HashMap_get(hm, x), &temp, sizeof(int));
        ASSERT_NEQ(UDPC_HashMap_has(hm, x), 0);
    }

    temp = 0;
    UDPC_HashMap_itercall(hm, TEST_HASHMAP_itercall_comp, &temp);
    ASSERT_EQ(temp, 32);

    // TODO DEBUG
    /*
    printf("Size = %d\n", hm->size);
    printf("Capacity = %d\n", hm->capacity);
    for(int x = 0; x < hm->capacity; ++x)
    {
        for(int y = 0; y * (4 + sizeof(int)) < hm->buckets[x]->size; ++y)
        {
            printf("Bucket%d[%d] = %d\n", x, y,
                *((int*)&hm->buckets[x]->buf[y * (4 + sizeof(int)) + 4]));
        }
    }
    for(int x = 0; x < hm->overflow->size; ++x)
    {
        printf("Overflow[%d] = %d\n", x,
            *((int*)&hm->overflow->buf[x * (4 + sizeof(int)) + 4]));
    }
    */

    UDPC_HashMap_destroy(hm);
    UNITTEST_REPORT(HASHMAP);
}

void TEST_STRTOA()
{
    ASSERT_EQ(0x04030201, UDPC_strtoa("1.2.3.4"));
    ASSERT_EQ(0x0100007F, UDPC_strtoa("127.0.0.1"));
    ASSERT_EQ(0xFF01A8C0, UDPC_strtoa("192.168.1.255"));
    ASSERT_EQ(0, UDPC_strtoa("1.2.3.4.5"));
    ASSERT_EQ(0, UDPC_strtoa("100.20.30"));
    ASSERT_EQ(0, UDPC_strtoa("200.400.30.50"));
    UNITTEST_REPORT(STRTOA);
}

void TEST_ATOSTRTOA()
{
    UDPC_Context ctx;

    ASSERT_EQ(0x01020304, UDPC_strtoa(UDPC_INTERNAL_atostr(&ctx, 0x01020304)));
    ASSERT_EQ(0x7F000001, UDPC_strtoa(UDPC_INTERNAL_atostr(&ctx, 0x7F000001)));
    ASSERT_EQ(0xC0A801FF, UDPC_strtoa(UDPC_INTERNAL_atostr(&ctx, 0xC0A801FF)));
    ASSERT_EQ(0xFFFEFDFC, UDPC_strtoa(UDPC_INTERNAL_atostr(&ctx, 0xFFFEFDFC)));

    UNITTEST_REPORT(ATOSTRTOA);
}

int main()
{
    TEST_DEQUE();
    TEST_ATOSTR();
    TEST_STRTOA();
    TEST_ATOSTRTOA();
    TEST_HASHMAP();
    return 0;
}
