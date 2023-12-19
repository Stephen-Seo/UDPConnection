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
spinLock(false),
read(0),
write(false)
{}

UDPC::LockObj<false> UDPC::SharedSpinLock::spin_read_lock() {
    bool expected;
    while (true) {
        expected = false;
        if(spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            if (!write) {
                ++read;
                spinLock.store(false, std::memory_order_release);
                return LockObj<false>(selfWeakPtr, Badge{});
            } else {
                spinLock.store(false, std::memory_order_release);
            }
        }
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::try_spin_read_lock() {
    bool expected;
    while (true) {
        expected = false;
        if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            if (!write) {
                ++read;
                spinLock.store(false, std::memory_order_release);
                return LockObj<false>(selfWeakPtr, Badge{});
            } else {
                spinLock.store(false, std::memory_order_release);
                break;
            }
        }
    }
    return LockObj<false>{};
}

void UDPC::SharedSpinLock::read_unlock(UDPC::Badge &&badge) {
    if (badge.isValid) {
        bool expected;
        while (true) {
            expected = false;
            if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                if (read > 0) {
                    --read;
                    badge.isValid = false;
                }
                spinLock.store(false, std::memory_order_release);
                break;
            }
        }
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::spin_write_lock() {
    bool expected;
    while (true) {
        expected = false;
        if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            if (!write && read == 0) {
                write = true;
                spinLock.store(false, std::memory_order_release);
                return LockObj<true>(selfWeakPtr, Badge{});
            } else {
                spinLock.store(false, std::memory_order_release);
            }
        }
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::try_spin_write_lock() {
    bool expected;
    while (true) {
        expected = false;
        if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            if (!write && read == 0) {
                write = true;
                spinLock.store(false, std::memory_order_release);
                return LockObj<true>(selfWeakPtr, Badge{});
            } else {
                spinLock.store(false, std::memory_order_release);
                break;
            }
        }
    }
    return LockObj<true>{};
}

void UDPC::SharedSpinLock::write_unlock(UDPC::Badge &&badge) {
    if (badge.isValid) {
        bool expected;
        while(true) {
            expected = false;
            if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                if (write) {
                    write = false;
                    badge.isValid = false;
                }
                spinLock.store(false, std::memory_order_release);
                break;
            }
        }
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::trade_write_for_read_lock(UDPC::LockObj<true> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        bool expected;
        while (true) {
            expected = false;
            if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                if (write && read == 0) {
                    read = 1;
                    write = false;
                    lockObj.isLocked = false;
                    lockObj.badge.isValid = false;
                    spinLock.store(false, std::memory_order_release);
                    return LockObj<false>(selfWeakPtr, Badge{});
                } else {
                    spinLock.store(false, std::memory_order_release);
                }
            }
        }
    } else {
        return LockObj<false>{};
    }
}

UDPC::LockObj<false> UDPC::SharedSpinLock::try_trade_write_for_read_lock(UDPC::LockObj<true> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        bool expected;
        while (true) {
            expected = false;
            if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                if (write && read == 0) {
                    read = 1;
                    write = false;
                    lockObj.isLocked = false;
                    lockObj.badge.isValid = false;
                    spinLock.store(false, std::memory_order_release);
                    return LockObj<false>(selfWeakPtr, Badge{});
                } else {
                    spinLock.store(false, std::memory_order_release);
                    break;
                }
            }
        }
    }
    return LockObj<false>{};
}

UDPC::LockObj<true> UDPC::SharedSpinLock::trade_read_for_write_lock(UDPC::LockObj<false> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        bool expected;
        while (true) {
            expected = false;
            if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                if (!write && read == 1) {
                    read = 0;
                    write = true;
                    lockObj.isLocked = false;
                    lockObj.badge.isValid = false;
                    spinLock.store(false, std::memory_order_release);
                    return LockObj<true>(selfWeakPtr, Badge{});
                } else {
                    spinLock.store(false, std::memory_order_release);
                }
            }
        }
    } else {
        return LockObj<true>{};
    }
}

UDPC::LockObj<true> UDPC::SharedSpinLock::try_trade_read_for_write_lock(UDPC::LockObj<false> &lockObj) {
    if (lockObj.isValid() && lockObj.badge.isValid) {
        bool expected;
        while (true) {
            expected = false;
            if (spinLock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                if (!write && read == 1) {
                    read = 0;
                    write = true;
                    lockObj.isLocked = false;
                    lockObj.badge.isValid = false;
                    spinLock.store(false, std::memory_order_release);
                    return LockObj<true>(selfWeakPtr, Badge{});
                } else {
                    spinLock.store(false, std::memory_order_release);
                    break;
                }
            }
        }
    }
    return LockObj<true>{};
}
