#include <gtest/gtest.h>

#include <future>
#include <functional>

#include "TSLQueue.hpp"

TEST(TSLQueue, PushTopPopSize) {
    TSLQueue<int> q;

    EXPECT_FALSE(q.top());

    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(i, q.size());
        q.push(i);
    }

    for(int i = 0; i < 10; ++i) {
        auto v = q.top();
        ASSERT_TRUE(v);
        EXPECT_EQ(*v, i);
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
        ASSERT_TRUE(v);
        EXPECT_EQ(*v, i);
        EXPECT_EQ(q.size(), 10 - i);
        v = q.top_and_pop();
        ASSERT_TRUE(v);
        EXPECT_EQ(*v, i);
    }

    {
        auto v = q.top_nb();
        ASSERT_FALSE(v);
    }
    {
        auto v = q.top_and_pop();
        ASSERT_FALSE(v);
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
        ASSERT_TRUE(v);
        EXPECT_EQ(*v, i);
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
        ASSERT_TRUE(v);
        EXPECT_GE(*v, 0);
        EXPECT_LE(*v, 100);
        EXPECT_EQ(i == 99, q.empty());
    }
    EXPECT_EQ(q.size(), 0);
}

TEST(TSLQueue, Iterator) {
    TSLQueue<int> q;

    for(int i = 0; i < 10; ++i) {
        q.push(i);
    }
    EXPECT_EQ(q.size(), 10);

    {
        // iteration
        auto iter = q.begin();
        int i = 0;
        auto op = iter.current();
        while(op) {
            EXPECT_EQ(*op, i++);
            if(i < 10) {
                EXPECT_TRUE(iter.next());
            } else {
                EXPECT_FALSE(iter.next());
            }
            op = iter.current();
        }

        // test that lock is held by iterator
        EXPECT_FALSE(q.push_nb(10));
        op = q.top_nb();
        EXPECT_FALSE(op);

        // backwards iteration
        EXPECT_TRUE(iter.prev());
        op = iter.current();
        while(op) {
            EXPECT_EQ(*op, --i);
            if(i > 0) {
                EXPECT_TRUE(iter.prev());
            } else {
                EXPECT_FALSE(iter.prev());
            }
            op = iter.current();
        }
    }

    {
        // iter remove
        auto iter = q.begin();
        EXPECT_TRUE(iter.next());
        EXPECT_TRUE(iter.next());
        EXPECT_TRUE(iter.next());
        EXPECT_TRUE(iter.remove());

        auto op = iter.current();
        EXPECT_TRUE(op);
        EXPECT_EQ(*op, 4);

        EXPECT_TRUE(iter.prev());
        op = iter.current();
        EXPECT_TRUE(op);
        EXPECT_EQ(*op, 2);
    }
    EXPECT_EQ(q.size(), 9);

    // check that "3" was removed from queue
    int i = 0;
    std::unique_ptr<int> op;
    while(!q.empty()) {
        op = q.top();
        EXPECT_TRUE(op);
        EXPECT_EQ(i++, *op);
        if(i == 3) {
            ++i;
        }
        EXPECT_TRUE(q.pop());
    }

    // remove from start
    q.push(0);
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.size(), 4);
    {
        auto iter = q.begin();
        EXPECT_TRUE(iter.remove());
    }
    EXPECT_EQ(q.size(), 3);
    i = 1;
    while(!q.empty()) {
        op = q.top();
        EXPECT_TRUE(op);
        EXPECT_EQ(i++, *op);
        EXPECT_TRUE(q.pop());
    }

    // remove from end
    q.push(0);
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.size(), 4);
    {
        auto iter = q.begin();
        while(true) {
            EXPECT_TRUE(iter.next());
            op = iter.current();
            EXPECT_TRUE(op);
            if(*op == 3) {
                EXPECT_FALSE(iter.remove());
                break;
            }
        }
    }
    EXPECT_EQ(q.size(), 3);
    i = 0;
    while(!q.empty()) {
        op = q.top();
        EXPECT_TRUE(op);
        EXPECT_EQ(i++, *op);
        EXPECT_TRUE(q.pop());
        if(i == 3) {
            EXPECT_TRUE(q.empty());
        }
    }
}

TEST(TSLQueue, TempToNew) {
    TSLQueue<int> q;

    q.push(1234);
    q.push(5678);

    auto getValue = [] (TSLQueue<int> *q) -> int {
        auto uptr = q->top_and_pop();
        return *uptr;
    };
    int value;

    value = getValue(&q);
    EXPECT_EQ(1234, value);
    value = getValue(&q);
    EXPECT_EQ(5678, value);
}
