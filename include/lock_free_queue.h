#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

/*  Michael-Scott lock-free queue. push/pop manipulate the actual list through CAS, no mutex. Not fully lock-free end to end though -
    popItem blocks on a condvar when the queue is empty instead of spinning, see waitMutex below.   */

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

    // only for the empty-queue sleep in popItem, never touches the actual list
    std::mutex waitMutex;
    std::condition_variable dataAvailableSignal;

public:
    LockFreeQueue() {
        // dummy node so head/tail are never null
        Node* dummyNode = new Node();
        headPointer.store(dummyNode);
        tailPointer.store(dummyNode);
    }

    void pushItem(ItemType item) {
        Node* newNode = new Node(item);

        while (true) {
            Node* lastNode = tailPointer.load();
            Node* nextNode = lastNode->next.load();

            if (lastNode == tailPointer.load()) {
                if (nextNode == nullptr) {

                    if (lastNode->next.compare_exchange_weak(nextNode, newNode)) {
                        // attached, swing tail forward too - if this CAS fails another thread finishes it for us
                        tailPointer.compare_exchange_weak(lastNode, newNode);
                        break;
                    }
                } else {
                    // tail is one behind, help it catch up before retrying
                    tailPointer.compare_exchange_weak(lastNode, nextNode);
                }
            }
        }

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

                        std::unique_lock<std::mutex> waitLock(waitMutex);
                        dataAvailableSignal.wait(waitLock, [this]() {
                            return headPointer.load()->next.load() != nullptr;
                        });
                        continue;
                    }

                    // tail is behind, help move it and retry
                    tailPointer.compare_exchange_weak(lastNode, nextNode);
                } else {
                    ItemType value = nextNode->data;
                    if (headPointer.compare_exchange_weak(firstNode, nextNode)) {

                        /* not deleting firstNode - TSan caught a use-after-free here under contention.
                           proper fix is hazard pointers / epoch reclamation, out of scope for now,
                           so this leaks the node instead: no race, memory grows unbounded. */
                        return value;
                    }
                }
            }
        }
    }
    
};