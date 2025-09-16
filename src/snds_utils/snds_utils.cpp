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

std::string trim_copy(std::string_view s) 
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return std::string{s.substr(a, b-a)};
}

std::string fix_mixed_utf8_cp1252(const std::string& s) 
{
    std::string out;
    out.reserve(s.size());
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    const unsigned char* e = p + s.size();

    auto append = [&](unsigned char c){ out.push_back(static_cast<char>(c)); };
    auto append_str = [&](const char* t){ out.append(t); };

    while (p < e) 
    {
        unsigned char c = *p;

        if (c < 0x80) { append(c); ++p; continue; }                // ASCII

        // Valid UTF-8?
        if ((c >> 5) == 0x6 && p+1 < e && (p[1] & 0xC0) == 0x80) { out.append((const char*)p, 2); p += 2; continue; }
        if ((c >> 4) == 0xE && p+2 < e && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { out.append((const char*)p, 3); p += 3; continue; }
        if ((c >> 3) == 0x1E && p+3 < e && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { out.append((const char*)p, 4); p += 4; continue; }

        // CP1252 common offenders → ASCII
        switch (c) {
            case 0x91: case 0x92: append('\''); break;      // ‘ ’
            case 0x93: case 0x94: append('\"'); break;      // “ ”
            case 0x96: append('-'); break;                  // –
            case 0x97: out += "--"; break;                  // —
            case 0x85: out += "..."; break;                 // …
            // Others in 0x80–0x9F: drop them
            default: /* drop */ break;
        }
        ++p;
    }
    return out;
}

// remove invalid UTF-8 bytes; keep ASCII and valid UTF-8 sequences
std::string sanitize_utf8(const std::string& s) 
{
    std::string out;
    out.reserve(s.size());
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    const unsigned char* e = p + s.size();

    auto append = [&](unsigned char c){ out.push_back(static_cast<char>(c)); };

    while (p < e) {
        unsigned char c = *p;
        if (c < 0x80) 
        { // ASCII
            append(c);
            ++p;
        } 
        else if ((c >> 5) == 0x6 && p+1 < e) 
        {           // 2-byte
            unsigned char c1 = *(p+1);
            if ((c1 & 0xC0) == 0x80) { out.append(reinterpret_cast<const char*>(p), 2); p += 2; }
            else { ++p; } // drop invalid lead
        } 
        else if ((c >> 4) == 0xE && p+2 < e) 
        {           // 3-byte
            unsigned char c1 = *(p+1), c2 = *(p+2);
            if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) { out.append(reinterpret_cast<const char*>(p), 3); p += 3; }
            else { ++p; }
        } 
        else if ((c >> 3) == 0x1E && p+3 < e) 
        {          // 4-byte
            unsigned char c1 = *(p+1), c2 = *(p+2), c3 = *(p+3);
            if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) { out.append(reinterpret_cast<const char*>(p), 4); p += 4; }
            else { ++p; }
        } 
        else 
        {
            ++p; // invalid lead, drop
        }
    }
    return out;
}

std::vector<std::string> extract_all_json_slices(const std::string& s) 
{
    std::vector<std::string> res;
    size_t i = 0;
    while (true) 
    {
        size_t start = s.find_first_of("{[", i);
        if (start == std::string::npos) break;
        char open = s[start], close = (open == '{') ? '}' : ']';
        int depth = 0; bool in_str = false, esc = false;

        size_t j = start;
        for (; j < s.size(); ++j) 
        {
            char ch = s[j];
            if (in_str) { if (esc) esc = false; else if (ch == '\\') esc = true; else if (ch == '"') in_str = false; continue; }
            if (ch == '"') { in_str = true; continue; }
            if (ch == open) ++depth;
            else if (ch == close) { if (--depth == 0) { res.emplace_back(s.substr(start, j - start + 1)); ++j; break; } }
        }
        i = j;
        if (j >= s.size()) break; // last slice may be unterminated; we already pushed it
    }
    return res;
}

std::string hexdump(const std::string& s, size_t offset, size_t len) 
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    size_t end = std::min(offset + len, s.size());
    for (size_t i = offset; i < end; ++i) 
    {
        if ((i - offset) % 16 == 0) oss << "\n" << std::setw(6) << i << ": ";
        oss << std::setw(2) << (static_cast<unsigned>(static_cast<unsigned char>(s[i])) & 0xFF) << ' ';
    }
    return oss.str();
}

bool is_all_ascii(const std::string& s) 
{
    for (unsigned char c : s) if (c & 0x80) return false;
    return true;
}

void log_non_ascii(const std::string& s, std::shared_ptr<spdlog::logger> lg) 
{
    for (size_t i = 0; i < s.size(); ++i) 
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x80) 
        {
            lg->error("Non-ASCII at pos {}: 0x{:02X}", i, static_cast<unsigned>(c));
            break;
        }

        //lg->debug("Normal character: {}", s[i]);
    }
}

// extract a balanced JSON object/array starting at first '{' or '['
// understands strings/escapes so it doesn't get fooled by braces inside quotes
std::string extract_json_slice(const std::string& s) 
{
    size_t start = s.find_first_of("{[");
    if (start == std::string::npos) return {};
    char open = s[start];
    char close = (open == '{') ? '}' : ']';

    size_t i = start;
    int depth = 0;
    bool in_str = false;
    bool esc = false;

    for (; i < s.size(); ++i) 
    {
        char ch = s[i];
        if (in_str) 
        {
            if (esc) { esc = false; continue; }
            if (ch == '\\') { esc = true; continue; }
            if (ch == '"') { in_str = false; }
            continue;
        }
        if (ch == '"') { in_str = true; continue; }
        if (ch == open) { ++depth; continue; }
        if (ch == close) 
        {
            --depth;
            if (depth == 0) 
            {
                return s.substr(start, i - start + 1);
            }
        }
    }
    // not balanced; return from start to end (caller will validate)
    return s.substr(start);
}

std::string hexdump_prefix(const std::string& s, size_t n) 
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < std::min(n, s.size()); ++i) 
    {
        oss << std::setw(2) << (static_cast<unsigned>(static_cast<unsigned char>(s[i])) & 0xFF) << ' ';
    }
    return oss.str();
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

std::vector<std::string> parse_ids_from_registry_file_payload
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
    uint16_t number_of_ids = (static_cast<uint16_t>(payload[offset]) << 8) | (static_cast<uint16_t>(payload[offset + 1]));
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
        if (logger) 
        {
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



std::string json_to_string(const nlohmann::json& j) {
    if (j.is_string())  return j.get<std::string>();
    if (j.is_boolean()) return j.get<bool>() ? "true" : "false";
    if (j.is_number_integer())  return std::to_string(j.get<long long>());
    if (j.is_number_unsigned()) return std::to_string(j.get<unsigned long long>());
    if (j.is_number_float()) { std::ostringstream os; os << std::setprecision(15) << j.get<double>(); return os.str(); }
    return j.dump();
}

long long json_to_epoch_ms(const nlohmann::json& j) {
    if (j.is_number_integer()) return j.get<long long>();
    if (j.is_string()) {
        std::tm tm{}; std::istringstream is(j.get<std::string>()); is >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        if (is.fail()) throw std::runtime_error("Unsupported $date string");
        time_t t = timegm(&tm); return static_cast<long long>(t) * 1000LL;
    }
    throw std::runtime_error("Unsupported $date type");
}

std::vector<std::byte> serialize_registry_file_payload(const std::unordered_set<std::string>& ids)
{

    // now we use a 2‐byte count, so up to 65 535 IDs:
    if (ids.size() > 65535) 
    {
        throw std::length_error("Limited description size: cannot describe more than 65535 IDs.");
    }

    size_t offset = 0;
    // We are about to create the payload
    // so we need to know the size of the byte buffer
    size_t payload_length = 2;
    for(const auto& id: ids)
    {
        if (id.size() > 65535)
        {
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

    for (const auto& id : ids) 
    {
        uint16_t len = static_cast<uint16_t>(id.size());
        payload[offset++] = static_cast<std::byte>((len >> 8) & 0xFF);
        payload[offset++] = static_cast<std::byte>( len       & 0xFF);
        std::memcpy(payload.data() + offset, id.data(), len);
        offset += len;
    }

    return payload;
}
