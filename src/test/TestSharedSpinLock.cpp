#include <gtest/gtest.h>

#include "CXX11_shared_spin_lock.hpp"

TEST(CXX11_shared_spin_lock, simple) {
    UDPC::SharedSpinLock::Ptr spinLockPtr = UDPC::SharedSpinLock::newInstance();

    auto readLock = spinLockPtr->spin_read_lock();
    EXPECT_TRUE(readLock.isValid());
    EXPECT_TRUE(spinLockPtr->spin_read_lock().isValid());
    EXPECT_FALSE(spinLockPtr->try_spin_write_lock().isValid());

    auto writeLock = spinLockPtr->trade_read_for_write_lock(readLock);
    EXPECT_TRUE(writeLock.isValid());
    EXPECT_FALSE(readLock.isValid());
    EXPECT_FALSE(spinLockPtr->try_spin_read_lock().isValid());
    EXPECT_FALSE(spinLockPtr->try_spin_write_lock().isValid());
}
