#include "TSQueue.hpp"

#include <cstring>

TSQueue::TSQueue(unsigned int elemSize, unsigned int capacity)
    : elemSize(elemSize), head(0), tail(0), isEmpty(true),
      spinLock(false) {
    if (elemSize == 0) {
        this->elemSize = 1;
    }
    if (capacity == 0) {
        this->capacityBytes = UDPC_TSQUEUE_DEFAULT_CAPACITY * this->elemSize;
    } else {
        this->capacityBytes = capacity * this->elemSize;
    }

    this->buffer =
        std::unique_ptr<unsigned char[]>(new unsigned char[this->capacityBytes]);
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
    tail = (tail + elemSize) % capacityBytes;

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
    memcpy(data.get(), buffer.get() + head, elemSize);
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
    if (head >= capacityBytes) {
        head = 0;
    }
    if (head == tail) {
        isEmpty = true;
    }
    spinLock.store(false);
    return true;
}

void TSQueue::clear() {
    while (spinLock.exchange(true) == true) {
    }

    head = 0;
    tail = 0;
    isEmpty = 0;

    spinLock.store(false);
}

void TSQueue::changeCapacity(unsigned int newCapacity) {
    if (newCapacity == 0) {
        return;
    }
    while (spinLock.exchange(true) == true) {
    }

    // repeat of sizeBytes() to avoid deadlock
    unsigned int size;
    if (head == tail) {
        size = capacityBytes;
    } else if (head < tail) {
        size = tail - head;
    } else {
        size = capacityBytes - head + tail;
    }

    unsigned int newCap = newCapacity * elemSize;
    auto newBuffer =
        std::unique_ptr<unsigned char[]>(new unsigned char[newCap]);

    if (!isEmpty) {
        unsigned int tempHead = head;
        if (size > newCap) {
            unsigned int diff = size - newCap;
            tempHead = (head + diff) % capacityBytes;
        }
        if (tempHead < tail) {
            memcpy(newBuffer.get(), buffer.get() + tempHead, tail - tempHead);
        } else {
            memcpy(newBuffer.get(), buffer.get() + tempHead,
                   capacityBytes - tempHead);
            if (tail != 0) {
                memcpy(newBuffer.get() + capacityBytes - tempHead, buffer.get(),
                       tail);
            }
        }
    }

    if (size < newCap) {
        if (head < tail) {
            tail = tail - head;
            head = 0;
        } else {
            tail = capacityBytes - head + tail;
            head = 0;
        }
    } else {
        head = 0;
        tail = 0;
        isEmpty = false;
    }
    buffer = std::move(newBuffer);
    capacityBytes = newCap;

    spinLock.store(false);
}

unsigned int TSQueue::size() {
    while (spinLock.exchange(true) == true) {
    }

    if (isEmpty) {
        spinLock.store(false);
        return 0;
    }

    unsigned int size;
    if (head == tail) {
        size = capacityBytes;
    } else if (head < tail) {
        size = tail - head;
    } else {
        size = capacityBytes - head + tail;
    }
    size /= elemSize;

    spinLock.store(false);
    return size;
}

unsigned int TSQueue::sizeBytes() {
    while (spinLock.exchange(true) == true) {
    }

    if (isEmpty) {
        spinLock.store(false);
        return 0;
    }

    unsigned int size;
    if (head == tail) {
        size = capacityBytes;
    } else if (head < tail) {
        size = tail - head;
    } else {
        size = capacityBytes - head + tail;
    }

    spinLock.store(false);
    return size;
}
