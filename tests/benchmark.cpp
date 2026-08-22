#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "../include/thread_safe_queue.h"
#include "../include/lock_free_queue.h"

constexpr int itemsPerProducer = 100000;
constexpr int producerThreadCount = 4;

/* Runs the same push/pop workload against whichever queue type is passed in,
   and returns how long the whole run took in milliseconds.    */

template <typename QueueType>
long long runBenchmark(QueueType& queue) {
    int totalItemsExpected = itemsPerProducer * producerThreadCount;

    auto startTime = std::chrono::steady_clock::now();

    std::vector<std::thread> producerThreads;
    for (int producerIndex = 0; producerIndex < producerThreadCount; producerIndex = producerIndex + 1) {
        producerThreads.push_back(std::thread([&queue]() {
            for (int itemIndex = 0; itemIndex < itemsPerProducer; itemIndex = itemIndex + 1) {
                queue.pushItem(itemIndex);
            }
        }));
    }

    std::thread consumerThread([&queue, totalItemsExpected]() {
        for (int receivedCount = 0; receivedCount < totalItemsExpected; receivedCount = receivedCount + 1) {
            queue.popItem();
        }
    });

    for (int producerIndex = 0; producerIndex < producerThreadCount; producerIndex = producerIndex + 1) {
        producerThreads[producerIndex].join();
    }
    consumerThread.join();

    auto endTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

int main() {
    std::cout << "Running benchmark: " << (itemsPerProducer * producerThreadCount)
              << " items, " << producerThreadCount << " producer threads, 1 consumer thread." << std::endl;

    ThreadSafeQueue<int> lockedQueue;
    long long lockedDuration = runBenchmark(lockedQueue);
    std::cout << "ThreadSafeQueue (mutex-based): " << lockedDuration << " ms" << std::endl;

    LockFreeQueue<int> lockFreeQueue;
    long long lockFreeDuration = runBenchmark(lockFreeQueue);
    std::cout << "LockFreeQueue (CAS-based):     " << lockFreeDuration << " ms" << std::endl;

    return 0;
}