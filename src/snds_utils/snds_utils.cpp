#include "snds_utils.h"
#include <iostream>

size_t getSNDSNodeForSubscription
(
    const std::string& name, 
    const size_t number_of_snds_services
) 
{
    std::hash<std::string> hash_fn;
    size_t hash_value = hash_fn(name);
    return hash_value % number_of_snds_services;
}

std::string format_timestamp_ms(long long ms_since_epoch){
    using namespace std::chrono;
    // build a time_point
    system_clock::time_point tp{milliseconds(ms_since_epoch)};
    // split off whole seconds vs leftover ms
    auto secs    = floor<seconds>(tp);
    auto ms_part = duration_cast<milliseconds>(tp - secs).count();
    // convert to time_t and break out UTC fields
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm    tm{};
    gmtime_r(&tt, &tm);  // POSIX; TODO on Windows use gmtime_s

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms_part
        << "+00:00";
    //TODO using a more generic date currently
    //oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
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

bool hasDateTag(const std::string& command){
    std::string decoded = url_decode(command);
    return decoded.find("date=") != std::string::npos;
}

bool isMetaFileTrue(const std::string& command) {
    std::string decoded = url_decode(command);

    const std::string key = "metafile=";
    auto pos = decoded.find(key);
    if (pos == std::string::npos)
        return false;

    pos += key.size();
    auto end = decoded.find('&', pos);
    std::string value = decoded.substr(pos, end == std::string::npos 
                                             ? std::string::npos 
                                             : end - pos);

    return value == "true";
}

std::string md5_hex(const void* data, std::size_t len) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    if (EVP_DigestUpdate(ctx, data, len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestUpdate failed");
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
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

    if (start == std::string_view::npos) 
    {
        return "";
    }

    start += pattern.size();

    if (start >= url.size()) 
    {
        return "";
    }

    std::size_t end = url.find(delimiter, start);
    if (end == std::string_view::npos) 
    {
        end = url.size();
    }

    return url.substr(start, end - start);
}
 
std::unordered_map<std::string,std::string> parse_query_params(std::string_view qs) {
    std::unordered_map<std::string,std::string> params;
    size_t pos = 0;
    while (pos < qs.size()) {
        // find next &
        auto amp = qs.find('&', pos);
        auto part = qs.substr(pos, amp == std::string_view::npos ? qs.size()-pos : amp-pos);

        // split on '='
        auto eq = part.find('=');
        if (eq != std::string_view::npos) {
            std::string key   = url_decode(std::string(part.substr(0, eq)));
            std::string value = url_decode(std::string(part.substr(eq+1)));
            params.emplace(std::move(key), std::move(value));
        }

        if (amp == std::string_view::npos) break;
        pos = amp + 1;
    }
    return params;
}


void ensureLeadingSlash(std::string &nm) {
    if (nm.empty()) {
        nm = "/";
    } else if (nm.front() != '/') {
        nm.insert(nm.begin(), '/');
    }
}

std::string removeLeadingSlash(const std::string &nm) {
    // make a copy
    std::string new_string = nm;

    // if it starts with '/', drop that first character
    if (!new_string.empty() && new_string.front() == '/') {
        new_string.erase(new_string.begin());
    }

    return new_string;
}

std::string getSNDSNodeScopeNameForSubscription(
    const std::string& id, 
    const std::string& prefix, 
    const size_t number_of_snds_services
) 
{
    size_t node_index = getSNDSNodeForSubscription(id, number_of_snds_services);
    return prefix + std::to_string(node_index);
}

std::vector<std::string> parseIDsFromRegistryFilePayload
(
    std::span<const uint8_t> payload,
    std::shared_ptr<spdlog::logger> logger
) 
{
    //Points to the place where the header ends
    constexpr size_t header_size = 2; 
    if (payload.size() <= header_size) 
    {
        if (logger) 
        {
            logger->error("Payload too short to contain header. Size: {}", payload.size());
        }         
        throw std::invalid_argument("Payload too short to contain header.");
    }

    size_t offset = header_size;

    // Read a 2‐byte big‐endian count of how many IDs follow
    if (offset + 1 >= payload.size()) 
    {
        if (logger) 
        {
            logger->error("Payload truncated before reading number_of_ids. Offset: {}, Size: {}", offset, payload.size());
        }
        throw std::out_of_range("Payload truncated before reading number_of_ids.");
    }
    uint16_t number_of_ids = (static_cast<uint16_t>(payload[offset]) << 8)
                           | (static_cast<uint16_t>(payload[offset + 1]));
    offset += 2;

    if (logger) 
    {
        logger->debug("Number of IDs in payload: {}", number_of_ids);
    } 

    std::vector<std::string> ids;
    // Preallocate space in the vector
    ids.reserve(number_of_ids);

    for (uint16_t i = 0; i < number_of_ids; ++i) 
    {
        // Ensure we have at least 2 bytes left for the next length field
        if (offset + 1 >= payload.size()) 
        {
            if (logger) 
            {
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

        if (offset + size_of_id > payload.size()) 
        {
            if (logger) 
            {
                logger->error("Payload truncated before reading entire ID. Offset: {}, Needed: {}, Size: {}", offset, size_of_id, payload.size());
            }
            throw std::out_of_range("Payload truncated before reading entire ID.");
        }

        // Extract the ID string
        std::string_view id(reinterpret_cast<const char*>(payload.data() + offset), size_of_id);
        offset += size_of_id;
        ids.emplace_back(id);

        if (logger) 
        {
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
