#include "TSQueue.hpp"

#include <cstring>

TSQueue::TSQueue(unsigned int elemSize, unsigned int capacity)
    : elemSize(elemSize), capacity(capacity), head(0), tail(0), isEmpty(true),
      spinLock(false) {
    if (elemSize == 0) {
        this->elemSize = 1;
    }
    if (capacity == 0) {
        this->capacity = UDPC_TSQUEUE_DEFAULT_CAPACITY * this->elemSize;
    }

    this->buffer =
        std::unique_ptr<unsigned char[]>(new unsigned char[this->capacity]);
}

TSQueue::~TSQueue() {}

bool TSQueue::push(void *data) {
    while (spinLock.exchange(true) == true) {
    }
    if (!isEmpty && head == tail) {
        spinLock.store(false);
        return false;
    }

    memcpy(buffer.get() + tail, data, elemSize);
    tail = (tail + elemSize) % capacity;

    isEmpty = false;

    spinLock.store(false);
    return true;
}

std::unique_ptr<unsigned char[]> TSQueue::top() {
    while (spinLock.exchange(true) == true) {
    }
    if (isEmpty) {
        spinLock.store(false);
        return std::unique_ptr<unsigned char[]>();
    }

    auto data = std::unique_ptr<unsigned char[]>(new unsigned char[elemSize]);
    if (tail != 0) {
        memcpy(data.get(), buffer.get() + (tail - elemSize), elemSize);
    } else {
        memcpy(data.get(), buffer.get() + (capacity - elemSize), elemSize);
    }
    spinLock.store(false);
    return data;
}

bool TSQueue::pop() {
    while (spinLock.exchange(true) == true) {
    }
    if (isEmpty) {
        spinLock.store(false);
        return false;
    }
    head += elemSize;
    if (head >= capacity) {
        head = 0;
    }
    if (head == tail) {
        isEmpty = true;
    }
    spinLock.store(false);
    return true;
}
