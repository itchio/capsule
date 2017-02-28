#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class LockingQueue
{
public:
    void push(T const& _data)
    {
        {
            std::lock_guard<std::mutex> lock(guard);
            queue.push(_data);
        }
        signal.notify_one();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(guard);
        return queue.empty();
    }

    bool tryPop(T& _value)
    {
        std::lock_guard<std::mutex> lock(guard);
        if (queue.empty())
        {
            return false;
        }

        _value = queue.front();
        queue.pop();
        return true;
    }

    void waitAndPop(T& _value)
    {
        std::unique_lock<std::mutex> lock(guard);
        while (queue.empty())
        {
            signal.wait(lock);
        }

        _value = queue.front();
        queue.pop();
    }

    bool tryWaitAndPop(T& _value, int _milli)
    {
        std::unique_lock<std::mutex> lock(guard);
        while (queue.empty())
        {
            signal.wait_for(lock, std::chrono::milliseconds(_milli));
            return false;
        }

        _value = queue.front();
        queue.pop();
        return true;
    }

private:
    std::queue<T> queue;
    mutable std::mutex guard;
    std::condition_variable signal;
};