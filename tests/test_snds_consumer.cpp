#include "snds_producer.h"

#include <gtest/gtest.h>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/name.hpp>
#include <spdlog/spdlog.h>

// New message queue class to override writeMessage(),
// so that messages are stored in a vector for later retrieval.
class TestMessageQueue : public MessageQueue {
    public:
        std::vector<std::unique_ptr<SNDS_INTRA_MESSAGE>> capturedMessages;

        void writeMessage(std::unique_ptr<SNDS_INTRA_MESSAGE> msg) override {
            capturedMessages.push_back(std::move(msg));
        }

        void clear() {
            capturedMessages.clear();
        }
};

// New consumer class to use the TestMessageQueue.
class TestSNDSConsumer : public SNDSConsumer {
    public:
        // Setter to allow injecting the test message queue.
        void setServiceQueue(std::unique_ptr<MessageQueue> mq) {
            v_to_service = std::move(mq);
        }
};

// Test fixture that ensures each test starts with a clean logger.
class SNDSConsumerTestFixture : public ::testing::Test {
    protected:
      void SetUp() override {
        // Drop logger "Consumer" if it exists to avoid duplicate logger errors.
        if (spdlog::get("Consumer")) {
            spdlog::drop("Consumer");
        }
      }
};

// Test that it properly handles a valid data packet.
TEST_F(SNDSConsumerTestFixture, OnData_HandlesValidMessage) {
    // Create a TestMessageQueue to use
    auto testQueue = std::make_unique<TestMessageQueue>();
    TestMessageQueue* testQueuePtr = testQueue.get();

    TestSNDSConsumer consumer;
    consumer.setServiceQueue(std::move(testQueue));

    // Create an ndn::Name with at least two components.
    ndn::Name dataName("/SNDS/message/info");
    std::string content = "data";

    // Create a Data packet with the name and content.
    ndn::Data data(dataName);
    data.setContent(reinterpret_cast<const uint8_t*>(content.data()), content.size());

    // Create an Interest.
    ndn::Interest interest(dataName);

    consumer.onData(interest, data);

    // writeMessage() should have been called,
    // adding at least one message to the testQueue.
    ASSERT_FALSE(testQueuePtr->capturedMessages.empty());

    // Examine the captured message.
    auto& msgPtr = testQueuePtr->capturedMessages.front();
    ASSERT_NE(msgPtr, nullptr);

    ASSERT_EQ(msgPtr->getPayload(), "content");
    ASSERT_EQ(msgPtr->getMessageName(), "message");
}
