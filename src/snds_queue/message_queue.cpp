#include "message_queue.hpp"

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


std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> MessageQueue::waitAndReadAll() {
    std::deque<std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE>> local;

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !queue.empty() || shutdownFlag.load(); });
        if (queue.empty())     // awakened by shutdown and nothing to read
            return nullptr;
        local.swap(queue);     // drain atomically
    }

    // Build batch payload
    nlohmann::json arr = nlohmann::json::array();
    arr.get_ref<nlohmann::json::array_t&>().reserve(local.size());
    while (!local.empty()) 
    {
        auto msg = std::move(local.front());
        local.pop_front();

        nlohmann::json item;
        item["name"]   = msg->get_message();
        item["type"]   = snds_intra_message::to_string(msg->get_message_type());
        item["sender"] = static_cast<int>(msg->get_sender()); // or to_string() if you have it
        item["payload"]= payload_to_json_or_string(msg->get_payload());

        arr.push_back(std::move(item));
    }

    nlohmann::json batch
    {
        {"batch_size", arr.size()},
        {"messages",   std::move(arr)}
    };

    std::string combined = batch.dump(2);
    combined.push_back('\n'); // keep your line-delimited convention

    // Use GENERIC as a safe envelope type and a recognizable name
    return std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        "BATCH",
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GENERIC,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        std::move(combined),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );
}

std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> MessageQueue::tryReadAll() {
    std::deque<std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE>> local;

    {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) return nullptr;
        local.swap(queue);
    }

    nlohmann::json arr = nlohmann::json::array();
    arr.get_ref<nlohmann::json::array_t&>().reserve(local.size());
    while (!local.empty()) 
    {
        auto msg = std::move(local.front());
        local.pop_front();

        nlohmann::json item;
        item["name"]   = msg->get_message();
        item["type"]   = snds_intra_message::to_string(msg->get_message_type());
        item["sender"] = static_cast<int>(msg->get_sender());
        item["payload"]= payload_to_json_or_string(msg->get_payload());

        arr.push_back(std::move(item));
    }

    nlohmann::json batch
    {
        {"batch_size", arr.size()},
        {"messages",   std::move(arr)}
    };

    std::string combined = batch.dump(2);
    combined.push_back('\n');

    return std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        "BATCH",
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GENERIC,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        std::move(combined),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );
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
