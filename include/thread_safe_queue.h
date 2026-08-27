#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

/*  A queue that many threads can safely push into and pop from at the same time.
    Tailer threads push log lines in, the rule engine thread pops them out.  */

template <typename ItemType>
class ThreadSafeQueue{
public:

    void pushItem( ItemType newItem ){
        std:: lock_guard <std::mutex> lockGuard(queueMutex);
        internalQueue.push(newItem);
        queueNotEmptySignal.notify_one();
    }

    // blocks (sleeps) if the queue is empty instead of busy-checking

    ItemType popItem(){
        std:: unique_lock <std::mutex> uniqueLock(queueMutex);
        queueNotEmptySignal.wait(uniqueLock, [this](){
            return !internalQueue.empty();
        });

        ItemType frontItem = internalQueue.front();
        internalQueue.pop();
        return frontItem;
    }

    //  Returns how many items are currently waiting in queue.

    size_t currentSize(){
        std:: lock_guard <std::mutex> lockGuard(queueMutex);
        return internalQueue.size();
    }

private:
    std:: queue<ItemType> internalQueue;
    std:: mutex queueMutex;
    std::condition_variable queueNotEmptySignal;
};

