#ifndef UDPC_UNIT_TEST_H
#define UDPC_UNIT_TEST_H

#include <stdio.h>
#include <string.h>

/*
#include <UDPC_Deque.h>
*/

#define ASSERT_TRUE(x) \
    if(!x) { printf("%d: ASSERT_TRUE(%s) FAILED\n", __LINE__, #x); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_FALSE(x) \
    if(x) { printf("%d: ASSERT_FALSE(%s) FAILED\n", __LINE__, #x); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_EQ(x, y) \
    if(x != y) { printf("%d: ASSERT_EQ(%s, %s) FAILED\n", __LINE__, #x, #y); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_NEQ(x, y) \
    if(x == y) { printf("%d: ASSERT_NEQ(%s, %s) FAILED\n", __LINE__, #x, #y); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_EQ_MEM(x, y, size) \
    if(memcmp(x, y, size) != 0) { printf("%d: ASSERT_EQ_MEM(%s, %s, %s) FAILED\n", \
            __LINE__, #x, #y, #size); ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_NEQ_MEM(x, y, size) \
    if(memcmp(x, y, size) == 0) { printf("%d: ASSERT_NEQ_MEM(%s, %s, %s) FAILED\n", \
            __LINE__, #x, #y, #size); ++UDPC_uts.failed; } ++UDPC_uts.total;

#define UNITTEST_REPORT() printf("%d/%d tests failed\n", UDPC_uts.failed, UDPC_uts.total);


typedef struct
{
    int result;
} UnitTest_Test;

typedef struct
{
    int failed;
    int total;
} UnitTestState;

static UnitTestState UDPC_uts;

#endif
