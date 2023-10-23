#ifndef UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP
#define UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP

#include <memory>
#include <thread>
#include <chrono>
#include <optional>
#include <cassert>
#include <list>
#include <type_traits>

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

    class TSLQIter {
    public:
        TSLQIter(UDPC::SharedSpinLock::Weak sharedSpinLockWeak,
                 std::weak_ptr<TSLQNode> currentNode,
                 unsigned long *msize);
        ~TSLQIter();

        // Disallow copy.
        TSLQIter(const TSLQIter &) = delete;
        TSLQIter& operator=(const TSLQIter &) = delete;

        std::unique_ptr<T> current();
        bool next();
        bool prev();
        bool remove();
        bool try_remove();

    private:
        UDPC::SharedSpinLock::Weak sharedSpinLockWeak;
        std::unique_ptr<UDPC::LockObj<false>> readLock;
        std::unique_ptr<UDPC::LockObj<true>> writeLock;
        std::weak_ptr<TSLQNode> currentNode;
        unsigned long *const msize;

        bool remove_impl();

    };

  public:
    TSLQIter begin();

  private:
    UDPC::SharedSpinLock::Ptr sharedSpinLock;
    std::shared_ptr<TSLQNode> head;
    std::shared_ptr<TSLQNode> tail;
    unsigned long msize;
};

template <typename T>
TSLQueue<T>::TSLQueue() :
    sharedSpinLock(UDPC::SharedSpinLock::newInstance()),
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
    head(std::shared_ptr<TSLQNode>(new TSLQNode())),
    tail(std::shared_ptr<TSLQNode>(new TSLQNode())),
    msize(0)
{
    auto selfWriteLock = sharedSpinLock->spin_write_lock();
    auto otherWriteLock = other.sharedSpinLock->spin_write_lock();
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
    auto selfWriteLock = sharedSpinLock->spin_write_lock();
    auto otherWriteLock = other.sharedSpinLock->spin_write_lock();
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
void TSLQueue<T>::push(const T &data) {
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
TSLQueue<T>::TSLQIter::TSLQIter(UDPC::SharedSpinLock::Weak lockWeak,
                                std::weak_ptr<TSLQNode> currentNode,
                                unsigned long *msize) :
sharedSpinLockWeak(lockWeak),
readLock(std::unique_ptr<UDPC::LockObj<false>>(new UDPC::LockObj<false>{})),
writeLock(),
currentNode(currentNode),
msize(msize)
{
    *readLock = lockWeak.lock()->spin_read_lock();
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
    if (readLock && !writeLock && readLock->isValid()) {
        auto sharedSpinLockStrong = sharedSpinLockWeak.lock();
        if (!sharedSpinLockStrong) {
            return false;
        }

        writeLock = std::unique_ptr<UDPC::LockObj<true>>(new UDPC::LockObj<true>{});
        *writeLock = sharedSpinLockStrong->trade_read_for_write_lock(*readLock);
        readLock.reset(nullptr);

        return remove_impl();
    } else {
        return false;
    }
}

template <typename T>
bool TSLQueue<T>::TSLQIter::try_remove() {
    if (readLock && !writeLock && readLock->isValid()) {
        auto sharedSpinLockStrong = sharedSpinLockWeak.lock();
        if (!sharedSpinLockStrong) {
            return false;
        }

        writeLock = std::unique_ptr<UDPC::LockObj<true>>(new UDPC::LockObj<true>{});
        *writeLock = sharedSpinLockStrong->try_trade_read_for_write_lock(*readLock);
        if (writeLock->isValid()) {
            readLock.reset(nullptr);
            return remove_impl();
        } else {
            writeLock.reset(nullptr);
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
typename TSLQueue<T>::TSLQIter TSLQueue<T>::begin() {
    return TSLQIter(sharedSpinLock, head->next, &msize);
}

#endif
