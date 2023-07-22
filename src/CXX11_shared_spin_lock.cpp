#include "CXX11_shared_spin_lock.hpp"

UDPC::Badge::Badge() :
isValid(true)
{}

UDPC::SharedSpinLock::Ptr UDPC::SharedSpinLock::newInstance() {
    Ptr sharedSpinLock = Ptr(new SharedSpinLock());
    sharedSpinLock->selfWeakPtr = sharedSpinLock;
    return sharedSpinLock;
}

UDPC::SharedSpinLock::SharedSpinLock() :
selfWeakPtr(),
mutex(),
read(0),
write(false)
{}

UDPC::LockObj<false> UDPC::SharedSpinLock::spin_read_lock() {
    while (true) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!write.load()) {
            ++read;
            return LockObj<false>(selfWeakPtr, Badge{});
        }
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::try_spin_read_lock() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!write.load()) {
        ++read;
        return LockObj<false>(selfWeakPtr, Badge{});
    }
    return LockObj<false>(Badge{});
}

void UDPC::SharedSpinLock::read_unlock(UDPC::Badge &&badge) {
    if (badge.isValid) {
        std::lock_guard<std::mutex> lock(mutex);
        if (read.load() > 0) {
            --read;
            badge.isValid = false;
        }
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::spin_write_lock() {
    while (true) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!write.load() && read.load() == 0) {
            write.store(true);
            return LockObj<true>(selfWeakPtr, Badge{});
        }
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::try_spin_write_lock() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!write.load() && read.load() == 0) {
        write.store(true);
        return LockObj<true>(selfWeakPtr, Badge{});
    }
    return LockObj<true>(Badge{});
}

void UDPC::SharedSpinLock::write_unlock(UDPC::Badge &&badge) {
    if (badge.isValid) {
        std::lock_guard<std::mutex> lock(mutex);
        write.store(false);
        badge.isValid = false;
    }
}
