#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "snds_message.hpp"

/**
 * @brief A thread-safe producer-consumer message queue.
 *
 * The MessageQueue class provides a thread-safe mechanism for producers and consumers
 * to exchange messages. Internally, it uses a std::deque to store messages along with a 
 * std::mutex and std::condition_variable for synchronization. A shutdown flag is included 
 * to allow the queue to break out of waiting when the service is shutting down.
 */
class MessageQueue {
private:
    std::deque<std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE>> queue; ///< The underlying queue storing messages.
    mutable std::mutex mtx;                                ///< Mutex to protect access to the queue.
    std::condition_variable cv;                            ///< Condition variable for synchronization.
    std::atomic<bool> shutdownFlag { false };            ///< Flag to signal shutdown for waiting threads.

    inline nlohmann::json payload_to_json_or_string(const std::string& s) 
    {
        if (nlohmann::json::accept(s)) 
        {
            nlohmann::json j = nlohmann::json::parse(s, nullptr, /*allow_exceptions=*/false);
            if (!j.is_discarded()) return j;
        }
        // Fallback when not valid JSON: carry raw text
        return nlohmann::json
        {
            {"raw", s}
        };
    }
public:
    /**
     * @brief Reads a message from the queue.
     *
     * This function blocks until there is a message available in the queue or 
     * until the shutdown flag is set to true. If the shutdown flag is set and the queue is empty,
     * the function returns a nullptr.
     *
     * @return A unique_ptr to a SNDS_INTRA_MESSAGE if available; otherwise, nullptr.
     */
    std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> readMessage();

    std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> tryReadMessage(); // Non-blocking

    /**
     * @brief Writes a message to the queue.
     *
     * This function adds the given message to the back of the queue and notifies one 
     * waiting consumer thread.
     *
     * @param msg A unique_ptr to the SNDS_INTRA_MESSAGE to add to the queue.
     */
    virtual void writeMessage(std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> msg);

    std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> tryReadAll();
    std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> waitAndReadAll();

    /**
     * @brief Returns the current size of the queue.
     *
     * This function locks the queue and returns the number of messages currently stored.
     *
     * @return The number of messages in the queue.
     */
    size_t size() const;

    /**
     * @brief Signals the queue to shutdown.
     *
     * This function sets the shutdown flag to true and notifies all waiting threads so that 
     * they can exit gracefully even if the queue is empty.
     */
    void shutdown();
};
