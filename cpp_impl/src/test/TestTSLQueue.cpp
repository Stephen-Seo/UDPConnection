#include <gtest/gtest.h>

#include <iostream>

#include "TSLQueue.hpp"

TEST(TSLQueue, Usage) {
    TSLQueue<int> q;
    bool isEmpty;
    std::optional<int> opt;

    // init
    EXPECT_FALSE(q.pop());

    opt = q.top_and_pop();
    EXPECT_FALSE(opt.has_value());

    opt = q.top_and_pop_and_empty(&isEmpty);
    EXPECT_FALSE(opt.has_value());
    EXPECT_TRUE(isEmpty);

    EXPECT_TRUE(q.empty());

    // push 1, 2, 3
    q.push(1);
    EXPECT_FALSE(q.empty());
    opt = q.top();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 1);

    q.push_nb(2);
    EXPECT_FALSE(q.empty());
    opt = q.top_nb();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 1);

    q.push(3);
    EXPECT_FALSE(q.empty());
    opt = q.top();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 1);

    // iterators
    {
        auto citer = q.citer();
        opt = citer.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 1);
        EXPECT_FALSE(citer.set(111));

        EXPECT_TRUE(citer.next());
        opt = citer.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 2);

        EXPECT_TRUE(citer.next());
        opt = citer.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 3);

        EXPECT_FALSE(citer.next());
        opt = citer.current();
        EXPECT_FALSE(opt.has_value());

        EXPECT_TRUE(citer.isValid());
        EXPECT_FALSE(citer.next());
        EXPECT_FALSE(citer.isValid());
    }
    {
        auto criter = q.criter();
        opt = criter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 3);
        EXPECT_FALSE(criter.set(333));

        EXPECT_TRUE(criter.next());
        opt = criter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 2);

        EXPECT_TRUE(criter.next());
        opt = criter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 1);

        EXPECT_FALSE(criter.next());
        opt = criter.current();
        EXPECT_FALSE(opt.has_value());
    }
    {
        // values changed to 10, 20, 30
        auto iter = q.iter();
        opt = iter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 1);
        EXPECT_TRUE(iter.set(10));
        opt = iter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 10);

        EXPECT_TRUE(iter.next());
        opt = iter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 2);
        EXPECT_TRUE(iter.set(20));
        opt = iter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 20);

        EXPECT_TRUE(iter.next());
        opt = iter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 3);
        EXPECT_TRUE(iter.set(30));
        opt = iter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 30);

        EXPECT_FALSE(iter.next());
        opt = iter.current();
        EXPECT_FALSE(opt.has_value());
    }
    {
        // values changed to 1, 2, 3
        auto riter = q.riter();
        opt = riter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 30);
        EXPECT_TRUE(riter.set(3));
        opt = riter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 3);

        EXPECT_TRUE(riter.next());
        opt = riter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 20);
        EXPECT_TRUE(riter.set(2));
        opt = riter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 2);

        EXPECT_TRUE(riter.next());
        opt = riter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 10);
        EXPECT_TRUE(riter.set(1));
        opt = riter.current();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(opt.value(), 1);
    }
}
