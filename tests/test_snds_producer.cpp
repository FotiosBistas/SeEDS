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

// New producer class to override sendData(),
// in order to capture the payload.
class TestSNDSProducer : public SNDSProducer {
  public:
    bool sendDataCalled = false;
    std::string capturedPayload;

    // Setter to allow injecting the test message queue.
    void setConsumerQueue(MessageQueue* mq) {
      v_to_consumer = mq;
    }

    // Override sendData to capture the payload.
    bool sendData(const ndn::Name& name, std::string payload) override {
      sendDataCalled = true;
      capturedPayload = payload;
      return true;
    }
};

// Test fixture that ensures each test starts with a clean logger.
class SNDSProducerTestFixture : public ::testing::Test {
protected:
  void SetUp() override {
    // Drop logger "ServiceBase" if it exists to avoid duplicate logger errors.
    if (spdlog::get("ServiceBase")) {
    spdlog::drop("ServiceBase");
    }
  }
};

// Test the SNDSProducer constructor.
TEST_F(SNDSProducerTestFixture, SNDSProducer_Constructor) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that there is an empty entry for "/CAR".
  ASSERT_EQ(myTypesRegistry.count("/CAR"), 1);
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());
}

// Test that the producer ignores Interests with short names.
TEST_F(SNDSProducerTestFixture, OnInterest_IgnoresShortInterest) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry for "/CAR" is empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Create an Interest with only the service prefix.
  ndn::Name interestName("/SNDS");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Verify that the registry for "/CAR" is *still* empty
  // and sendData() was *not* called.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());
  ASSERT_FALSE(producer.sendDataCalled);
}

// Test that if no ID is provided the advertisement is ignored.
TEST_F(SNDSProducerTestFixture, ProcessTypeAdvertisement_IgnoresWhenIDMissing) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry for "/CAR" is empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Create an Interest with only the service prefix and type (no ID).
  ndn::Name interestName("/SNDS/CAR");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Since no ID is provided, sendData() should not have been called.
  ASSERT_FALSE(producer.sendDataCalled);

  // Verify that the registry for "/CAR" remains empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that processing an advertisement with a duplicate ID sends a warning.
TEST_F(SNDSProducerTestFixture, ProcessTypeAdvertisement_WarnsOnDuplicateID) {
  // Prepopulate the registry for "/CAR" with "car1".
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();
  myTypesRegistry["/CAR"].push_back("car1");

  // Verify that the registry for "/CAR" has one ID.
  ASSERT_EQ(myTypesRegistry["/CAR"].size(), 1);

  // Create an Interest that advertises the same ID.
  ndn::Name interestName("/SNDS/CAR/car1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Expect sendData() to have been called with a warning message.
  std::string expectedWarnMsg = "WARNING:;ID already registered for type {CAR}";
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, expectedWarnMsg);

  // The registry should *still* have only one entry.
  ASSERT_EQ(myTypesRegistry["/CAR"].size(), 1);

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that a new ID is properly registered and ACK is sent.
TEST_F(SNDSProducerTestFixture, ProcessTypeAdvertisement_RegistersNewID) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry for "/CAR" is empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Create an Interest that advertises a new ID.
  ndn::Name interestName("/SNDS/CAR/car1");
  ndn::Interest interest(interestName);

  // Calling producer.onInterest() tests both redirection of requests
  // and the functionality of producer.isTypeAdvertisement().
  producer.onInterest(interest);

  // "car1" should have been added to the registry for the key "/CAR".
  auto& registry = producer.getMutableMyTypesRegistryMap();
  ASSERT_EQ(myTypesRegistry["/CAR"].size(), 1);
  ASSERT_EQ(myTypesRegistry["/CAR"][0], "car1");

  // Expect sendData() to have been called with the ACK message.
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, "THIS_IS_AN_SNDS_ACK");

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that if the type is not found in the registry, the advertisement is ignored.
TEST_F(SNDSProducerTestFixture, ProcessTypeAdvertisement_IgnoresWhenTypeNotFound) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry does not have an entry for "/BUS".
  ASSERT_EQ(myTypesRegistry.count("/BUS"), 0);

  // Create an Interest with a "BUS" type.
  ndn::Name interestName("/SNDS/BUS/bus1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Since the type "/BUS" does not exist in the registry,
  // sendData() should not have been called.
  ASSERT_FALSE(producer.sendDataCalled);

  // Verify that the registry *still* does not have an entry for "/BUS".
  ASSERT_EQ(myTypesRegistry.count("/BUS"), 0);

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test the SUBSCRIBE command of processSubscription.
// If the SUBSCRIBE parameters are missing, the request is ignored.
TEST_F(SNDSProducerTestFixture, ProcessSubscription_IgnoresIncompleteSubscription) {
  TestSNDSProducer producer;

  // Create an Interest with a SUBSCRIBE command,
  // missing the node parameter.
  ndn::Name interestName("/SNDS/SUBSCRIBE/car1");
  ndn::Interest interest(interestName);

  // Also tests producer.isSubscriptionCommand() via producer.onInterest().
  producer.onInterest(interest);

  // Since there is only one parameter in the interest,
  // sendData() should not have been called.
  ASSERT_FALSE(producer.sendDataCalled);

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test the SUBSCRIBE command of processSubscription.
// If the ID parameter is not in the map already, a new vector is created.
TEST_F(SNDSProducerTestFixture, ProcessSubscription_ValidNewSubscription) {
  TestSNDSProducer producer;
  auto& subscribedById = producer.getMutableSubscribedById();

  // Verify that the map does not have an entry for "/car1".
  ASSERT_EQ(subscribedById.count("/car1"), 0);

  // Create an Interest with a SUBSCRIBE command.
  ndn::Name interestName("/SNDS/SUBSCRIBE/car1/node1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // "/car1" should have been added to the map with a vector of one node.
  ASSERT_EQ(subscribedById["/car1"].size(), 1);
  ASSERT_EQ(subscribedById["/car1"][0], "/node1");

  // Expect sendData() to have been called with the ACK message.
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, "THIS_IS_AN_SNDS_ACK to_new_subscription");

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test the SUBSCRIBE command of processSubscription.
// If the ID parameter is already in the map, the node is added to the vector.
TEST_F(SNDSProducerTestFixture, ProcessSubscription_AddsToValidSubscription) {
  TestSNDSProducer producer;
  auto& subscribedById = producer.getMutableSubscribedById();
  subscribedById["/car1"].push_back("node1");

  // Create an Interest with a SUBSCRIBE command.
  ndn::Name interestName("/SNDS/SUBSCRIBE/car1/node2");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // "node2" should have been added to the vector of "car1".
  ASSERT_EQ(subscribedById["/car1"].size(), 2);
  ASSERT_EQ(subscribedById["/car1"][1], "/node2");

  // Expect sendData() to have been called with the ACK message.
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, "THIS_IS_AN_SNDS_ACK to_new_subscription");

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test the NOTIFY command of processSubscription.
// If the NOTIFY parameter is missing, the request is ignored.
TEST_F(SNDSProducerTestFixture, ProcessSubscription_IgnoresIncompleteNotify) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Create an Interest with a NOTIFY command,
  // missing the ID parameter.
  ndn::Name interestName("/SNDS/NOTIFY");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Since there are no parameters in the interest,
  // sendData() should not have been called.
  ASSERT_FALSE(producer.sendDataCalled);

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test the NOTIFY command of processSubscription.
// If the ID exists in the map and it has subscribers,
// they are notified.
TEST_F(SNDSProducerTestFixture, ProcessSubscription_NotifiesSubscribers) {
  // Create a TestMessageQueue to use
  TestMessageQueue testQueue;

  TestSNDSProducer producer;
  producer.setConsumerQueue(&testQueue);
  auto& subscribedById = producer.getMutableSubscribedById();
  subscribedById["/car1"].push_back("node1");

  // Create an Interest with a NOTIFY command.
  ndn::Name interestName("/SNDS/NOTIFY/car1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // writeMessage() should have been called,
  // adding at least one message to the testQueue.
  ASSERT_FALSE(testQueue.capturedMessages.empty());

  // Examine the captured message.
  auto& msgPtr = testQueue.capturedMessages.front();
  ASSERT_NE(msgPtr, nullptr);

  ASSERT_EQ(msgPtr->getMessage(), "node1/car1");
  ASSERT_EQ(msgPtr->getMessageType(), SNDS_INTRA_MESSAGE_TYPE::GENERIC);
  ASSERT_EQ(msgPtr->getSender(), SNDS_CLIENT_TYPES::ON_INTEREST);
  ASSERT_TRUE(msgPtr->getName().empty());

  // Expect sendData() to have been called with the ACK message.
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, "THIS_IS_AN_SNDS_ACK to_notification_for_new_subscription");

  // Reset our capture.
  testQueue.clear();
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test the NOTIFY command of processSubscription.
// If the id exists in the map and it has *no* subscribers,
// an acknowledgment is still sent.
TEST_F(SNDSProducerTestFixture, ProcessSubscription_AcknowledgesNoSubscribers) {
  TestSNDSProducer producer;
  auto& subscribedById = producer.getMutableSubscribedById();

  // Verify that the map does not have an entry for "/car1".
  ASSERT_EQ(subscribedById.count("/car1"), 0);

  // Create an Interest with a NOTIFY command.
  ndn::Name interestName("/SNDS/NOTIFY/car1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Expect sendData() to have been called with the ACK message.
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, "THIS_IS_AN_SNDS_ACK to_notification_for_new_subscription");

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that the helper function adds a leading slash
// to an empty string.
TEST_F(SNDSProducerTestFixture, EnsureLeadingSlashTest_HandlesEmptyString) {
  SNDSProducer producer;

  std::string str = "";
  producer.ensureLeadingSlash(str);
  EXPECT_EQ(str, "/");
}

// Test that the helper function adds a leading slash
// to a non-empty string that doesn't already have one.
TEST_F(SNDSProducerTestFixture, EnsureLeadingSlashTest_AddsMissingSlash) {
  SNDSProducer producer;

  std::string str = "test";
  producer.ensureLeadingSlash(str);
  EXPECT_EQ(str, "/test");
}

// Test that the helper function does *not* add a leading slash
// to a non-empty string that already has one.
TEST_F(SNDSProducerTestFixture, EnsureLeadingSlashTest_HandlesExistingSlash) {
  SNDSProducer producer;

  std::string str = "/test";
  producer.ensureLeadingSlash(str);
  EXPECT_EQ(str, "/test");
}
