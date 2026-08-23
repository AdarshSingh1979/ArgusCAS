#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

/*  A queue that many threads can safely push into and pop from at the same time,
    without stepping on each other's toes.
    The tailer threads will push log line in, and the rule engine thread will pop them out.  */

template <typename ItemType>
class ThreadSafeQueue{
public:

    //  Add one item to the back of the queue.
    void pushItem( ItemType newItem ){
        std:: lock_guard <std::mutex> lockGuard(queueMutex);
        internalQueue.push(newItem);
        queueNotEmptySignal.notify_one();
    }

    /*  Removes and returns the items at the front of the queue.
        If the queue is empty, this call waits (sleeps) until something is pushed in,
        instead of wasting CPU checking again and again.    */

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

