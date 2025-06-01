#include "snds_utils.h"
#include <iostream>

#include <arpa/inet.h>

unsigned getSNDSNodeForSubscription(const std::string& name, int numberOfSNDSServices) {
    std::hash<std::string> hash_fn;
    size_t hash_value = hash_fn(name);
    return hash_value % numberOfSNDSServices;
}

void removeIdPrefix(std::string& input) {
    constexpr std::string_view prefix = "id=";
    if (input.compare(0, prefix.size(), prefix) == 0) {
        input.erase(0, prefix.size());
    }
}

void removeTypePrefix(std::string& input) {
    constexpr std::string_view prefix = "type=";
    if (input.compare(0, prefix.size(), prefix) == 0) {
        input.erase(0, prefix.size());
    }
}

std::string getFromGETRequest(std::string_view url, std::string_view tag) {
    constexpr std::string_view delimiter = "&";
    constexpr std::string_view assignSymbol = "=";

    std::string pattern = std::string(tag) + std::string(assignSymbol);
    std::size_t start = url.find(pattern);

    if (start == std::string_view::npos) {
        return "";
    }

    start += pattern.size();

    if (start >= url.size()) {
        return "";
    }

    std::size_t end = url.find(delimiter, start);
    if (end == std::string_view::npos) {
        end = url.size();
    }

    std::string_view valueView = url.substr(start, end - start);
    return std::string(valueView);
}


void ensureLeadingSlash(std::string &nm) {
    if (nm.empty()) {
        nm = "/";
    } else if (nm.front() != '/') {
        nm.insert(nm.begin(), '/');
    }
}

std::string getSNDSNodeScopeNameForSubscription(const std::string& id, const std::string& prefix, int numberOfSNDSServices) {
    unsigned nodeIndex = getSNDSNodeForSubscription(id, numberOfSNDSServices);
    return prefix + std::to_string(nodeIndex);
}

std::vector<std::string> parseIDsFromRegistryFilePayload(
    std::span<const uint8_t> payload,
    std::shared_ptr<spdlog::logger> logger) 
{
    // We expect at least 2 bytes for the 16-bit “number_of_ids”
    if (payload.size() < 2) {
        if (logger) logger->error("Registry payload too short to read ID count.");
        throw std::out_of_range("Registry payload too short.");
    }

    size_t offset = 0;

    // 1) Read the 2-byte count (big-endian)
    uint16_t netCount;
    std::memcpy(&netCount, payload.data() + offset, 2);
    uint16_t count = ntohs(netCount);
    offset += 2;

    if (logger) logger->debug("Number of IDs in payload: {}", count);

    std::vector<std::string> ids;
    ids.reserve(count);

    // 2) For each ID, read 2-byte length, then read that many bytes
    for (uint16_t i = 0; i < count; i++) {
        if (offset + 2 > payload.size()) {
        if (logger) logger->error("Payload truncated before reading ID length. Offset={} size={}", offset, payload.size());
        throw std::out_of_range("Payload truncated before reading ID length.");
        }
        uint16_t netLen;
        std::memcpy(&netLen, payload.data() + offset, 2);
        uint16_t len = ntohs(netLen);
        offset += 2;

        if (logger) logger->debug("Reading ID #{} with length {}", i + 1, len);

        if (offset + len > payload.size()) {
        if (logger) logger->error("Payload truncated before reading ID bytes. Offset={} needed={} size={}", offset, len, payload.size());
        throw std::out_of_range("Payload truncated before reading ID bytes.");
        }

        // Extract the ID string
        std::string id(reinterpret_cast<const char*>(payload.data() + offset), len);
        ids.emplace_back(std::move(id));
        offset += len;
    }

    return ids;
}


std::vector<std::byte> serializeRegistryFilePayload(const std::unordered_set<std::string>& ids){

    // 1) Check that count fits in a 16-bit unsigned integer
    if (ids.size() > 0xFFFF) {
        throw std::length_error("Too many IDs (must be ≤ 65 535).");
    }

    // 2) Compute total buffer size:
    //    2 bytes for “number of IDs”, plus for each ID: 2 bytes for “length” + actual bytes.
    size_t totalSize = 2;  // space for 16-bit count
    for (const auto& id : ids) {
        if (id.size() > 0xFFFF) {
            throw std::length_error("Single ID exceeds 65 535 bytes.");
        }
    totalSize += 2;          // 2 bytes for the ID length
    totalSize += id.size();  // plus the ID’s characters
    }

    std::vector<std::byte> buffer(totalSize);
    size_t pos = 0;

    // 3) Write “number of IDs” as uint16_t in network byte order
    uint16_t count = static_cast<uint16_t>(ids.size());
    uint16_t netCount = htons(count);
    std::memcpy(buffer.data() + pos, &netCount, 2);
    pos += 2;

    // 4) For each ID, write (uint16_t length, then bytes)
    for (const auto& id : ids) {
        uint16_t len = static_cast<uint16_t>(id.size());
        uint16_t netLen = htons(len);
        std::memcpy(buffer.data() + pos, &netLen, 2);
        pos += 2;

        std::memcpy(buffer.data() + pos, id.data(), id.size());
        pos += id.size();
    }

    // At this point pos == totalSize
    return buffer;
}