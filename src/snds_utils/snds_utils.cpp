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

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    throw std::invalid_argument(std::string("Invalid hex digit: ") + c);
}

std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

// Decode percent‐escapes in a URL‐query fragment.
// Does NOT treat '+' as space; '+' remains '+'. 
std::string url_decode(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '%' && i + 2 < in.size()) {
            char hi = in[i+1], lo = in[i+2];
            //Example: 
            //%3A
            //00111010
            //58 -> :
            auto val = (hex_val(hi) << 4) | hex_val(lo);
            char decoded = static_cast<unsigned char>(val);
            out += decoded;
            i += 2;
        }
        else {
            out += c;
        }
    }
    return out;
}

std::string_view get_from_get_request(std::string_view url, std::string_view tag) {
    constexpr std::string_view delimiter = "&";
    constexpr std::string_view assign_symbol = "=";

    std::string pattern = std::string(tag) + std::string(assign_symbol);
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

    std::string_view value_view = url.substr(start, end - start);
    return value_view;
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
    //Points to the place where the header ends
    constexpr size_t header_size = 2; 
    if (payload.size() <= header_size) {
        if (logger) {
            logger->error("Payload too short to contain header. Size: {}", payload.size());
        }         
        throw std::invalid_argument("Payload too short to contain header.");
    }

    size_t offset = header_size;

    // Read a 2‐byte big‐endian count of how many IDs follow
    if (offset + 1 >= payload.size()) {
        if (logger) {
            logger->error("Payload truncated before reading number_of_ids. Offset: {}, Size: {}", offset, payload.size());
        }
        throw std::out_of_range("Payload truncated before reading number_of_ids.");
    }
    uint16_t number_of_ids = (static_cast<uint16_t>(payload[offset]) << 8)
                           | (static_cast<uint16_t>(payload[offset + 1]));
    offset += 2;

    if (logger) {
        logger->debug("Number of IDs in payload: {}", number_of_ids);
    } 

    std::vector<std::string> ids;
    // Preallocate space in the vector
    ids.reserve(number_of_ids);

    for (uint16_t i = 0; i < number_of_ids; ++i) {
        // Ensure we have at least 2 bytes left for the next length field
        if (offset + 1 >= payload.size()) {
            if (logger) {
                logger->error("Payload truncated before reading ID size. Offset: {}, Size: {}", offset, payload.size());
            }
            throw std::out_of_range("Payload truncated before reading ID size.");
        }

        // Read a 2‐byte big‐endian length of the next ID
        uint16_t size_of_id = (static_cast<uint16_t>(payload[offset]) << 8)
                            | (static_cast<uint16_t>(payload[offset + 1]));
        offset += 2;
        if (logger) {
            logger->debug("Reading ID #{} with size {}", i + 1, size_of_id);
        }

        if (offset + size_of_id > payload.size()) {
            if (logger) {
                logger->error("Payload truncated before reading entire ID. Offset: {}, Needed: {}, Size: {}", offset, size_of_id, payload.size());
            }
            throw std::out_of_range("Payload truncated before reading entire ID.");
        }

        // Extract the ID string
        std::string_view id(reinterpret_cast<const char*>(payload.data() + offset), size_of_id);
        offset += size_of_id;
        ids.emplace_back(id);

        if (logger) {
            logger->info("Parsed ID #{}: {}", i + 1, id);
        }
    }

    return ids;
}


std::vector<std::byte> serializeRegistryFilePayload(const std::unordered_set<std::string>& ids){

    // now we use a 2‐byte count, so up to 65 535 IDs:
    if (ids.size() > 65535) {
        throw std::length_error("Limited description size: cannot describe more than 65535 IDs.");
    }

    size_t offset = 0;
    // We are about to create the payload
    // so we need to know the size of the byte buffer
    size_t payload_length = 2;
    for(const auto& id: ids){
        if (id.size() > 65535){
            throw std::length_error("ID exceeds the size that can be described by 2 bytes (2^16).");
        }
        // Allocate size for describing the length of the ID and the ID itself
        payload_length += 2 + id.size();
    }

    // Allocate needed memory for our payload
    std::vector<std::byte> payload(payload_length);

    // First 2 bytes: high‐order then low‐order of ids.size()
    uint16_t count = static_cast<uint16_t>(ids.size());
    payload[offset++] = static_cast<std::byte>((count >> 8) & 0xFF);
    payload[offset++] = static_cast<std::byte>( count       & 0xFF);

    for (const auto& id : ids) {
        uint16_t len = static_cast<uint16_t>(id.size());
        payload[offset++] = static_cast<std::byte>((len >> 8) & 0xFF);
        payload[offset++] = static_cast<std::byte>( len       & 0xFF);
        std::memcpy(payload.data() + offset, id.data(), len);
        offset += len;
    }

    return payload;
}
