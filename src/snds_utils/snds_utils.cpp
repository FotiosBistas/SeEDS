#include "snds_utils.h"
#include <iostream>

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

std::vector<std::string> parseIDsFromRegistryFilePayload(std::span<const uint8_t> payload) {

    //Points to the place where the header ends
    constexpr size_t header_size = 2; 
    if (payload.size() <= header_size) {
        throw std::invalid_argument("Payload too short to contain header.");
    }

    size_t offset = header_size;
    // TODO 2^8 maximum IDS good idea?
    // Use uint8_t here to ensure you can read only 1 byte
    const uint8_t number_of_ids = payload[offset++];
    std::vector<std::string> ids;
    // Preallocate space in the vector
    ids.reserve(number_of_ids);

    for(uint8_t i = 0; i < number_of_ids; i++){
        if (offset >= payload.size()) {
            throw std::out_of_range("Payload truncated before reading ID size.");
        }
        // Request should contain the size of the ID after
        // For example if the size is 3 we should read 3 bytes
        const uint8_t size_of_id = payload[offset++];

        if(offset + size_of_id > payload.size()){
            throw std::out_of_range("Payload truncated before reading entire ID.");
        }
        // Parse the payload from the index you are currently at 
        // Until the size of the id
        std::string_view id(reinterpret_cast<const char*>(payload.data()+offset), size_of_id);
        // Advance the index
        offset = offset + size_of_id;
        // Push back
        ids.emplace_back(id);
    }

    return ids;
}

std::vector<std::byte> serializeRegistryFilePayload(const std::unordered_set<std::string>& ids){

    //TODO huge limitation on how many IDs you can have @yannisT
    if(ids.size() > 255){
        throw std::length_error("Limited description size for the ID set provided.");
    }

    size_t offset = 0;
    // We are about to create the payload
    // so we need to know the size of the byte buffer
    size_t payload_length = 1;
    for(const auto& id: ids){
        if (id.size() > 255){
            throw std::length_error("ID exceeds the size that can be described by 1 byte (2^8).");
        }
        // Allocate size for describing the length of the ID and the ID itself
        payload_length += 1 + id.size();
    }

    // Allocate needed memory for our payload
    std::vector<std::byte> payload(payload_length);
    // First byte should be allocated to hold the number_of_ids
    payload[offset++] = static_cast<std::byte>(ids.size());

    for(const auto& id: ids){
        payload[offset++] = static_cast<std::byte>(id.size());
        std::memcpy(payload.data() + offset, id.data(), id.size());
        offset += id.size();
    }

    return payload;
}