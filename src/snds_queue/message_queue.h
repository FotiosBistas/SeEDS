#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "snds_message.h"

/**
 * @brief A thread-safe producer-consumer message queue.
 *
 * The `MessageQueue` class provides a thread-safe way to enqueue and dequeue
 * messages between producers and consumers. Internally, it uses a `std::deque`
 * to store messages and employs a `std::mutex` and `std::condition_variable`
 * for synchronization.
 */
class MessageQueue {
private:
    std::deque<std::unique_ptr<SNDS_INTRA_MESSAGE>> queue; ///< The underlying queue to hold messages.
    mutable std::mutex mtx;                            ///< Mutex to protect access to the queue.
    std::condition_variable cv;                        ///< Condition variable for synchronization.

public:
    /**
     * @brief Reads a message from the queue.
     *
     * This method blocks the caller if the queue is empty until a message is
     * available. It removes the message from the front of the queue.
     *
     * @return A unique pointer to the `SNDS_Intra_Msg` read from the queue.
     */
    std::unique_ptr<SNDS_INTRA_MESSAGE> readMessage();

    /**
     * @brief Writes a message to the queue.
     *
     * This method adds a message to the back of the queue and notifies one
     * waiting consumer thread, if any.
     *
     * @param msg A unique pointer to the `SNDS_Intra_Msg` to add to the queue.
     */
    virtual void writeMessage(std::unique_ptr<SNDS_INTRA_MESSAGE> msg);

    /**
     * @brief Gets the current size of the queue.
     *
     * This method returns the number of messages currently in the queue.
     * It is thread-safe and locks the queue before accessing it.
     *
     * @return The number of messages in the queue.
     */
    size_t size() const;

};

#endif // MESSAGE_QUEUE_H
