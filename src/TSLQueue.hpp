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

    void push_back(const T &data);
    bool push_back_nb(const T &data);
    void push_front(const T &data);
    bool push_front_nb(const T &data);
    void push_back(T &&data);
    bool push_back_nb(T &&data);
    void push_front(T &&data);
    bool push_front_nb(T &&data);
    std::unique_ptr<T> top();
    std::unique_ptr<T> top_nb();
    bool pop();
    std::unique_ptr<T> top_and_pop();
    std::unique_ptr<T> top_and_pop_and_empty(bool *isEmpty);
    std::unique_ptr<T> top_and_pop_and_rsize(unsigned long *rsize);
    std::unique_ptr<T> bot();
    std::unique_ptr<T> bot_nb();
    bool pop_bot();
    std::unique_ptr<T> bot_and_pop();
    std::unique_ptr<T> bot_and_pop_and_empty(bool *isEmpty);
    std::unique_ptr<T> bot_and_pop_and_rsize(unsigned long *rsize);
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

    other.head = std::shared_ptr<TSLQNode>(new TSLQNode());
    other.tail = std::shared_ptr<TSLQNode>(new TSLQNode());

    other.head->type = TSLQNode::TSLQN_Type::TSLQN_HEAD;
    other.tail->type = TSLQNode::TSLQN_Type::TSLQN_TAIL;

    other.head->next = other.tail;
    other.tail->prev = other.head;

    other.msize = 0;
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
    auto selfWriteLock = sharedSpinLock->spin_write_lock();
    auto otherWriteLock = other.sharedSpinLock->spin_write_lock();
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);

    other.head = std::shared_ptr<TSLQNode>(new TSLQNode());
    other.tail = std::shared_ptr<TSLQNode>(new TSLQNode());

    other.head->type = TSLQNode::TSLQN_Type::TSLQN_HEAD;
    other.tail->type = TSLQNode::TSLQN_Type::TSLQN_TAIL;

    other.head->next = other.tail;
    other.tail->prev = other.head;

    other.msize = 0;
}

template <typename T>
void TSLQueue<T>::push_back(const T &data) {
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
bool TSLQueue<T>::push_back_nb(const T &data) {
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
void TSLQueue<T>::push_front(const T &data) {
    auto writeLock = sharedSpinLock->spin_write_lock();
    auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
    newNode->data = std::unique_ptr<T>(new T(data));

    auto first = head->next;
    assert(first);

    newNode->next = first;
    newNode->prev = head;
    first->prev = newNode;
    head->next = newNode;

    ++msize;
}

template <typename T>
bool TSLQueue<T>::push_front_nb(const T &data) {
    auto writeLock = sharedSpinLock->try_spin_write_lock();
    if(writeLock.isValid()) {
        auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
        newNode->data = std::unique_ptr<T>(new T(data));

        auto first = head->next;
        assert(first);

        newNode->next = first;
        newNode->prev = head;
        first->prev = newNode;
        head->next = newNode;

        ++msize;

        return true;
    } else {
        return false;
    }
}

template <typename T>
void TSLQueue<T>::push_back(T &&data) {
    auto writeLock = sharedSpinLock->spin_write_lock();
    auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
    newNode->data = std::unique_ptr<T>(new T(std::forward<T>(data)));

    auto last = tail->prev.lock();
    assert(last);

    newNode->prev = last;
    newNode->next = tail;
    last->next = newNode;
    tail->prev = newNode;
    ++msize;
}

template <typename T>
bool TSLQueue<T>::push_back_nb(T &&data) {
    auto writeLock = sharedSpinLock->try_spin_write_lock();
    if(writeLock.isValid()) {
        auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
        newNode->data = std::unique_ptr<T>(new T(std::forward<T>(data)));

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
void TSLQueue<T>::push_front(T &&data) {
    auto writeLock = sharedSpinLock->spin_write_lock();
    auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
    newNode->data = std::unique_ptr<T>(new T(std::forward<T>(data)));

    auto first = head->next;
    assert(first);

    newNode->next = first;
    newNode->prev = head;
    first->prev = newNode;
    head->next = newNode;

    ++msize;
}

template <typename T>
bool TSLQueue<T>::push_front_nb(T &&data) {
    auto writeLock = sharedSpinLock->try_spin_write_lock();
    if(writeLock.isValid()) {
        auto newNode = std::shared_ptr<TSLQNode>(new TSLQNode());
        newNode->data = std::unique_ptr<T>(new T(std::forward<T>(data)));

        auto first = head->next;
        assert(first);

        newNode->next = first;
        newNode->prev = head;
        first->prev = newNode;
        head->next = newNode;

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
        result = std::unique_ptr<T>(new T(*head->next->data.get()));
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
            result = std::unique_ptr<T>(new T(*head->next->data.get()));
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
        result = std::move(head->next->data);

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
        result = std::move(head->next->data);

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
        result = std::move(head->next->data);

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
std::unique_ptr<T> TSLQueue<T>::bot() {
    auto readLock = sharedSpinLock->spin_read_lock();
    std::unique_ptr<T> result;

    std::shared_ptr<TSLQNode> prev = tail->prev.lock();
    if (prev && prev != head) {
        assert(prev->data);
        result = std::unique_ptr<T>(new T(*prev->data.get()));
    }

    return result;
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::bot_nb() {
    std::unique_ptr<T> result;
    auto readLock = sharedSpinLock->try_spin_read_lock();

    if (readLock.isValid()) {
        std::shared_ptr<TSLQNode> prev = tail->prev.lock();
        if (prev && prev != head) {
            assert(prev->data);
            result = std::unique_ptr<T>(new T(*prev->data.get()));
        }
    }

    return result;
}

template <typename T>
bool TSLQueue<T>::pop_bot() {
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next == tail) {
        return false;
    } else {
        std::shared_ptr<TSLQNode> end = tail->prev.lock();
        if (end) {
            std::shared_ptr<TSLQNode> before_end = end->prev.lock();
            if (before_end) {
                before_end->next = tail;
                tail->prev = before_end;
                assert(msize > 0);
                --msize;

                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::bot_and_pop() {
    std::unique_ptr<T> result;
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next != tail) {
        std::shared_ptr<TSLQNode> end = tail->prev.lock();
        if (end) {
            assert(end->data);
            std::shared_ptr<TSLQNode> before_end = end->prev.lock();
            if (before_end) {
                result = std::move(end->data);

                before_end->next = tail;
                tail->prev = before_end;
                assert(msize > 0);
                --msize;

                return result;
            } else {
                return result;
            }
        } else {
            return result;
        }
    }
    return result;
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::bot_and_pop_and_empty(bool *isEmpty) {
    std::unique_ptr<T> result;
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next != tail) {
        std::shared_ptr<TSLQNode> end = tail->prev.lock();
        if (end) {
            assert(end->data);
            std::shared_ptr<TSLQNode> before_end = end->prev.lock();
            if (before_end) {
                result = std::move(end->data);

                before_end->next = tail;
                tail->prev = before_end;
                assert(msize > 0);
                --msize;

                if(isEmpty) {
                    *isEmpty = head->next == tail;
                }
                return result;
            } else {
                if(isEmpty) {
                    *isEmpty = head->next == tail;
                }
                return result;
            }
        } else {
            if(isEmpty) {
                *isEmpty = head->next == tail;
            }
            return result;
        }
    }
    if(isEmpty) {
        *isEmpty = head->next == tail;
    }
    return result;
}

template <typename T>
std::unique_ptr<T> TSLQueue<T>::bot_and_pop_and_rsize(unsigned long *rsize) {
    std::unique_ptr<T> result;
    auto writeLock = sharedSpinLock->spin_write_lock();
    if(head->next != tail) {
        std::shared_ptr<TSLQNode> end = tail->prev.lock();
        if (end) {
            assert(end->data);
            std::shared_ptr<TSLQNode> before_end = end->prev.lock();
            if (before_end) {
                result = std::move(end->data);

                before_end->next = tail;
                tail->prev = before_end;
                assert(msize > 0);
                --msize;

                if(rsize) {
                    *rsize = msize;
                }
                return result;
            } else {
                if(rsize) {
                    *rsize = msize;
                }
                return result;
            }
        } else {
            if(rsize) {
                *rsize = msize;
            }
            return result;
        }
    }
    if(rsize) {
        *rsize = 0;
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

#endif
