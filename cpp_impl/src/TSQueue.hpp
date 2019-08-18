#ifndef UDPC_THREADSAFE_QUEUE_HPP
#define UDPC_THREADSAFE_QUEUE_HPP

#define UDPC_TSQUEUE_DEFAULT_CAPACITY 32

#include <cstdlib>
#include <memory>
#include <mutex>

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
    std::mutex mutex;
    RB::RingBuffer<T> rb;
};

template <typename T>
TSQueue<T>::TSQueue(unsigned int capacity) :
mutex(),
rb(capacity)
{
    rb.setResizePolicy(false);
}

template <typename T>
TSQueue<T>::~TSQueue()
{}

template <typename T>
bool TSQueue<T>::push(const T &data) {
    std::lock_guard<std::mutex> lock(mutex);
    if(rb.getSize() == rb.getCapacity()) {
        return false;
    }
    rb.push(data);
    return true;
}

template <typename T>
T TSQueue<T>::top() {
    std::lock_guard<std::mutex> lock(mutex);
    T value = rb.top();
    return value;
}

template <typename T>
bool TSQueue<T>::pop() {
    std::lock_guard<std::mutex> lock(mutex);
    if(rb.empty()) {
        return false;
    }
    rb.pop();
    return true;
}

template <typename T>
void TSQueue<T>::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    rb.resize(0);
}

template <typename T>
void TSQueue<T>::changeCapacity(unsigned int newCapacity) {
    std::lock_guard<std::mutex> lock(mutex);
    rb.changeCapacity(newCapacity);
}

template <typename T>
unsigned int TSQueue<T>::size() {
    std::lock_guard<std::mutex> lock(mutex);
    unsigned int size = rb.getSize();
    return size;
}

template <typename T>
bool TSQueue<T>::empty() {
    // No lock required, since this is calling size() that uses a lock
    unsigned int size = this->size();
    return size == 0;
}

#endif
