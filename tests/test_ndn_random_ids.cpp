#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/time.hpp>
#include "snds_utils.h"
#include <random>
#include <string>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <span>

int
main()
{
  ndn::Face face;
  const std::string prefix = "/SNDS";
  const std::string typeName = "CAR";
  const std::string registryName = typeName + "_registry";

  // 1) Generate 256 unique random IDs of length 8
  std::unordered_set<std::string> idSet;
  std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<char> dist('a', 'z');
  while (idSet.size() < 256) {
    std::string s;
    s.reserve(8);
    for (int i = 0; i < 8; ++i) s += dist(rng);
    idSet.insert(std::move(s));
  }
  std::cout << "→ Generated 256 IDs\n";

  // Helper: express an Interest and block until we get Data / Nack / timeout
  auto expressAndWait = [&](const ndn::Name& name, std::vector<uint8_t>* outContent) {

    ndn::Interest interest(name);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::milliseconds{6000});

    face.expressInterest(
      interest,
      // onData
      [&](auto&&, const ndn::Data& data) {
        auto contentBlock = data.getContent();
        const auto* ptr   = contentBlock.value();
        auto        len   = contentBlock.value_size();
        if (outContent) {
          outContent->assign(ptr, ptr + len);
        }
        face.shutdown();
      },
      // onNack
      [&](auto&&, const ndn::lp::Nack& nack) {
        std::cerr << "Nack for " << name.toUri()
                  << ": " << nack.getReason() << "\n";
        face.shutdown();
      },
      // onTimeout
      [&](auto&&) {
        std::cerr << "Timeout for " << name.toUri() << "\n";
        face.shutdown();
      });

    face.processEvents();
    // give NFD a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  };

  // 2) Post each ID via a CAR advertisement
  size_t count = 0;
  for (auto& id : idSet) {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
    ndn::Name name(prefix);
    name.append(typeName).append(id).append("v=" + std::to_string(ts));

    expressAndWait(name, nullptr);
    if (++count % 50 == 0) {
      std::cout << "  → posted " << count << "…\n";
    }
  }
  std::cout << "→ All 256 posted\n";

  // 3) Fetch the registry blob
  std::vector<uint8_t> wire;
  {
    ndn::Name name(prefix);
    name.append(registryName);
    expressAndWait(name, &wire);
  }
  if (wire.empty()) {
    std::cerr << "FAILED to fetch registry data\n";
    return 1;
  }

  // 4) Prepend dummy 2-byte header
  std::vector<uint8_t> payload;
  payload.reserve(wire.size() + 2);
  payload.push_back(0);
  payload.push_back(0);
  payload.insert(payload.end(), wire.begin(), wire.end());

  // 5) Parse back into IDs
  std::vector<std::string> parsed;
  try {
    parsed = parseIDsFromRegistryFilePayload(
      std::span<const uint8_t>(payload.data(), payload.size()),
      /* logger */ nullptr);
  }
  catch (const std::exception& e) {
    std::cerr << "parse error: " << e.what() << "\n";
    return 2;
  }

  // 6) Compare sets
  std::unordered_set<std::string> got(parsed.begin(), parsed.end());
  if (got == idSet) {
    std::cout << "✅ All 256 IDs round-tripped successfully!\n";
    return 0;
  }
  std::cout << "❌ Mismatch!\n";
  for (auto& id : idSet)
    if (!got.count(id)) std::cout << " Missing: " << id << "\n";
  for (auto& id : got)
    if (!idSet.count(id)) std::cout << " Unexpected: " << id << "\n";
  return 3;
}
