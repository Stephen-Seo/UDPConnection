#ifndef UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP
#define UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP

#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <optional>
#include <cassert>

#include <list>
#include <type_traits>

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
    std::optional<T> top();
    std::optional<T> top_nb();
    bool pop();
    std::optional<T> top_and_pop();
    std::optional<T> top_and_pop_and_empty(bool *isEmpty);
    void clear();

    bool empty();
    unsigned long long size();

  private:
    struct TSLQNode {
        TSLQNode() = default;
        // disable copy
        TSLQNode(TSLQNode& other) = delete;
        TSLQNode& operator=(TSLQNode& other) = delete;
        // enable move
        TSLQNode(TSLQNode&& other) = default;
        TSLQNode& operator=(TSLQNode&& other) = default;

        std::shared_ptr<TSLQNode> next;
        std::weak_ptr<TSLQNode> prev;
        std::unique_ptr<T> data;
    };

    std::shared_ptr<char> iterValid;
    std::shared_ptr<char> iterWrapperCount;
    std::mutex mutex;
    std::shared_ptr<TSLQNode> head;
    std::shared_ptr<TSLQNode> tail;
    unsigned long long msize;
};

template <typename T>
TSLQueue<T>::TSLQueue() :
    iterValid(std::make_shared<char>()),
    iterWrapperCount(std::make_shared<char>()),
    head(std::make_shared<TSLQNode>()),
    tail(std::make_shared<TSLQNode>()),
    msize(0)
{
    head->next = tail;
    tail->prev = head;
}

template <typename T>
TSLQueue<T>::~TSLQueue() {
}

template <typename T>
TSLQueue<T>::TSLQueue(TSLQueue &&other) :
    iterValid(std::make_shared<char>()),
    iterWrapperCount(std::make_shared<char>())
{
    std::lock_guard lock(other.mutex);
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
    iterValid = std::make_shared<char>();
    iterWrapperCount = std::make_shared<char>();
    std::scoped_lock lock(mutex, other.mutex);
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
void TSLQueue<T>::push(const T &data) {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    auto newNode = std::make_shared<TSLQNode>();
    newNode->data = std::make_unique<T>(data);

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
    if(iterWrapperCount.use_count() > 1) {
        return false;
    } else if(mutex.try_lock()) {
        auto newNode = std::make_shared<TSLQNode>();
        newNode->data = std::make_unique<T>(data);

        auto last = tail->prev.lock();
        assert(last);

        newNode->prev = last;
        newNode->next = tail;
        last->next = newNode;
        tail->prev = newNode;
        ++msize;

        mutex.unlock();
        return true;
    } else {
        return false;
    }
}

template <typename T>
std::optional<T> TSLQueue<T>::top() {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    if(head->next != tail) {
        assert(head->next->data);
        return *head->next->data.get();
    } else {
        return std::nullopt;
    }
}

template <typename T>
std::optional<T> TSLQueue<T>::top_nb() {
    if(iterWrapperCount.use_count() > 1) {
        return std::nullopt;
    } else if(mutex.try_lock()) {
        std::optional<T> ret = std::nullopt;
        if(head->next != tail) {
            assert(head->next->data);
            ret = *head->next->data.get();
        }
        mutex.unlock();
        return ret;
    } else {
        return std::nullopt;
    }
}

template <typename T>
bool TSLQueue<T>::pop() {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    if(head->next == tail) {
        return false;
    } else {
        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;

        iterValid = std::make_shared<char>();
        iterWrapperCount = std::make_shared<char>();
        return true;
    }
}

template <typename T>
std::optional<T> TSLQueue<T>::top_and_pop() {
    std::optional<T> ret = std::nullopt;
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    if(head->next != tail) {
        assert(head->next->data);
        ret = *head->next->data.get();

        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;

        iterValid = std::make_shared<char>();
        iterWrapperCount = std::make_shared<char>();
    }
    return ret;
}

template <typename T>
std::optional<T> TSLQueue<T>::top_and_pop_and_empty(bool *isEmpty) {
    std::optional<T> ret = std::nullopt;
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    if(head->next == tail) {
        if(isEmpty) {
            *isEmpty = true;
        }
    } else {
        assert(head->next->data);
        ret = *head->next->data.get();

        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;

        iterValid = std::make_shared<char>();
        iterWrapperCount = std::make_shared<char>();
        if(isEmpty) {
            *isEmpty = head->next == tail;
        }
    }
    return ret;
}

template <typename T>
void TSLQueue<T>::clear() {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);

    head->next = tail;
    tail->prev = head;
    msize = 0;

    iterValid = std::make_shared<char>();
    iterWrapperCount = std::make_shared<char>();
}

template <typename T>
bool TSLQueue<T>::empty() {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    return head->next == tail;
}

template <typename T>
unsigned long long TSLQueue<T>::size() {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    return msize;
}

#endif
