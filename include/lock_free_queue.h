#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

/*  A queue built on the Michael-Scott lock-free design. Push and pop operate through compare-and-swap instead of a mutex,
    so threads never block each other, they just retry if another thread got there first.  */

template <typename ItemType>
class LockFreeQueue {
private:
    struct Node {
        ItemType data;
        std::atomic<Node*> next;

        Node() : next(nullptr) {}
        Node(ItemType value) : data(value), next(nullptr) {}
    };

    std::atomic<Node*> headPointer;
    std::atomic<Node*> tailPointer;

    /* These exist purely to let popItem sleep when the queue is empty instead of spinning. 
       They never guard the queue's actual data, push and pop stay fully lock-free through compare-and-swap.    */
    std::mutex waitMutex;
    std::condition_variable dataAvailableSignal;

public:
    LockFreeQueue() {
        /* The queue always starts with one empty dummy node,
           so head and tail are never null and pop/push don't need a separate "queue is empty" case.   */

        Node* dummyNode = new Node();
        headPointer.store(dummyNode);
        tailPointer.store(dummyNode);
    }

    void pushItem(ItemType item) {
        Node* newNode = new Node(item);

        while (true) {
            Node* lastNode = tailPointer.load();
            Node* nextNode = lastNode->next.load();

            // Make sure tail hasn't moved since we read it a moment ago.
            if (lastNode == tailPointer.load()) {
                if (nextNode == nullptr) {
                    // Tail is genuinely the last node, try to attach here.

                    if (lastNode->next.compare_exchange_weak(nextNode, newNode)) {
                        /*  Attached successfully, now try to advance tail to match.
                         If this fails, another thread will finish the job for us.  */

                        tailPointer.compare_exchange_weak(lastNode, newNode);
                        break;
                    }
                } else {
                    /* Tail is lagging behind an already-attached node,
                     help move it forward before retrying our own attempt.  */

                    tailPointer.compare_exchange_weak(lastNode, nextNode);
                }
            }
        }

        // Wake up one sleeping consumer thread, if any, now that there's a new item available.
        std::lock_guard<std::mutex> notifyLock(waitMutex);
        dataAvailableSignal.notify_one();
    }

    ItemType popItem() {
        while (true) {
            Node* firstNode = headPointer.load();
            Node* lastNode = tailPointer.load();
            Node* nextNode = firstNode->next.load();

            // Make sure head hasn't moved since we read it a moment ago.
            if (firstNode == headPointer.load()) {
                if (firstNode == lastNode) {
                    if (nextNode == nullptr) {

                        // Queue is empty, sleep here instead of spinning, and wake back up once pushItem signals us.
                        std::unique_lock<std::mutex> waitLock(waitMutex);
                        dataAvailableSignal.wait(waitLock, [this]() {
                            return headPointer.load()->next.load() != nullptr;
                        });
                        continue;
                    }

                    // Tail is lagging behind, help move it forward and retry.
                    tailPointer.compare_exchange_weak(lastNode, nextNode);
                } else {

                    // There is a real item waiting, grab its value first.
                    ItemType value = nextNode->data;
                    if (headPointer.compare_exchange_weak(firstNode, nextNode)) {

                        /* Intentionally not deleting firstNode here. A fully safe reclamation strategy needs hazard pointers or epoch-based
                           reclamation so other threads can tell when a node is truly unreferenced. Without that, deleting immediately
                           causes a use-after-free race under contention, confirmed by ThreadSanitizer. Leaking the node trades memory growth
                           for correctness, which is the safe choice at this scope. */
                        return value;
                    }
                }
            }
        }
    }
    
};