#include "UDPC_UnitTest.h"

#include <stdlib.h>
#include <UDPC_Deque.h>
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
    UDPC_Context *ctx = malloc(sizeof(UDPC_Context));
    ASSERT_EQ_MEM(
        UDPC_INTERNAL_atostr(ctx, (0xAC << 24) | (0x1E << 16) | (0x1 << 8) | 0xFF),
        "172.30.1.255",
        13);
    free(ctx);
    UNITTEST_REPORT(ATOSTR);
}

int main()
{
    TEST_DEQUE();
    TEST_ATOSTR();
    return 0;
}
