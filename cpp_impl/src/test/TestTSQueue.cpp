#include <gtest/gtest.h>

#include <cstdio>
#include <future>

#include "TSQueue.hpp"

TEST(TSQueue, Usage)
{
    TSQueue<int> q(4);
    int temp = 100;

    EXPECT_EQ(q.size(), 0);
    EXPECT_FALSE(q.pop());

    EXPECT_TRUE(q.push(temp));
    EXPECT_EQ(q.size(), 1);

    // { 100 }

    temp = 200;
    EXPECT_TRUE(q.push(temp));
    EXPECT_EQ(q.size(), 2);
    EXPECT_EQ(100, q.top());

    // { 100, 200 }

    temp = 300;
    EXPECT_TRUE(q.push(temp));
    EXPECT_EQ(q.size(), 3);

    // { 100, 200, 300 }

    temp = 400;
    EXPECT_TRUE(q.push(temp));
    EXPECT_EQ(q.size(), 4);

    // { 100, 200, 300, 400 }

    temp = 500;
    EXPECT_FALSE(q.push(temp));
    EXPECT_EQ(q.size(), 4);

    EXPECT_EQ(100, q.top());

    EXPECT_TRUE(q.pop());
    EXPECT_EQ(q.size(), 3);

    // { 200, 300, 400 }

    EXPECT_EQ(200, q.top());

    temp = 1;
    EXPECT_TRUE(q.push(temp));
    EXPECT_EQ(q.size(), 4);

     // { 200, 300, 400, 1 }

    EXPECT_EQ(200, q.top());

    temp = 2;
    EXPECT_FALSE(q.push(temp));
    EXPECT_EQ(q.size(), 4);

    q.changeCapacity(8, nullptr);
    EXPECT_EQ(q.size(), 4);

    temp = 10;
    EXPECT_TRUE(q.push(temp));
    EXPECT_EQ(q.size(), 5);

    // { 200, 300, 400, 1, 10 }

    EXPECT_EQ(200, q.top());

    EXPECT_TRUE(q.pop());
    EXPECT_EQ(q.size(), 4);

    // { 300, 400, 1, 10 }

    EXPECT_EQ(300, q.top());

    EXPECT_TRUE(q.pop());
    EXPECT_EQ(q.size(), 3);

    // { 400, 1, 10 }

    EXPECT_EQ(400, q.top());

    q.changeCapacity(1, nullptr);

    // { 10 }

    EXPECT_EQ(q.size(), 1);

    EXPECT_EQ(10, q.top());

    EXPECT_TRUE(q.pop());

    // { }

    EXPECT_FALSE(q.pop());
    EXPECT_EQ(0, q.size());
}

TEST(TSQueue, Concurrent)
{
    TSQueue<int> q(4);

    auto a0 = std::async(std::launch::async, [&q] () {int i = 0; return q.push(i); });
    auto a1 = std::async(std::launch::async, [&q] () {int i = 1; return q.push(i); });
    auto a2 = std::async(std::launch::async, [&q] () {int i = 2; return q.push(i); });
    auto a3 = std::async(std::launch::async, [&q] () {int i = 3; return q.push(i); });
    auto a4 = std::async(std::launch::async, [&q] () {int i = 4; return q.push(i); });

    bool results[] = {
        a0.get(),
        a1.get(),
        a2.get(),
        a3.get(),
        a4.get()
    };

    int insertCount = 0;
    for(int i = 0; i < 5; ++i) {
        if(results[i]) {
            ++insertCount;
        }
    }

    EXPECT_EQ(insertCount, 4);
    EXPECT_EQ(q.size(), 4);

    int top;
    for(int i = 0; i < 4; ++i) {
        top = q.top().value();
        EXPECT_TRUE(q.pop());
        EXPECT_EQ(q.size(), 3 - i);
        printf("%d ", top);
    }
    printf("\n");

    EXPECT_FALSE(q.pop());
    EXPECT_EQ(q.size(), 0);
}
