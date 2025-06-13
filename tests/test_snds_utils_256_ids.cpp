// test_snds_utils_256_ids.cpp

#include <gtest/gtest.h>
#include <random>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <span>
#include "snds_utils.h"

// We pass nullptr for the logger since we don't need logging in this test
TEST(SerializeParseRegistry, Handles256RandomUniqueIds) {
  // 1) Generate 256 unique random IDs of length 8
  std::unordered_set<std::string> idSet;
  std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<char> dist('a', 'z');
  while (idSet.size() < 256) {
    std::string s;
    s.reserve(8);
    for (int i = 0; i < 8; ++i) {
      s += dist(rng);
    }
    idSet.insert(std::move(s));
  }
  ASSERT_EQ(idSet.size(), 256u);

  // 2) Serialize the set into our byte‐stream payload
  auto payload = serializeRegistryFilePayload(idSet);
  ASSERT_FALSE(payload.empty());

  // 2b) Prepend two dummy header bytes so parseIDsFromRegistryFilePayload will skip them
  std::vector<uint8_t> withHeader;
  withHeader.reserve(payload.size() + 2);
  withHeader.push_back(0x00);
  withHeader.push_back(0x00);
  for (auto b : payload) {
    withHeader.push_back(static_cast<uint8_t>(b));
  }

  // 3) Parse back the payload into a vector of strings
  auto parsed = parseIDsFromRegistryFilePayload(
    std::span<const uint8_t>(withHeader.data(), withHeader.size()),
    /* logger */ nullptr);

  // 4) We should get exactly 256 IDs back
  ASSERT_EQ(parsed.size(), idSet.size());

  // 5) Compare contents: sort both and ensure they match exactly
  std::vector<std::string> orig{idSet.begin(), idSet.end()};
  std::vector<std::string> got = parsed;
  std::sort(orig.begin(), orig.end());
  std::sort(got.begin(), got.end());
  EXPECT_EQ(orig, got);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
