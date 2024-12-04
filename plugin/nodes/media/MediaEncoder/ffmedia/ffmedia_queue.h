#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

typedef void (*pfn_release)(void*);

class FFMedia_Queue
{
protected:
    // Data
    std::queue<void*> _queue;
    typename std::queue<void*>::size_type _size_max;

    // Thread gubbins
    std::mutex _mutex;
    std::condition_variable _fullQue;
    std::condition_variable _empty;

    // Exit
    // 原子操作
    std::atomic_bool _quit  ATOMIC_FLAG_INIT;
    std::atomic_bool _finished  ATOMIC_FLAG_INIT;
    pfn_release release_func;

public:
    FFMedia_Queue(const size_t size_max, pfn_release func = nullptr) : _size_max(size_max)
    {
        _quit = false;
        _finished = false;
        release_func = func;
    }

    bool push(void* data, bool block = false)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_quit && !_finished)
        {
            if (_queue.size() < _size_max)
            {
                _queue.push(data);
                _empty.notify_all();
                return true;
            }
            else
            {
                if (block)
                {
                    // wait的时候自动释放锁，如果wait到了会获取锁
                    _fullQue.wait(lock);
                }
                else
                {
                    void* to_remove = _queue.front();
                    if (release_func)
                        release_func(to_remove);
                    _queue.pop();
                    _queue.push(data);
                    return false;
                }
            }
        }

        return false;
    }

    bool pop(void*& data, bool block = true)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_quit)
        {
            if (!_queue.empty())
            {
                // data = std::move(_queue.front());
                data = _queue.front();
                _queue.pop();

                _fullQue.notify_all();
                return true;
            }
            else if (_queue.empty() && _finished)
            {
                return false;
            }
            else
            {
                if (block)
                {
                    _empty.wait(lock);
                }
                else
                {
                    return false;
                }
            }
        }
        return false;
    }

    bool flush()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_queue.empty())
        {
            void* to_remove = _queue.front();
            if (release_func)
                release_func(to_remove);
            _queue.pop();
        }
        _fullQue.notify_all();
        return true;
    }

    // The queue has finished accepting input
    void stop()
    {
        _finished = true;
        _empty.notify_all();
    }

    void quit()
    {
        _quit = true;
        stop();
        flush();
        _empty.notify_all();
        _fullQue.notify_all();
    }

    int length()
    {
        return static_cast<int>(_queue.size());
    }
};
