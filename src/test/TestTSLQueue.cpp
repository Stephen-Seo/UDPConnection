#include "test_helpers.h"
#include "test_headers.h"

#include <future>

#include "TSLQueue.hpp"

void TEST_TSLQueue() {
    // PushTopPopSize
    {
        TSLQueue<int> q;

        CHECK_FALSE(q.top());

        for(int i = 0; i < 10; ++i) {
            CHECK_EQ(i, q.size());
            q.push_back(i);
        }

        for(int i = 0; i < 10; ++i) {
            auto v = q.top();
            ASSERT_TRUE(v);
            CHECK_EQ(*v, i);
            CHECK_EQ(10 - i, q.size());
            CHECK_TRUE(q.pop());
        }
        CHECK_EQ(q.size(), 0);

        CHECK_FALSE(q.pop());
    }

    // PushNB_TopNB_TopAndPop_Size
    {
        TSLQueue<int> q;

        for(int i = 0; i < 10; ++i) {
            CHECK_EQ(q.size(), i);
            CHECK_TRUE(q.push_back_nb(i));
        }

        for(int i = 0; i < 10; ++i) {
            auto v = q.top_nb();
            ASSERT_TRUE(v);
            CHECK_EQ(*v, i);
            CHECK_EQ(q.size(), 10 - i);
            v = q.top_and_pop();
            ASSERT_TRUE(v);
            CHECK_EQ(*v, i);
        }

        {
            auto v = q.top_nb();
            ASSERT_FALSE(v);
        }
        {
            auto v = q.top_and_pop();
            ASSERT_FALSE(v);
        }
        CHECK_EQ(q.size(), 0);
    }

    // Push_TopAndPopAndEmpty_Size
    {
        TSLQueue<int> q;

        for(int i = 0; i < 10; ++i) {
            CHECK_EQ(q.size(), i);
            q.push_back(i);
        }

        bool isEmpty;
        for(int i = 0; i < 10; ++i) {
            CHECK_EQ(q.size(), 10 - i);
            auto v = q.top_and_pop_and_empty(&isEmpty);
            ASSERT_TRUE(v);
            CHECK_EQ(*v, i);
            CHECK_EQ(i == 9, isEmpty);
        }
        CHECK_EQ(q.size(), 0);
    }

    // PushClearEmptySize
    {
        TSLQueue<int> q;

        for(int i = 0; i < 10; ++i) {
            CHECK_EQ(q.size(), i);
            q.push_back(i);
        }
        CHECK_EQ(q.size(), 10);

        CHECK_FALSE(q.empty());
        q.clear();
        CHECK_TRUE(q.empty());
        CHECK_EQ(q.size(), 0);
    }

    // Concurrent
    {
        TSLQueue<int> q;

        const auto add_fn = [] (TSLQueue<int> *q, int i) -> void {
            q->push_back(i);
        };

        std::future<void> futures[100];
        for(int i = 0; i < 100; ++i) {
            futures[i] = std::async(std::launch::async, add_fn, &q, i);
        }
        for(int i = 0; i < 100; ++i) {
            futures[i].wait();
        }

        CHECK_FALSE(q.empty());
        for(int i = 0; i < 100; ++i) {
            CHECK_EQ(q.size(), 100 - i);
            auto v = q.top_and_pop();
            ASSERT_TRUE(v);
            CHECK_GE(*v, 0);
            CHECK_LE(*v, 100);
            CHECK_EQ(i == 99, q.empty());
        }
        CHECK_EQ(q.size(), 0);
    }

    // TempToNew
    {
        TSLQueue<int> q;

        q.push_back(1234);
        q.push_back(5678);

        auto getValue = [] (TSLQueue<int> *q) -> int {
            auto uptr = q->top_and_pop();
            return *uptr;
        };
        int value;

        value = getValue(&q);
        CHECK_EQ(1234, value);
        value = getValue(&q);
        CHECK_EQ(5678, value);
    }

    // bot
    {
        TSLQueue<int> q;

        q.push_back(1);
        q.push_back(2);
        q.push_back(3);
        q.push_back(4);
        q.push_back(5);
        q.push_back(6);

        CHECK_EQ(*q.bot(), 6);
        {
            auto ret = q.bot_nb();
            CHECK_EQ(static_cast<bool>(ret), true);
            if (ret) {
                CHECK_EQ(*ret, 6);
            }
        }

        CHECK_EQ(q.pop_bot(), true);
        {
            auto ret = q.bot_and_pop();
            CHECK_EQ(static_cast<bool>(ret), true);
            if (ret) {
                CHECK_EQ(*ret, 5);
            }
        }
        {
            bool empty = false;
            auto ret = q.bot_and_pop_and_empty(&empty);
            CHECK_EQ(empty, false);
            CHECK_EQ(static_cast<bool>(ret), true);
            if (ret) {
                CHECK_EQ(*ret, 4);
            }
        }
        {
            unsigned long size = 0;
            auto ret = q.bot_and_pop_and_rsize(&size);
            CHECK_EQ(size, 2);
            CHECK_EQ(static_cast<bool>(ret), true);
            if (ret) {
                CHECK_EQ(*ret, 3);
            }
        }
    }
}
