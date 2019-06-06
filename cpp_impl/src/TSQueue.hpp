#ifndef UDPC_THREADSAFE_QUEUE_HPP
#define UDPC_THREADSAFE_QUEUE_HPP

#define UDPC_TSQUEUE_DEFAULT_CAPACITY 32

#include <atomic>
#include <cstdlib>
#include <memory>

class TSQueue {
  public:
    TSQueue(unsigned int elemSize,
            unsigned int capacity = UDPC_TSQUEUE_DEFAULT_CAPACITY);
    ~TSQueue();

    // disable copy
    TSQueue(const TSQueue &other) = delete;
    TSQueue &operator=(const TSQueue &other) = delete;
    // disable move
    TSQueue(TSQueue &&other) = delete;
    TSQueue &operator=(TSQueue &&other) = delete;

    bool push(void *data);
    std::unique_ptr<unsigned char[]> top();
    bool pop();

  private:
    unsigned int elemSize;
    unsigned int capacity;
    unsigned int head;
    unsigned int tail;
    bool isEmpty;
    std::unique_ptr<unsigned char[]> buffer;
    std::atomic_bool spinLock;
};

#endif
