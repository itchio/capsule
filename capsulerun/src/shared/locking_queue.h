#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class LockingQueue
{
public:
    void Push(T const& data)
    {
        {
            std::lock_guard<std::mutex> lock(guard_);
            queue_.push(data);
        }
        signal_.notify_one();
    }

    bool Empty() const
    {
        std::lock_guard<std::mutex> lock(guard_);
        return queue_.empty();
    }

    bool TryPop(T& value)
    {
        std::lock_guard<std::mutex> lock(guard_);
        if (queue_.empty())
        {
            return false;
        }

        value = queue_.front();
        queue_.pop();
        return true;
    }

    void WaitAndPop(T& value)
    {
        std::unique_lock<std::mutex> lock(guard_);
        while (queue_.empty())
        {
            signal_.wait(lock);
        }

        value = queue_.front();
        queue_.pop();
    }

    bool TryWaitAndPop(T& value, int milli)
    {
        std::unique_lock<std::mutex> lock(guard_);
        while (queue_.empty())
        {
            signal_.wait_for(lock, std::chrono::milliseconds(milli));
            return false;
        }

        value = queue_.front();
        queue_.pop();
        return true;
    }

private:
    std::queue<T> queue_;
    mutable std::mutex guard_;
    std::condition_variable signal_;
};