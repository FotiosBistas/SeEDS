#include "message_queue.h"

std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> MessageQueue::readMessage() {
    std::unique_lock<std::mutex> lock(mtx);
    // Wait until the queue is not empty or shutdown is requested.
    cv.wait(lock, [this] { return !queue.empty() || shutdownFlag.load(); });

    // If shutdown is signaled and the queue is empty, return nullptr.
    if (queue.empty()) 
    {
        return nullptr;
    }

    auto msg = std::move(queue.front());
    queue.pop_front();
    return msg;
}

std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> MessageQueue::tryReadMessage() {
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.empty()) 
    {
        return nullptr;
    }

    auto msg = std::move(queue.front());
    queue.pop_front();
    return msg;
}

void MessageQueue::writeMessage(std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> msg) {
    {
        // Lock the queue before modifying it and release when we go outside of scope.
        std::lock_guard<std::mutex> lock(mtx);
        queue.push_back(std::move(msg));
    }
    // Notify one waiting thread that a new message is available.
    cv.notify_one();
}

size_t MessageQueue::size() const 
{
    std::lock_guard<std::mutex> lock(mtx);
    return queue.size();
}

void MessageQueue::shutdown() 
{
    // Set the shutdown flag to true.
    shutdownFlag.store(true);
    // Notify all threads waiting on the condition variable so they can check the shutdown state.
    cv.notify_all();
}
