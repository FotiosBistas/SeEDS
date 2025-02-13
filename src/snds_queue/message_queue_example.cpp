#include "message_queue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

// Producer Function
void producer(MessageQueue& queue, int producerId, int numMessages) {
    for (int i = 0; i < numMessages; ++i) {
        // Create a new message with a unique ID and payload
        auto msg = std::make_unique<SNDS_INTRA_MESSAGE>(
            "Payload from producer " + std::to_string(producerId * 100 + i),
            SNDS_INTRA_MESSAGE_TYPE::GENERIC,
            SNDS_CLIENT_TYPES::PRODUCER,
            "Message_" + std::to_string(producerId * 100 + i));

        // Debug log: producer producing message
        std::cout << "[Producer " << producerId << "] Produced message: \n";
        msg->printDetails(); // Use the debug-friendly `printDetails`

        // Add the message to the queue
        queue.writeMessage(std::move(msg));

        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Consumer Function
void consumer(MessageQueue& queue, int consumerId, int numMessagesToConsume) {
    for (int i = 0; i < numMessagesToConsume; ++i) {
        // Retrieve a message from the queue
        auto msg = queue.readMessage();

        // Debug log: consumer consuming message
        std::cout << "[Consumer " << consumerId << "] Consumed message: \n";
        msg->printDetails(); // Use the debug-friendly `printDetails`

        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

// Main Function
int main() {
    MessageQueue queue;

    // Create producers and consumers
    std::thread producer1(producer, std::ref(queue), 1, 5); // Producer 1 generates 5 messages
    std::thread producer2(producer, std::ref(queue), 2, 5); // Producer 2 generates 5 messages
    std::thread consumer1(consumer, std::ref(queue), 1, 5); // Consumer 1 processes 5 messages
    std::thread consumer2(consumer, std::ref(queue), 2, 5); // Consumer 2 processes 5 messages

    // Wait for all threads to finish
    producer1.join();
    producer2.join();
    consumer1.join();
    consumer2.join();

    std::cout << "All messages have been processed." << std::endl;

    return 0;
}
