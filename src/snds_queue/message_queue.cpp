#include "message_queue.h"

std::unique_ptr<SNDS_INTRA_MESSAGE> MessageQueue::readMessage() {
    std::unique_lock<std::mutex> lock(mtx);
    if(queue.empty()){
        return nullptr;
    }
    auto msg = std::move(queue.front());
    queue.pop_front();
    return msg;
}

void MessageQueue::writeMessage(std::unique_ptr<SNDS_INTRA_MESSAGE> msg) {
    {
        //lock is released when scope is destroyed
        std::lock_guard<std::mutex> lock(mtx);
        queue.push_back(std::move(msg));
    }
    cv.notify_one(); // Notify one waiting thread
}

size_t MessageQueue::size() const {
    std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
    return queue.size(); // Return the size of the queue
}