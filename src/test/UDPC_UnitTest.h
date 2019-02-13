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
#define ASSERT_GT(x, y) \
    if(x <= y) { printf("%d: ASSERT_GT(%s, %s) FAILED\n", __LINE__, #x, #y); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_GTE(x, y) \
    if(x < y) { printf("%d: ASSERT_GTE(%s, %s) FAILED\n", __LINE__, #x, #y); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_LT(x, y) \
    if(x >= y) { printf("%d: ASSERT_LT(%s, %s) FAILED\n", __LINE__, #x, #y); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;
#define ASSERT_LTE(x, y) \
    if(x > y) { printf("%d: ASSERT_LTE(%s, %s) FAILED\n", __LINE__, #x, #y); \
        ++UDPC_uts.failed; } ++UDPC_uts.total;

#define UNITTEST_REPORT(x) { \
    printf("%s: %d/%d tests failed\n", #x, UDPC_uts.failed, UDPC_uts.total); \
    UDPC_uts.failed = 0; \
    UDPC_uts.total = 0; }


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
