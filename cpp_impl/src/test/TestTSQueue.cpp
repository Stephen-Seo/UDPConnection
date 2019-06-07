#include <gtest/gtest.h>

#include "TSQueue.hpp"

TEST(TSQueue, Usage)
{
    TSQueue q(sizeof(int), 4);
    int temp = 100;

    EXPECT_EQ(q.size(), 0);
    EXPECT_FALSE(q.pop());

    EXPECT_TRUE(q.push(&temp));
    EXPECT_EQ(q.size(), 1);

    // { 100 }

    temp = 200;
    EXPECT_TRUE(q.push(&temp));
    EXPECT_EQ(q.size(), 2);
    auto top = q.top();
    EXPECT_EQ(100, *((int*)top.get()));

    // { 100, 200 }

    temp = 300;
    EXPECT_TRUE(q.push(&temp));
    EXPECT_EQ(q.size(), 3);

    // { 100, 200, 300 }

    temp = 400;
    EXPECT_TRUE(q.push(&temp));
    EXPECT_EQ(q.size(), 4);

    // { 100, 200, 300, 400 }

    temp = 500;
    EXPECT_FALSE(q.push(&temp));
    EXPECT_EQ(q.size(), 4);

    top = q.top();
    EXPECT_EQ(100, *((int*)top.get()));

    EXPECT_TRUE(q.pop());
    EXPECT_EQ(q.size(), 3);

    // { 200, 300, 400 }

    top = q.top();
    EXPECT_EQ(200, *((int*)top.get()));

    temp = 1;
    EXPECT_TRUE(q.push(&temp));
    EXPECT_EQ(q.size(), 4);

     // { 200, 300, 400, 1 }

    top = q.top();
    EXPECT_EQ(200, *((int*)top.get()));

    temp = 2;
    EXPECT_FALSE(q.push(&temp));
    EXPECT_EQ(q.size(), 4);

    q.changeCapacity(8);
    EXPECT_EQ(q.size(), 4);

    temp = 10;
    EXPECT_TRUE(q.push(&temp));
    EXPECT_EQ(q.size(), 5);

    // { 200, 300, 400, 1, 10 }

    top = q.top();
    EXPECT_EQ(200, *((int*)top.get()));

    EXPECT_TRUE(q.pop());
    EXPECT_EQ(q.size(), 4);

    // { 300, 400, 1, 10 }

    top = q.top();
    EXPECT_EQ(300, *((int*)top.get()));

    EXPECT_TRUE(q.pop());
    EXPECT_EQ(q.size(), 3);

    // { 400, 1, 10 }

    top = q.top();
    EXPECT_EQ(400, *((int*)top.get()));

    q.changeCapacity(1);

    // { 10 }

    EXPECT_EQ(q.size(), 1);

    top = q.top();
    EXPECT_EQ(10, *((int*)top.get()));

    EXPECT_TRUE(q.pop());

    // { }

    EXPECT_FALSE(q.pop());
    EXPECT_EQ(0, q.size());
}
