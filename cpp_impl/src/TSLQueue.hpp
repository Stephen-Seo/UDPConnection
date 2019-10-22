#ifndef UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP
#define UDPC_THREADSAFE_LINKEDLIST_QUEUE_HPP

#include <memory>
#include <mutex>
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
    std::optional<T> top();
    bool pop();
    std::optional<T> top_and_pop();
    std::optional<T> top_and_pop_and_empty(bool *isEmpty);
    void clear();

    bool empty();

    template <bool isConst, bool isRev>
    class TSLQIterWrapper {
      public:
        TSLQIterWrapper(std::conditional_t<isConst, const std::list<T>, std::list<T>> *container, std::weak_ptr<void> iterValid);

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
    };

  private:
    std::shared_ptr<void> iterValid;
};

// implementation

template <typename T>
TSLQueue<T>::TSLQueue() {
}

template <typename T>
TSLQueue<T>::~TSLQueue() {
}

template <typename T>
TSLQueue<T>::TSLQueue(TSLQueue &&other) {
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
}

template <typename T>
template <bool isConst, bool isRev>
TSLQueue<T>::TSLQIterWrapper<isConst, isRev>::TSLQIterWrapper(
        std::conditional_t<isConst, const std::list<T>, std::list<T>> *container,
        std::weak_ptr<void> iterValid) :
            containerPtr(container),
            iterValid(iterValid) {
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

#endif
