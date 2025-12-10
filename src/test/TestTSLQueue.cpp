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
            q.push(i);
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
            CHECK_TRUE(q.push_nb(i));
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
            q.push(i);
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
            q.push(i);
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
            q->push(i);
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

    // Iterator
    {
        TSLQueue<int> q;

        for(int i = 0; i < 10; ++i) {
            q.push(i);
        }
        CHECK_EQ(q.size(), 10);

        {
            // iteration
            auto iter_opt = q.begin_readonly(1);
            auto iter = std::move(iter_opt.value());
            int i = 0;
            auto op = iter.current();
            while(op) {
                CHECK_EQ(*op, i++);
                if(i < 10) {
                    CHECK_TRUE(iter.next());
                } else {
                    CHECK_FALSE(iter.next());
                }
                op = iter.current();
            }

            // test that lock is held by iterator
            CHECK_FALSE(q.push_nb(10));
            op = q.top_nb();
            // Getting top and iterator both hold read locks so this should be true.
            CHECK_TRUE(op);

            // backwards iteration
            CHECK_TRUE(iter.prev());
            op = iter.current();
            while(op) {
                CHECK_EQ(*op, --i);
                if(i > 0) {
                    CHECK_TRUE(iter.prev());
                } else {
                    CHECK_FALSE(iter.prev());
                }
                op = iter.current();
            }
        }

        {
            // iter remove
            {
                auto iter_opt = q.begin(300);
                auto iter = std::move(iter_opt.value());
                CHECK_TRUE(iter.next());
                CHECK_TRUE(iter.next());
                CHECK_TRUE(iter.next());
                CHECK_TRUE(iter.remove());

                auto op = iter.current();
                CHECK_TRUE(op);
                CHECK_EQ(*op, 4);

                CHECK_TRUE(iter.prev());
                op = iter.current();
                CHECK_TRUE(op);
                CHECK_EQ(*op, 2);
            }
            // Drop first iterator, there can only be 1 rw iterator.

            // second iterator, read-only.
            auto iter2 = q.begin_readonly(1);
            CHECK_TRUE(iter2.has_value());

            // Still should be able to get top.
            CHECK_TRUE(iter2.has_value() && iter2->current());

            // third iterator, read-only.
            auto iter3 = q.begin_readonly(1);

            // Still should be able to get top.
            CHECK_TRUE(iter3.has_value() && iter3->current());
        }
        CHECK_EQ(q.size(), 9);

        // check that "3" was removed from queue
        int i = 0;
        std::unique_ptr<int> op;
        while(!q.empty()) {
            op = q.top();
            CHECK_TRUE(op);
            CHECK_EQ(i++, *op);
            if(i == 3) {
                ++i;
            }
            CHECK_TRUE(q.pop());
        }

        // remove from start
        q.push(0);
        q.push(1);
        q.push(2);
        q.push(3);
        CHECK_EQ(q.size(), 4);
        {
            auto iter = q.begin(300);
            CHECK_TRUE(iter.has_value() && iter->remove());
        }
        CHECK_EQ(q.size(), 3);
        i = 1;
        while(!q.empty()) {
            op = q.top();
            CHECK_TRUE(op);
            CHECK_EQ(i++, *op);
            CHECK_TRUE(q.pop());
        }

        // remove from end
        q.push(0);
        q.push(1);
        q.push(2);
        q.push(3);
        CHECK_EQ(q.size(), 4);
        {
            auto iter = q.begin(300);
            CHECK_TRUE(iter.has_value());
            if (iter.has_value()) {
                while(true) {
                    CHECK_TRUE(iter->next());
                    op = iter->current();
                    CHECK_TRUE(op);
                    if(*op == 3) {
                        CHECK_FALSE(iter->remove());
                        break;
                    }
                }
            }
        }
        CHECK_EQ(q.size(), 3);
        i = 0;
        while(!q.empty()) {
            op = q.top();
            CHECK_TRUE(op);
            CHECK_EQ(i++, *op);
            CHECK_TRUE(q.pop());
            if(i == 3) {
                CHECK_TRUE(q.empty());
            }
        }

        // Iterator timeout
        {
            auto write_iter = q.begin(300);
            CHECK_TRUE(write_iter.has_value());
            auto read_iter = q.begin_readonly(1);
            CHECK_FALSE(read_iter.has_value());
        }
        {
            auto read_iter = q.begin_readonly(1);
            CHECK_TRUE(read_iter.has_value());
            auto write_iter = q.begin(300);
            CHECK_FALSE(write_iter.has_value());
            auto read_iter2 = q.begin_readonly(1);
            CHECK_TRUE(read_iter2.has_value());
        }
    }

    // TempToNew
    {
        TSLQueue<int> q;

        q.push(1234);
        q.push(5678);

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
}
