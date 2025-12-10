#ifndef UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP
#define UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <optional>
#include <cassert>

#include "CXX11_shared_spin_lock.hpp"

template <typename T>
class TSLQueue {
  public:
    TSLQueue();
    ~TSLQueue();

    // disable copy
    TSLQueue(const TSLQueue &other) = delete;
    TSLQueue &operator=(const TSLQueue &other) = delete;
    // enable move
    TSLQueue(TSLQueue &&other);
    TSLQueue &operator=(TSLQueue &&other);

    void push(const T &data);
    bool push_nb(const T &data);
    std::unique_ptr<T> top();
    std::unique_ptr<T> top_nb();
    bool pop();
    std::unique_ptr<T> top_and_pop();
    std::unique_ptr<T> top_and_pop_and_empty(bool *isEmpty);
    std::unique_ptr<T> top_and_pop_and_rsize(unsigned long *rsize);
    void clear();

    bool empty();
    unsigned long size();

  private:
    struct TSLQNode {
        TSLQNode();
        // disable copy
        TSLQNode(TSLQNode& other) = delete;
        TSLQNode& operator=(TSLQNode& other) = delete;
        // enable move
        TSLQNode(TSLQNode&& other) = default;
        TSLQNode& operator=(TSLQNode&& other) = default;

        std::shared_ptr<TSLQNode> next;
        std::weak_ptr<TSLQNode> prev;
        std::unique_ptr<T> data;

        enum TSLQN_Type {
            TSLQN_NORMAL,
            TSLQN_HEAD,
            TSLQN_TAIL
        };

        TSLQN_Type type;
        bool isNormal() const;
    };

    class IterCount {
    public:
        IterCount(std::shared_ptr<std::atomic_ullong> count);
        ~IterCount();

        // Disallow copy.
        IterCount(const IterCount &) = delete;
        IterCount& operator=(const IterCount &) = delete;

        // Allow move.
        IterCount(IterCount &&);
        IterCount& operator=(IterCount &&);

    private:
        std::shared_ptr<std::atomic_ullong> count;

    };

    class WriteIterExists {
    public:
        WriteIterExists(std::shared_ptr<std::atomic_bool> exists);
        ~WriteIterExists();

        // Disallow copy.
        WriteIterExists(const WriteIterExists &) = delete;
        WriteIterExists& operator=(const WriteIterExists &) = delete;

        // Allow move.
        WriteIterExists(WriteIterExists &&);
        WriteIterExists& operator=(WriteIterExists &&);

        bool isReadWrite() const;

    private:
        std::shared_ptr<std::atomic_bool> exists;

    };

    class TSLQIter {
    public:
        TSLQIter(UDPC::SharedSpinLock::Weak sharedSpinLockWeak,
                 std::weak_ptr<TSLQNode> currentNode,
                 unsigned long *msize,
                 std::shared_ptr<std::atomic_ullong> count,
                 std::shared_ptr<std::atomic_bool> writeExists);
        ~TSLQIter();

        // Disallow copy.
        TSLQIter(const TSLQIter &) = delete;
        TSLQIter& operator=(const TSLQIter &) = delete;

        // Allow move.
        TSLQIter(TSLQIter &&) = default;
        TSLQIter& operator=(TSLQIter &&) = default;

        std::unique_ptr<T> current();
        bool next();
        bool prev();
        bool remove();
        bool try_remove();

    private:
        UDPC::SharedSpinLock::Weak sharedSpinLockWeak;
        IterCount iterCount;
        WriteIterExists writeExists;
        std::unique_ptr<UDPC::LockObj<false>> readLock;
        std::unique_ptr<UDPC::LockObj<true>> writeLock;
        std::weak_ptr<TSLQNode> currentNode;
        unsigned long *const msize;
        bool readOnly;

        bool remove_impl();

    };

  public:
    /// Only 1 read-write Iter can exist.
    std::optional<TSLQIter> begin(uint64_t timeout_sec);
    /// There can be many read-only Iterators.
    std::optional<TSLQIter> begin_readonly(uint64_t timeout_sec);

  private:
    UDPC::SharedSpinLock::Ptr sharedSpinLock;
    std::shared_ptr<std::atomic_ullong> iterCount;
    std::shared_ptr<std::atomic_bool> writeIterExists;
    std::mutex iterCreateMutex;
    std::shared_ptr<TSLQNode> head;
    std::shared_ptr<TSLQNode> tail;
    unsigned long msize;
};

template <typename T>
TSLQueue<T>::TSLQueue() :
    sharedSpinLock(UDPC::SharedSpinLock::newInstance()),
    iterCount(std::shared_ptr<std::atomic_ullong>(new std::atomic_ullong(0))),
    writeIterExists(std::shared_ptr<std::atomic_bool>(new std::atomic_bool(false))),
    iterCreateMutex(),
    head(std::shared_ptr<TSLQNode>(new TSLQNode())),
    tail(std::shared_ptr<TSLQNode>(new TSLQNode())),
    msize(0)
{
    head->next = tail;
    tail->prev = head;
    head->type = TSLQNode::TSLQN_Type::TSLQN_HEAD;
    tail->type = TSLQNode::TSLQN_Type::TSLQN_TAIL;
}

template <typename T>
TSLQueue<T>::~TSLQueue() {
}

template <typename T>
TSLQueue<T>::TSLQueue(TSLQueue &&other) :
    sharedSpinLock(UDPC::SharedSpinLock::newInstance()),
    iterCount(other.iterCount),
    head(std::shared_ptr<TSLQNode>(new TSLQNode())),
    tail(std::shared_ptr<TSLQNode>(new TSLQNode())),
    msize(0)
{
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto selfWriteLock = sharedSpinLock->spin_write_lock();
    auto otherWriteLock = other.sharedSpinLock->spin_write_lock();
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
    while (iterCount->load() != 0 && other.iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto selfWriteLock = sharedSpinLock->spin_write_lock();
    auto otherWriteLock = other.sharedSpinLock->spin_write_lock();
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
    iterCount = std::move(other.iterCount);
}

template <typename T>
void TSLQueue<T>::push(const T &data) {
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto writeLock = sharedSpinLock->spin_write_lock();
    auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
    newNode->data = std::unique_ptr<T>(new T(data));

    auto last = tail->prev.lock();
    assert(last);

    newNode->prev = last;
    newNode->next = tail;
    last->next = newNode;
    tail->prev = newNode;
    ++msize;
}

template <typename T>
bool TSLQueue<T>::push_nb(const T &data) {
    if (iterCount->load() != 0) {
        return false;
    }
    auto writeLock = sharedSpinLock->try_spin_write_lock();
    if(writeLock.isValid()) {
        auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
        newNode->data = std::unique_ptr<T>(new T(data));

        auto last = tail->prev.lock();
        assert(last);

        newNode->prev = last;
        newNode->next = tail;
        last->next = newNode;
        tail->prev = newNode;
        ++msize;

        return true;
    } else {
        return false;
    }
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::top() {
    auto readLock = sharedSpinLock->spin_read_lock();
    std::unique_ptr<T> result;
    if(head->next != tail) {
        assert(head->next->data);
        result = std::unique_ptr<T>(new T);
        *result = *head->next->data;
    }
    return result;
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::top_nb() {
    std::unique_ptr<T> result;
    auto readLock = sharedSpinLock->try_spin_read_lock();
    if(readLock.isValid()) {
        if(head->next != tail) {
            assert(head->next->data);
            result = std::unique_ptr<T>(new T);
            *result = *head->next->data;
        }
    }
    return result;
}

template <typename T>
bool TSLQueue<T>::pop() {
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next == tail) {
        return false;
    } else {
        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;

        return true;
    }
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::top_and_pop() {
    std::unique_ptr<T> result;
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next != tail) {
        assert(head->next->data);
        result = std::unique_ptr<T>(new T);
        *result = *head->next->data;

        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;
    }
    return result;
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::top_and_pop_and_empty(bool *isEmpty) {
    std::unique_ptr<T> result;
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next == tail) {
        if(isEmpty) {
            *isEmpty = true;
        }
    } else {
        assert(head->next->data);
        result = std::unique_ptr<T>(new T);
        *result = *head->next->data;

        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;

        if(isEmpty) {
            *isEmpty = head->next == tail;
        }
    }
    return result;
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::top_and_pop_and_rsize(unsigned long *rsize) {
    std::unique_ptr<T> result;
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next == tail) {
        if(rsize) {
            *rsize = 0;
        }
    } else {
        assert(head->next->data);
        result = std::unique_ptr<T>(new T);
        *result = *head->next->data;

        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;

        if(rsize) {
            *rsize = msize;
        }
    }
    return result;
}

template <typename T>
void TSLQueue<T>::clear() {
    while (iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto writeLock = sharedSpinLock->spin_write_lock();

    head->next = tail;
    tail->prev = head;
    msize = 0;
}

template <typename T>
bool TSLQueue<T>::empty() {
    auto readLock = sharedSpinLock->spin_read_lock();
    return head->next == tail;
}

template <typename T>
unsigned long TSLQueue<T>::size() {
    auto readLock = sharedSpinLock->spin_read_lock();
    return msize;
}

template <typename T>
TSLQueue<T>::TSLQNode::TSLQNode() :
next(),
prev(),
data(),
type(TSLQN_Type::TSLQN_NORMAL)
{}

template <typename T>
bool TSLQueue<T>::TSLQNode::isNormal() const {
    return type == TSLQN_Type::TSLQN_NORMAL;
}

template <typename T>
TSLQueue<T>::IterCount::IterCount(std::shared_ptr<std::atomic_ullong> count) : count(count) {
    if (this->count) {
        this->count->fetch_add(1);
    }
}

template <typename T>
TSLQueue<T>::IterCount::~IterCount() {
    if (this->count) {
        this->count->fetch_sub(1);
    }
}

template <typename T>
TSLQueue<T>::IterCount::IterCount(IterCount &&other) : count(other.count) {
    other.count.reset();
}

template <typename T>
typename TSLQueue<T>::IterCount &TSLQueue<T>::IterCount::operator=(IterCount &&other) {
    ~IterCount();

    this->count = std::move(other.count);
    other.count.reset();
}

template <typename T>
TSLQueue<T>::WriteIterExists::WriteIterExists(std::shared_ptr<std::atomic_bool> exists) : exists(exists) {
    if (this->exists) {
        this->exists->store(true);
    }
}

template <typename T>
TSLQueue<T>::WriteIterExists::~WriteIterExists() {
    if (this->exists) {
        this->exists->store(false);
    }
}

template <typename T>
TSLQueue<T>::WriteIterExists::WriteIterExists(WriteIterExists &&other) : exists(other.exists) {
    other.exists.reset();
}

template <typename T>
typename TSLQueue<T>::WriteIterExists &TSLQueue<T>::WriteIterExists::operator=(WriteIterExists &&other) {
    ~WriteIterExists();

    this->exists = std::move(other.exists);
    other.exists.reset();
}

template <typename T>
bool TSLQueue<T>::WriteIterExists::isReadWrite() const {
    return static_cast<bool>(this->exists);
}

template <typename T>
TSLQueue<T>::TSLQIter::TSLQIter(UDPC::SharedSpinLock::Weak lockWeak,
                                std::weak_ptr<TSLQNode> currentNode,
                                unsigned long *msize,
                                std::shared_ptr<std::atomic_ullong> count,
                                std::shared_ptr<std::atomic_bool> writeExists) :
sharedSpinLockWeak(lockWeak),
iterCount(count),
writeExists(writeExists),
readLock(std::unique_ptr<UDPC::LockObj<false>>(new UDPC::LockObj<false>{})),
writeLock(),
currentNode(currentNode),
msize(msize),
readOnly(true)
{
    *readLock = lockWeak.lock()->spin_read_lock();
    this->readOnly = !this->writeExists.isReadWrite();
}

template <typename T>
TSLQueue<T>::TSLQIter::~TSLQIter() {}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::TSLQIter::current() {
    std::unique_ptr<T> result;
    std::shared_ptr<TSLQNode> currentNode = this->currentNode.lock();
    assert(currentNode);
    if(currentNode->isNormal()) {
        result = std::unique_ptr<T>(new T);
        *result = *currentNode->data;
    }
    return result;
}

template <typename T>
bool TSLQueue<T>::TSLQIter::next() {
    std::shared_ptr<TSLQNode> currentNode = this->currentNode.lock();
    assert(currentNode);
    if(currentNode->type == TSLQNode::TSLQN_Type::TSLQN_TAIL) {
        return false;
    }

    this->currentNode = currentNode->next;
    return currentNode->next->type != TSLQNode::TSLQN_Type::TSLQN_TAIL;
}

template <typename T>
bool TSLQueue<T>::TSLQIter::prev() {
    std::shared_ptr<TSLQNode> currentNode = this->currentNode.lock();
    assert(currentNode);
    if(currentNode->type == TSLQNode::TSLQN_Type::TSLQN_HEAD) {
        return false;
    }

    auto parent = currentNode->prev.lock();
    assert(parent);
    this->currentNode = currentNode->prev;
    return parent->type != TSLQNode::TSLQN_Type::TSLQN_HEAD;
}

template <typename T>
bool TSLQueue<T>::TSLQIter::remove() {
    if (!readOnly && readLock && !writeLock && readLock->isValid()) {
        auto sharedSpinLockStrong = sharedSpinLockWeak.lock();
        if (!sharedSpinLockStrong) {
            return false;
        }

        // Drop the read lock.
        readLock.reset(nullptr);

        writeLock = std::unique_ptr<UDPC::LockObj<true>>(new UDPC::LockObj<true>{});
        // Get the write lock.
        *writeLock = sharedSpinLockStrong->spin_write_lock();

        return remove_impl();
    } else {
        return false;
    }
}

template <typename T>
bool TSLQueue<T>::TSLQIter::try_remove() {
    if (!readOnly && readLock && !writeLock && readLock->isValid()) {
        auto sharedSpinLockStrong = sharedSpinLockWeak.lock();
        if (!sharedSpinLockStrong) {
            return false;
        }

        // Drop the read lock.
        readLock.reset(nullptr);

        writeLock = std::unique_ptr<UDPC::LockObj<true>>(new UDPC::LockObj<true>{});
        // Get the write lock.
        *writeLock = sharedSpinLockStrong->try_spin_write_lock();
        if (writeLock->isValid()) {
            return remove_impl();
        } else {
            // Get a read lock back.
            readLock = std::unique_ptr<UDPC::LockObj<false>>(new UDPC::LockObj<false>{});
            *readLock = sharedSpinLockStrong->spin_read_lock();
            return false;
        }
    } else {
        return false;
    }
}

template <typename T>
bool TSLQueue<T>::TSLQIter::remove_impl() {
    const auto cleanupWriteLock = [this] () {
        UDPC::SharedSpinLock::Ptr sharedSpinLockStrong = this->sharedSpinLockWeak.lock();
        if (!sharedSpinLockStrong) {
            writeLock.reset(nullptr);
            return;
        }
        this->readLock = std::unique_ptr<UDPC::LockObj<false>>(new UDPC::LockObj<false>{});
        (*this->readLock) = sharedSpinLockStrong->trade_write_for_read_lock(*(this->writeLock));
        this->writeLock.reset(nullptr);
    };

    std::shared_ptr<TSLQNode> currentNode = this->currentNode.lock();
    assert(currentNode);
    if(!currentNode->isNormal()) {
        cleanupWriteLock();
        return false;
    }

    this->currentNode = currentNode->next;
    auto parent = currentNode->prev.lock();
    assert(parent);

    currentNode->next->prev = parent;
    parent->next = currentNode->next;

    assert(*msize > 0);
    --(*msize);

    cleanupWriteLock();
    return parent->next->isNormal();
}

template <typename T>
std::optional<typename TSLQueue<T>::TSLQIter> TSLQueue<T>::begin(uint64_t timeout_sec) {
    const auto duration = std::chrono::seconds(timeout_sec);
    const auto start_time = std::chrono::steady_clock::now();
    auto mlock = std::lock_guard<std::mutex>(this->iterCreateMutex);
    while (this->iterCount->load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (std::chrono::steady_clock::now() - start_time > duration) {
            return std::nullopt;
        }
    }
    return TSLQIter(sharedSpinLock, head->next, &msize, iterCount, writeIterExists);
}

template <typename T>
std::optional<typename TSLQueue<T>::TSLQIter> TSLQueue<T>::begin_readonly(uint64_t timeout_sec) {
    const auto duration = std::chrono::seconds(timeout_sec);
    const auto start_time = std::chrono::steady_clock::now();
    auto mlock = std::lock_guard<std::mutex>(this->iterCreateMutex);
    while (this->writeIterExists->load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (std::chrono::steady_clock::now() - start_time > duration) {
            return std::nullopt;
        }
    }
    return TSLQIter(sharedSpinLock, head->next, &msize, iterCount, {});
}

#endif
