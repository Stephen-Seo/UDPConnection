#include "CXX11_shared_spin_lock.hpp"
#include "test_helpers.h"
#include "test_headers.h"

void TEST_CXX11_shared_spin_lock() {
    UDPC::SharedSpinLock::Ptr spinLockPtr = UDPC::SharedSpinLock::newInstance();

    auto readLock = spinLockPtr->spin_read_lock();
    CHECK_TRUE(readLock.isValid());
    CHECK_TRUE(spinLockPtr->spin_read_lock().isValid());
    CHECK_FALSE(spinLockPtr->try_spin_write_lock().isValid());

    auto writeLock = spinLockPtr->trade_read_for_write_lock(readLock);
    CHECK_TRUE(writeLock.isValid());
    CHECK_FALSE(readLock.isValid());
    CHECK_FALSE(spinLockPtr->try_spin_read_lock().isValid());
    CHECK_FALSE(spinLockPtr->try_spin_write_lock().isValid());
}
