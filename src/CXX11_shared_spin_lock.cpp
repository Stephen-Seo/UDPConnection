#include "CXX11_shared_spin_lock.hpp"

UDPC::Badge UDPC::Badge::newInvalid() {
    Badge badge;
    badge.isValid = false;
    return badge;
}

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
        if (!write) {
            ++read;
            return LockObj<false>(selfWeakPtr, Badge{});
        }
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::try_spin_read_lock() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!write) {
        ++read;
        return LockObj<false>(selfWeakPtr, Badge{});
    }
    return LockObj<false>(Badge{});
}

void UDPC::SharedSpinLock::read_unlock(UDPC::Badge &&badge) {
    if (badge.isValid) {
        std::lock_guard<std::mutex> lock(mutex);
        if (read > 0) {
            --read;
            badge.isValid = false;
        }
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::spin_write_lock() {
    while (true) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!write && read == 0) {
            write = true;
            return LockObj<true>(selfWeakPtr, Badge{});
        }
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::try_spin_write_lock() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!write && read == 0) {
        write = true;
        return LockObj<true>(selfWeakPtr, Badge{});
    }
    return LockObj<true>(Badge{});
}

void UDPC::SharedSpinLock::write_unlock(UDPC::Badge &&badge) {
    if (badge.isValid) {
        std::lock_guard<std::mutex> lock(mutex);
        write = false;
        badge.isValid = false;
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::trade_write_for_read_lock(UDPC::LockObj<true> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        while (true) {
            std::lock_guard<std::mutex> lock(mutex);
            if (write && read == 0) {
                read = 1;
                write = false;
                lockObj.isLocked = false;
                lockObj.badge.isValid = false;
                return LockObj<false>(selfWeakPtr, Badge{});
            }
        }
    } else {
        return LockObj<false>(Badge{});
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::try_trade_write_for_read_lock(UDPC::LockObj<true> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        std::lock_guard<std::mutex> lock(mutex);
        if (write && read == 0) {
            read = 1;
            write = false;
            lockObj.isLocked = false;
            lockObj.badge.isValid = false;
            return LockObj<false>(selfWeakPtr, Badge{});
        }
    }
    return LockObj<false>(Badge{});
}

UDPC::LockObj<true> UDPC::SharedSpinLock::trade_read_for_write_lock(UDPC::LockObj<false> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        while (true) {
            std::lock_guard<std::mutex> lock(mutex);
            if (!write && read == 1) {
                read = 0;
                write = true;
                lockObj.isLocked = false;
                lockObj.badge.isValid = false;
                return LockObj<true>(selfWeakPtr, Badge{});
            }
        }
    } else {
        return LockObj<true>(Badge{});
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::try_trade_read_for_write_lock(UDPC::LockObj<false> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!write && read == 1) {
            read = 0;
            write = true;
            lockObj.isLocked = false;
            lockObj.badge.isValid = false;
            return LockObj<true>(selfWeakPtr, Badge{});
        }
    }
    return LockObj<true>(Badge{});
}
