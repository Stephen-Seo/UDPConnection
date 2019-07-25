#ifndef UDPC_THREADSAFE_QUEUE_HPP
#define UDPC_THREADSAFE_QUEUE_HPP

#define UDPC_TSQUEUE_DEFAULT_CAPACITY 32

#include <atomic>
#include <cstdlib>
#include <memory>

#include <RB/RingBuffer.hpp>

template <typename T>
class TSQueue {
  public:
    TSQueue(unsigned int capacity = UDPC_TSQUEUE_DEFAULT_CAPACITY);
    ~TSQueue();

    // disable copy
    TSQueue(const TSQueue &other) = delete;
    TSQueue &operator=(const TSQueue &other) = delete;
    // disable move
    TSQueue(TSQueue &&other) = delete;
    TSQueue &operator=(TSQueue &&other) = delete;

    bool push(const T &data);
    T top();
    bool pop();
    void clear();
    void changeCapacity(unsigned int newCapacity);
    unsigned int size();
    bool empty();

  private:
    std::atomic_bool spinLock;
    RB::RingBuffer<T> rb;
};

template <typename T>
TSQueue<T>::TSQueue(unsigned int capacity) :
spinLock(false),
rb(capacity)
{
    rb.setResizePolicy(false);
}

template <typename T>
TSQueue<T>::~TSQueue()
{}

template <typename T>
bool TSQueue<T>::push(const T &data) {
    while(spinLock.exchange(true)) {}
    if(rb.getSize() == rb.getCapacity()) {
        spinLock.store(false);
        return false;
    }
    rb.push(data);
    spinLock.store(false);
    return true;
}

template <typename T>
T TSQueue<T>::top() {
    while(spinLock.exchange(true)) {}
    T value = rb.top();
    spinLock.store(false);
    return value;
}

template <typename T>
bool TSQueue<T>::pop() {
    while(spinLock.exchange(true)) {}
    if(rb.empty()) {
        spinLock.store(false);
        return false;
    }
    rb.pop();
    spinLock.store(false);
    return true;
}

template <typename T>
void TSQueue<T>::clear() {
    while(spinLock.exchange(true)) {}
    rb.resize(0);
    spinLock.store(false);
}

template <typename T>
void TSQueue<T>::changeCapacity(unsigned int newCapacity) {
    while(spinLock.exchange(true)) {}
    rb.changeCapacity(newCapacity);
    spinLock.store(false);
}

template <typename T>
unsigned int TSQueue<T>::size() {
    while(spinLock.exchange(true)) {}
    unsigned int size = rb.getSize();
    spinLock.store(false);
    return size;
}

template <typename T>
bool TSQueue<T>::empty() {
    // No lock required, since this is calling size() that uses a lock
    unsigned int size = this->size();
    return size == 0;
}

#endif
