#ifndef UDPC_CXX11_SHARED_SPIN_LOCK_H_
#define UDPC_CXX11_SHARED_SPIN_LOCK_H_

#include <memory>
#include <mutex>
#include <atomic>

namespace UDPC {

// Forward declaration for LockObj.
class SharedSpinLock;

class Badge {
public:
    // Disallow copy.
    Badge(const Badge&) = delete;
    Badge& operator=(const Badge&) = delete;

    // Allow move.
    Badge(Badge&&) = default;
    Badge& operator=(Badge&&) = default;

private:
    friend class SharedSpinLock;

    // Can only be created by SharedSpinLock.
    Badge();

    bool isValid;
};

template <bool IsWriteObj>
class LockObj {
public:
    ~LockObj();

    // Disallow copy.
    LockObj(const LockObj&) = delete;
    LockObj& operator=(const LockObj&) = delete;

    // Allow move.
    LockObj(LockObj&&) = default;
    LockObj& operator=(LockObj&&) = default;

    bool isValid() const;

private:
    friend class SharedSpinLock;

    // Only can be created by SharedSpinLock.
    LockObj(Badge &&badge);
    LockObj(std::weak_ptr<SharedSpinLock> lockPtr, Badge &&badge);

    std::weak_ptr<SharedSpinLock> weakPtrLock;
    bool isLocked;
    Badge badge;
};

class SharedSpinLock {
public:
    using Ptr = std::shared_ptr<SharedSpinLock>;
    using Weak = std::weak_ptr<SharedSpinLock>;

    static Ptr newInstance();

    // Disallow copy.
    SharedSpinLock(const SharedSpinLock&) = delete;
    SharedSpinLock& operator=(const SharedSpinLock&) = delete;

    // Allow move.
    SharedSpinLock(SharedSpinLock&&) = default;
    SharedSpinLock& operator=(SharedSpinLock&&) = default;

    LockObj<false> spin_read_lock();
    LockObj<false> try_spin_read_lock();
    void read_unlock(Badge&&);

    LockObj<true> spin_write_lock();
    LockObj<true> try_spin_write_lock();
    void write_unlock(Badge&&);

private:
    SharedSpinLock();

    Weak selfWeakPtr;
    std::mutex mutex;
    unsigned int read;
    bool write;

};

template <bool IsWriteObj>
LockObj<IsWriteObj>::LockObj(Badge &&badge) :
weakPtrLock(),
isLocked(false),
badge(std::forward<Badge>(badge))
{}

template <bool IsWriteObj>
LockObj<IsWriteObj>::LockObj(SharedSpinLock::Weak lockPtr, Badge &&badge) :
weakPtrLock(lockPtr),
isLocked(true),
badge(std::forward<Badge>(badge))
{}

template <bool IsWriteObj>
LockObj<IsWriteObj>::~LockObj() {
    if (!isLocked) {
        return;
    }
    auto strongPtrLock = weakPtrLock.lock();
    if (strongPtrLock) {
        if (IsWriteObj) {
            strongPtrLock->write_unlock(std::move(badge));
        } else {
            strongPtrLock->read_unlock(std::move(badge));
        }
    }
}

template <bool IsWriteObj>
bool LockObj<IsWriteObj>::isValid() const {
    return isLocked;
}

} // namespace UDPC

#endif
