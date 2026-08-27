#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include "../include/thread_safe_queue.h"
#include "../include/lock_free_queue.h"

constexpr int itemsPerProducer = 100000;
constexpr int producerThreadCount = 4;
constexpr int totalItemsExpected = itemsPerProducer * producerThreadCount;

// Holds everything we measured from one benchmark run.
struct BenchmarkResult {
    long long totalDurationMs;
    long long p50LatencyNs;
    long long p90LatencyNs;
    bool allItemsAccountedFor;
};

// each item carries a unique ID so the consumer can verify no losses/duplicates

template <typename QueueType>
BenchmarkResult runBenchmark(QueueType& queue) {
    auto startTime = std::chrono::steady_clock::now();

    // each producer records its own latencies locally so threads don't contend just to log a timestamp

    std::vector<std::vector<long long>> perThreadLatencies(producerThreadCount);

    std::vector<std::thread> producerThreads;
    for (int producerIndex = 0; producerIndex < producerThreadCount; producerIndex = producerIndex + 1) {
        producerThreads.push_back(std::thread([&queue, &perThreadLatencies, producerIndex]() {
            perThreadLatencies[producerIndex].reserve(itemsPerProducer);
            for (int itemIndex = 0; itemIndex < itemsPerProducer; itemIndex = itemIndex + 1) {
                int uniqueItemId = producerIndex * itemsPerProducer + itemIndex;

                auto pushStart = std::chrono::steady_clock::now();
                queue.pushItem(uniqueItemId);
                auto pushEnd = std::chrono::steady_clock::now();

                long long latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(pushEnd - pushStart).count();
                perThreadLatencies[producerIndex].push_back(latencyNs);
            }
        }));
    }

    // marks off every item ID seen, to confirm each arrived exactly once
    std::vector<bool> itemWasReceived(totalItemsExpected, false);
    bool duplicateDetected = false;

    std::thread consumerThread([&queue, &itemWasReceived, &duplicateDetected]() {
        for (int receivedCount = 0; receivedCount < totalItemsExpected; receivedCount = receivedCount + 1) {
            int receivedId = queue.popItem();
            if (itemWasReceived[receivedId]) {
                duplicateDetected = true;
            }
            itemWasReceived[receivedId] = true;
        }
    });

    for (int producerIndex = 0; producerIndex < producerThreadCount; producerIndex = producerIndex + 1) {
        producerThreads[producerIndex].join();
    }
    consumerThread.join();

    auto endTime = std::chrono::steady_clock::now();

    // Merge every thread's latency measurements into one list, then sort it so we can read off percentiles directly by index.
    std::vector<long long> allLatencies;
    allLatencies.reserve(totalItemsExpected);
    for (int producerIndex = 0; producerIndex < producerThreadCount; producerIndex = producerIndex + 1) {
        for (long long latency : perThreadLatencies[producerIndex]) {
            allLatencies.push_back(latency);
        }
    }
    std::sort(allLatencies.begin(), allLatencies.end());

    bool allReceived = true;
    for (int itemIndex = 0; itemIndex < totalItemsExpected; itemIndex = itemIndex + 1) {
        if (!itemWasReceived[itemIndex]) {
            allReceived = false;
        }
    }

    BenchmarkResult result;
    result.totalDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    result.p50LatencyNs = allLatencies[allLatencies.size() * 50 / 100];
    result.p90LatencyNs = allLatencies[allLatencies.size() * 90 / 100];
    result.allItemsAccountedFor = allReceived && !duplicateDetected;

    return result;
}

void printResult(const std::string& queueName, const BenchmarkResult& result) {
    std::cout << queueName << ":" << std::endl;
    std::cout << "  Total time:        " << result.totalDurationMs << " ms" << std::endl;
    std::cout << "  p50 push latency:  " << result.p50LatencyNs << " ns" << std::endl;
    std::cout << "  p90 push latency:  " << result.p90LatencyNs << " ns" << std::endl;
    std::cout << "  Correctness check: " << (result.allItemsAccountedFor ? "PASSED (no lost or duplicated items)" : "FAILED") << std::endl;
}

int main() {
    std::cout << "Running benchmark: " << totalItemsExpected
              << " items, " << producerThreadCount << " producer threads, 1 consumer thread." << std::endl;
    std::cout << std::endl;

    ThreadSafeQueue<int> lockedQueue;
    BenchmarkResult lockedResult = runBenchmark(lockedQueue);
    printResult("ThreadSafeQueue (mutex-based)", lockedResult);

    std::cout << std::endl;

    LockFreeQueue<int> lockFreeQueue;
    BenchmarkResult lockFreeResult = runBenchmark(lockFreeQueue);
    printResult("LockFreeQueue (CAS-based)", lockFreeResult);

    return 0;
}