#include <gtest/gtest.h>

#include <future>
#include <functional>

#include "TSLQueue.hpp"

TEST(TSLQueue, PushTopPopSize) {
    TSLQueue<int> q;

    EXPECT_FALSE(q.top().has_value());

    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(i, q.size());
        q.push(i);
    }

    for(int i = 0; i < 10; ++i) {
        auto v = q.top();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(v.value(), i);
        EXPECT_EQ(10 - i, q.size());
        EXPECT_TRUE(q.pop());
    }
    EXPECT_EQ(q.size(), 0);

    EXPECT_FALSE(q.pop());
}

TEST(TSLQueue, PushNB_TopNB_TopAndPop_Size) {
    TSLQueue<int> q;

    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(q.size(), i);
        EXPECT_TRUE(q.push_nb(i));
    }

    for(int i = 0; i < 10; ++i) {
        auto v = q.top_nb();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(v.value(), i);
        EXPECT_EQ(q.size(), 10 - i);
        v = q.top_and_pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(v.value(), i);
    }

    {
        auto v = q.top_nb();
        ASSERT_FALSE(v.has_value());
    }
    {
        auto v = q.top_and_pop();
        ASSERT_FALSE(v.has_value());
    }
    EXPECT_EQ(q.size(), 0);
}

TEST(TSLQueue, Push_TopAndPopAndEmpty_Size) {
    TSLQueue<int> q;

    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(q.size(), i);
        q.push(i);
    }

    bool isEmpty;
    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(q.size(), 10 - i);
        auto v = q.top_and_pop_and_empty(&isEmpty);
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(v.value(), i);
        EXPECT_EQ(i == 9, isEmpty);
    }
    EXPECT_EQ(q.size(), 0);
}

TEST(TSLQueue, PushClearEmptySize) {
    TSLQueue<int> q;

    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(q.size(), i);
        q.push(i);
    }
    EXPECT_EQ(q.size(), 10);

    EXPECT_FALSE(q.empty());
    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0);
}

TEST(TSLQueue, Concurrent) {
    TSLQueue<int> q;

    const auto add_fn = [] (TSLQueue<int> *q, int i) -> void {
        q->push(i);
    };

    std::future<void> futures[100];
    for(int i = 0; i < 100; ++i) {
        futures[i] = std::async(std::launch::async, add_fn, &q, i);
    }
    for(int i = 0; i < 100; ++i) {
        futures[i].wait();
    }

    EXPECT_FALSE(q.empty());
    for(int i = 0; i < 100; ++i) {
        EXPECT_EQ(q.size(), 100 - i);
        auto v = q.top_and_pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_GE(v.value(), 0);
        EXPECT_LE(v.value(), 100);
        EXPECT_EQ(i == 99, q.empty());
    }
    EXPECT_EQ(q.size(), 0);
}
