#include "snds_producer.h"

#include <gtest/gtest.h>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/name.hpp>
#include <spdlog/spdlog.h>

// New producer class to override sendData(),
// in order to capture the payload.
class TestSNDSProducer : public SNDSProducer {
  public:
    bool sendDataCalled = false;
    std::string capturedPayload;

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

// Test that a new ID is properly registered and ACK is sent.
TEST_F(SNDSProducerTestFixture, RegistersNewID) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry for "/CAR" is empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Create an Interest that advertises a new ID.
  ndn::Name interestName("/SNDS/CAR/car1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // "car1" should be added to the registry for the key "/CAR".
  auto& registry = producer.getMutableMyTypesRegistryMap();
  ASSERT_EQ(myTypesRegistry["/CAR"].size(), 1);
  ASSERT_EQ(myTypesRegistry["/CAR"][0], "car1");

  // Expect sendData to have been called with the ACK message.
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, "THIS_IS_AN_SNDS_ACK");


  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that processing an advertisement with a duplicate ID sends a warning.
TEST_F(SNDSProducerTestFixture, WarnsOnDuplicateID) {
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

  // Expect sendData to have been called with a warning message.
  std::string expectedWarnMsg = "WARNING:;ID already registered for type {CAR}";
  ASSERT_TRUE(producer.sendDataCalled);
  ASSERT_EQ(producer.capturedPayload, expectedWarnMsg);

  // The registry should still only have one entry.
  ASSERT_EQ(myTypesRegistry["/CAR"].size(), 1);

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that if no ID is provided the advertisement is ignored.
TEST_F(SNDSProducerTestFixture, IgnoresWhenIDMissing) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry for "/CAR" is empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Create an Interest with only the service prefix and type (no ID).
  ndn::Name interestName("/SNDS/CAR");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Since no ID is provided, sendData should not have beeen called.
  ASSERT_FALSE(producer.sendDataCalled);

  // Verify that the registry for "/CAR" remains empty.
  ASSERT_TRUE(myTypesRegistry["/CAR"].empty());

  // Reset our capture.
  producer.sendDataCalled = false;
  producer.capturedPayload.clear();
}

// Test that if the type is not found in the registry, the advertisement is ignored.
TEST_F(SNDSProducerTestFixture, IgnoresWhenTypeNotFound) {
  TestSNDSProducer producer;
  auto& myTypesRegistry = producer.getMutableMyTypesRegistryMap();

  // Verify that the registry does not have an entry for "/BUS".
  ASSERT_EQ(myTypesRegistry.count("/BUS"), 0);

  // Create an Interest with a "BUS" type.
  ndn::Name interestName("/SNDS/BUS/bus1");
  ndn::Interest interest(interestName);

  producer.onInterest(interest);

  // Since the type "/BUS" does not exist in the registry,
  // sendData should not be called.
  ASSERT_FALSE(producer.sendDataCalled);

  // Verify that the registry *still* does not have an entry for "/BUS".
  ASSERT_EQ(myTypesRegistry.count("/BUS"), 0);
}

