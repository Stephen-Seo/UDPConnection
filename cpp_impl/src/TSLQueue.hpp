#ifndef UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP
#define UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP

#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <optional>

#include <list>
#include <type_traits>

// definition

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

    bool push(const T &data);
    bool push_nb(const T &data);
    std::optional<T> top();
    std::optional<T> top_nb();
    bool pop();
    std::optional<T> top_and_pop();
    std::optional<T> top_and_pop_and_empty(bool *isEmpty);
    void clear();

    bool empty();

    template <bool isConst, bool isRev>
    class TSLQIterWrapper {
      public:
        TSLQIterWrapper(
            std::conditional_t<isConst, const std::list<T>, std::list<T>> *container,
            std::weak_ptr<void> iterValid,
            std::shared_ptr<void> iterWrapperCount
        );

        bool isValid() const;

        bool next();
        bool prev();
        std::optional<T> current();

      private:
        std::conditional_t<isConst, const std::list<T>, std::list<T>>
            *containerPtr;
        std::conditional_t<isRev,
            std::conditional_t<isConst,
                typename std::list<T>::const_reverse_iterator,
                typename std::list<T>::reverse_iterator>,
            std::conditional_t<isConst,
                typename std::list<T>::const_iterator,
                typename std::list<T>::iterator>>
                    iter;

        std::weak_ptr<void> iterValid;
        std::shared_ptr<void> iterWrapperCount;
    };

    TSLQIterWrapper<false, false> iter();
    TSLQIterWrapper<false, true> riter();
    TSLQIterWrapper<true, false> citer();
    TSLQIterWrapper<true, true> criter();

  private:
    std::shared_ptr<void> iterValid;
    std::shared_ptr<void> iterWrapperCount;
    std::mutex mutex;
    std::list<T> container;
};

// implementation

template <typename T>
TSLQueue<T>::TSLQueue() :
    iterValid(std::make_shared<void>()),
    iterWrapperCount(std::make_shared<void>())
{
}

template <typename T>
TSLQueue<T>::~TSLQueue() {
}

template <typename T>
TSLQueue<T>::TSLQueue(TSLQueue &&other) :
    iterValid(std::make_shared<void>()),
    iterWrapperCount(std::make_shared<void>())
{
    std::lock_guard lock(other.mutex);
    container = std::move(other.container);
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
    std::scoped_lock lock(mutex, other.mutex);
    container = std::move(other.container);
}

template <typename T>
bool TSLQueue<T>::push(const T &data) {
    while(iterWrapperCount.use_count() > 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard lock(mutex);
    container.push_back(data);
    return true;
}

template <typename T>
bool TSLQueue<T>::push_nb(const T &data) {
    if(iterWrapperCount.use_count() > 1) {
        return false;
    } else if(mutex.try_lock()) {
        container.push_back(data);
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
    if(container.empty()) {
        return std::nullopt;
    } else {
        return container.front();
    }
}

template <typename T>
std::optional<T> TSLQueue<T>::top_nb() {
    if(iterWrapperCount.use_count() > 1) {
        return std::nullopt;
    } else if(mutex.try_lock()) {
        std::optional<T> ret = container.front();
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
    if(container.empty()) {
        return false;
    } else {
        container.pop_front();
        iterValid = std::make_shared<void>();
        iterWrapperCount = std::make_shared<void>();
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
    if(!container.empty()) {
        ret = container.front();
        container.pop_front();
        iterValid = std::make_shared<void>();
        iterWrapperCount = std::make_shared<void>();
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
    if(container.empty()) {
        if(isEmpty) {
            *isEmpty = true;
        }
    } else {
        ret = container.front();
        container.pop_front();
        iterValid = std::make_shared<void>();
        iterWrapperCount = std::make_shared<void>();
        if(isEmpty) {
            *isEmpty = container.empty();
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
    container.clear();
    iterValid = std::make_shared<void>();
    iterWrapperCount = std::make_shared<void>();
}

template <typename T>
template <bool isConst, bool isRev>
TSLQueue<T>::TSLQIterWrapper<isConst, isRev>::TSLQIterWrapper(
        std::conditional_t<isConst, const std::list<T>, std::list<T>> *container,
        std::weak_ptr<void> iterValid,
        std::shared_ptr<void> iterWrapperCount) :
            containerPtr(container),
            iterValid(iterValid),
            iterWrapperCount(iterWrapperCount) {
    if constexpr (isRev) {
        if constexpr (isConst) {
            iter = containerPtr->crbegin();
        } else {
            iter = containerPtr->rbegin();
        }
    } else {
        if constexpr (isConst) {
            iter = containerPtr->cbegin();
        } else {
            iter = containerPtr->begin();
        }
    }
}

template <typename T>
template <bool isConst, bool isRev>
bool TSLQueue<T>::TSLQIterWrapper<isConst, isRev>::isValid() const {
    return !iterValid.expired();
}

template <typename T>
template <bool isConst, bool isRev>
bool TSLQueue<T>::TSLQIterWrapper<isConst, isRev>::next() {
    if(!isValid()) {
        return false;
    }

    if constexpr (isRev) {
        if(containerPtr->rend() == iter) {
            iterValid.reset();
            return false;
        } else {
            ++iter;
        }
    } else {
        if(containerPtr->end() == iter) {
            iterValid.reset();
            return false;
        } else {
            ++iter;
        }
    }

    return true;
}

template <typename T>
template <bool isConst, bool isRev>
bool TSLQueue<T>::TSLQIterWrapper<isConst, isRev>::prev() {
    if(!isValid()) {
        return false;
    }

    if constexpr (isRev) {
        if(containerPtr->rbegin() == iter) {
            iterValid.reset();
            return false;
        } else {
            --iter;
        }
    } else {
        if(containerPtr->begin() == iter) {
            iterValid.reset();
            return false;
        } else {
            --iter;
        }
    }

    return true;
}

template <typename T>
template <bool isConst, bool isRev>
std::optional<T> TSLQueue<T>::TSLQIterWrapper<isConst, isRev>::current() {
    if(!isValid()) {
        return std::nullopt;
    } else {
        if constexpr (isRev) {
            if(containerPtr->rend() == iter) {
                return std::nullopt;
            }
        } else {
            if(containerPtr->end() == iter) {
                return std::nullopt;
            }
        }
    }

    return *iter;
}

template <typename T>
typename TSLQueue<T>::template TSLQIterWrapper<false, false> TSLQueue<T>::iter() {
    return TSLQIterWrapper<false, false>(&container, iterValid, iterWrapperCount);
}

template <typename T>
typename TSLQueue<T>::template TSLQIterWrapper<false, true> TSLQueue<T>::riter() {
    return TSLQIterWrapper<false, true>(&container, iterValid, iterWrapperCount);
}

template <typename T>
typename TSLQueue<T>::template TSLQIterWrapper<true, false> TSLQueue<T>::citer() {
    return TSLQIterWrapper<true, false>(&container, iterValid, iterWrapperCount);
}

template <typename T>
typename TSLQueue<T>::template TSLQIterWrapper<true, true> TSLQueue<T>::criter() {
    return TSLQIterWrapper<true, true>(&container, iterValid, iterWrapperCount);
}

#endif
