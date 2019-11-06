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
        TSLQIter(std::mutex &mutex,
                 std::weak_ptr<TSLQNode> currentNode,
                 unsigned long long *msize);
        ~TSLQIter();

        std::optional<T> current();
        bool next();
        bool prev();
        bool remove();

    private:
        std::lock_guard<std::mutex> lock;
        std::weak_ptr<TSLQNode> currentNode;
        unsigned long long *msize;

    };

  public:
    TSLQIter begin();

  private:
    std::mutex mutex;
    std::shared_ptr<TSLQNode> head;
    std::shared_ptr<TSLQNode> tail;
    unsigned long long msize;
};

template <typename T>
TSLQueue<T>::TSLQueue() :
    head(std::make_shared<TSLQNode>()),
    tail(std::make_shared<TSLQNode>()),
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
TSLQueue<T>::TSLQueue(TSLQueue &&other)
{
    std::lock_guard lock(other.mutex);
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
TSLQueue<T> & TSLQueue<T>::operator=(TSLQueue &&other) {
    std::scoped_lock lock(mutex, other.mutex);
    head = std::move(other.head);
    tail = std::move(other.tail);
    msize = std::move(other.msize);
}

template <typename T>
void TSLQueue<T>::push(const T &data) {
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
    if(mutex.try_lock()) {
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
    if(mutex.try_lock()) {
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
    std::lock_guard lock(mutex);
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
std::optional<T> TSLQueue<T>::top_and_pop() {
    std::optional<T> ret = std::nullopt;
    std::lock_guard lock(mutex);
    if(head->next != tail) {
        assert(head->next->data);
        ret = *head->next->data.get();

        auto& newNext = head->next->next;
        newNext->prev = head;
        head->next = newNext;
        assert(msize > 0);
        --msize;
    }
    return ret;
}

template <typename T>
std::optional<T> TSLQueue<T>::top_and_pop_and_empty(bool *isEmpty) {
    std::optional<T> ret = std::nullopt;
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

        if(isEmpty) {
            *isEmpty = head->next == tail;
        }
    }
    return ret;
}

template <typename T>
void TSLQueue<T>::clear() {
    std::lock_guard lock(mutex);

    head->next = tail;
    tail->prev = head;
    msize = 0;
}

template <typename T>
bool TSLQueue<T>::empty() {
    std::lock_guard lock(mutex);
    return head->next == tail;
}

template <typename T>
unsigned long long TSLQueue<T>::size() {
    std::lock_guard lock(mutex);
    return msize;
}

template <typename T>
TSLQueue<T>::TSLQNode::TSLQNode() :
type(TSLQN_Type::TSLQN_NORMAL)
{}

template <typename T>
bool TSLQueue<T>::TSLQNode::isNormal() const {
    return type == TSLQN_Type::TSLQN_NORMAL;
}

template <typename T>
TSLQueue<T>::TSLQIter::TSLQIter(std::mutex &mutex,
                                std::weak_ptr<TSLQNode> currentNode,
                                unsigned long long *msize) :
lock(mutex),
currentNode(currentNode),
msize(msize)
{
}

template <typename T>
TSLQueue<T>::TSLQIter::~TSLQIter() {
}

template <typename T>
std::optional<T> TSLQueue<T>::TSLQIter::current() {
    std::shared_ptr<TSLQNode> currentNode = this->currentNode.lock();
    assert(currentNode);
    if(currentNode->isNormal()) {
        return *currentNode->data.get();
    } else {
        return std::nullopt;
    }
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
    std::shared_ptr<TSLQNode> currentNode = this->currentNode.lock();
    assert(currentNode);
    if(!currentNode->isNormal()) {
        return false;
    }

    this->currentNode = currentNode->next;
    auto parent = currentNode->prev.lock();
    assert(parent);

    currentNode->next->prev = parent;
    parent->next = currentNode->next;

    assert(*msize > 0);
    --(*msize);

    return parent->next->isNormal();
}

template <typename T>
typename TSLQueue<T>::TSLQIter TSLQueue<T>::begin() {
    return TSLQIter(mutex, head->next, &msize);
}

#endif
