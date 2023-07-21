// Author     : Teo.Sun
// Date       : 2019-09-27
// Description: Provide a lockfree ringbuffer

#pragma once
#pragma warning(disable : 4996)

#include <boost/lockfree/spsc_queue.hpp>
#include <condition_variable>
#include <iostream>
#include <vector>

template <class T>
class lockfree_spsc_queue
{
public:
    /** Constructs a spsc_queue for element_count elements
     *
     *  \pre spsc_queue must be configured to be sized at run-time
     */
    lockfree_spsc_queue(std::size_t element_count)
        : capacity_(element_count)
        , spsc_queue_(element_count)
    {
    }
    virtual ~lockfree_spsc_queue() = default;

public:
    /** Pushes object t to the ringbuffer.
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \post object will be pushed to the spsc_queue, unless it is full.
     * \return true, if the push operation is successful.
     *
     * \note Thread-safe and wait-free
     * */
    bool push(T const &t)
    {
        bool ret = spsc_queue_.push(t);
        if (ret)
        {
            used_size_++;
        }
        condition_.notify_one();
        return ret;
    }
    /** Pushes as many objects from the range [begin, end) as there is space .
     *
     * \pre only one thread is allowed to push data to the spsc_queue
     * \return iterator to the first element, which has not been pushed
     *
     * \note Thread-safe and wait-free
     */
    template <typename ElementIterator>
    ElementIterator push(ElementIterator begin, ElementIterator end)
    {
        auto iter = spsc_queue_.push(begin, end);
        used_size_ += iter - begin;
        condition_.notify_one();
        return iter;
    }

    /** Pushes object t to the ringbuffer.
     *
     * \pre only one thread is allowed to push data to the spsc_queue.
     *
     * \note Thread-safe and wait when the spsc_queue is full.
     * */
    void push_wait(const T &t)
    {
        while (!quit_ && !spsc_queue_.push(t))
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock);
        }
        used_size_++;
        condition_.notify_one();
    }

    /** Pops a maximum of size objects from ringbuffer.
     *
     * \pre only one thread is allowed to pop data to the spsc_queue.
     * \return number of popped items
     *
     * \note Thread-safe and wait when the spsc_queue is empty.
     * */
    std::shared_ptr<std::vector<T>> pop(bool wait = true)
    {
        if (wait && spsc_queue_.empty())
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock);
        }
        auto pop_size = spsc_queue_.read_available();
        auto pop_data = std::make_shared<std::vector<T>>(pop_size);
        spsc_queue_.pop(pop_data->data(), pop_size);
        used_size_ -= pop_size;
        condition_.notify_one();
        return pop_data;
    }

    /** Pops a range-size objects from ringbuffer.
     *
     * \pre only one thread is allowed to pop data to the spsc_queue.\
     * \return number of popped items
     *
     * \note Thread-safe and wait when the spsc_queue is less than min.
     * */
    std::shared_ptr<std::vector<T>> pop(size_t min, size_t max)
    {
        while (!quit_ && spsc_queue_.read_available() < min)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock);
        }
        auto pop_size = std::min(max, spsc_queue_.read_available());
        auto pop_data = std::make_shared<std::vector<T>>(pop_size);
        spsc_queue_.pop(pop_data->data(), pop_size);
        used_size_ -= pop_size;
        condition_.notify_one();
        return pop_data;
    }

    // note thread-safe
    size_t capacity() const
    {
        return capacity_;
    }
    size_t used_size() const
    {
        return used_size_;
    }

    void quit()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        quit_ = true;
        condition_.notify_all();
    }

    // note Not thread-safe
    void reset()
    {
        used_size_ = 0;
        quit_ = false;
        spsc_queue_.reset();
    }

private:
    std::atomic<size_t> capacity_;
    std::atomic<size_t> used_size_ = 0;
    std::atomic_bool quit_ = false;
    std::mutex mutex_;
    std::condition_variable condition_;
    boost::lockfree::spsc_queue<T> spsc_queue_;
};