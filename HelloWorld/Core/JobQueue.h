#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "PacketJob.h"

class JobQueue
{
public:
    void Push(PacketJob job)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(job));
        }
        _cv.notify_one();
    }

    // blocking Pop, _stopping이면 false 리턴
    bool Pop(PacketJob& outJob)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return _stopping || !_queue.empty(); });

        if (_stopping && _queue.empty())
            return false;

        outJob = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopping = true;
        }
        _cv.notify_all();
    }

private:
    std::mutex _mutex{};
    std::condition_variable _cv{};
    std::queue<PacketJob> _queue{};
    bool _stopping = false;
};
