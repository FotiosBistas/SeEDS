#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_set>
#include <cctype>
#include <cstddef>      // for std::byte
#include <cstdint>      // for uint8_t
#include <cstring>      // for std::memcpy
#include <span>         // for std::span (C++20)
#include <stdexcept>    // for std::length_error, std::invalid_argument, std::out_of_range
#include <unordered_map>
#include <iomanip>
#include <sstream>
#include <memory>
#include <span>
#include <cstdint>  
#include <openssl/evp.h>
#include <ctime>

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

void removeIdPrefix(std::string& input);
void removeTypePrefix(std::string& input);

bool isMetaFileTrue(const std::string& command);

std::string format_timestamp_ms(long long ms_since_epoch);

std::unordered_map<std::string,std::string> parse_query_params(std::string_view qs);

std::string build_query_uri(std::string_view base_path, const std::vector<std::string>& kv_tokens, bool encode_values = false);
std::string trim_copy(std::string_view s);
std::string json_to_string(const nlohmann::json& j);
std::string sanitize_utf8(const std::string& s);
std::string fix_mixed_utf8_cp1252(const std::string& s);
std::string extract_json_slice(const std::string& s);
std::vector<std::string> extract_all_json_slices(const std::string& s);
std::string hexdump(const std::string& s, size_t offset = 0, size_t len = 64);
std::string hexdump_prefix(const std::string& s, size_t n = 32);
bool is_all_ascii(const std::string& s);
void log_non_ascii(const std::string& s, std::shared_ptr<spdlog::logger> lg);

long long json_to_epoch_ms(const nlohmann::json& j);

/**
 * @brief Decodes a URL-encoded string.
 *
 * This function takes a URL-encoded input string and decodes it, replacing
 * percent-encoded sequences (e.g., "%20") with their corresponding characters.
 *
 * @param in The URL-encoded input string.
 * @return A decoded string with special characters restored.
 */
std::string url_decode(std::string_view in);

/**
 * @brief Encodes a string for safe inclusion in a URL.
 *
 * This function converts special characters in the input string into
 * percent-encoded format so that the resulting string can be safely used
 * within a URL.
 *
 * @param value The input string to be URL-encoded.
 * @return A URL-encoded version of the input string.
 */
std::string url_encode(const std::string& value);

/**
 * @brief Checks whether the command contains a 'date=' version tag (URL-decoded).
 *
 * @return true if the command contains 'date='; false otherwise.
 */
bool hasDateTag(const std::string& command);

/**
 * @brief Computes the responsible SNDS node ID for a given subscription name.
 *
 * This function uses std::hash to hash the subscription name and then computes the node ID
 * by taking the hash modulo the total number of SNDS servers.
 *
 * @param name The subscription name for which to determine the responsible node.
 * @param number_of_snds_services The total number of SNDS servers.
 * @return The ID of the responsible SNDS node.
 */
size_t getSNDSNodeForSubscription
(
    const std::string& name, 
    const size_t number_of_snds_services
);

/**
 * @brief Extracts the value associated with a specific tag from a GET request URL.
 *
 * The function expects the URL to have the following format:
 *   "/tag=value&tag=value&tag=value..."
 *
 * It searches for "tag=" within the URL, and if found, extracts the substring
 * up to the next '&' (or the end of the URL).
 *
 * @param url The GET request string as a std::string_view.
 * @param tag The tag whose associated value should be extracted, as a std::string_view.
 * @return The value corresponding to the tag. Returns an empty string if the tag is not found.
 */
std::string_view get_from_get_request(std::string_view url, std::string_view tag);

/**
 * @brief Ensures that the given name starts with a forward slash ('/').
 *
 * If the string is empty, it is set to "/". If the string is non-empty and does not
 * begin with '/', a '/' is inserted at the beginning.
 *
 * @param nm The string that will be modified in-place.
 */
void ensureLeadingSlash(std::string &nm);

std::string removeLeadingSlash(const std::string &nm);

/**
 * @brief Computes and returns the scope name for the responsible SNDS node based on the given subscription identifier.
 *
 * This function computes the responsible node index by hashing the provided subscription
 * identifier using getSNDSNodeForSubscription. It then converts the node index to a string and
 * concatenates it with a provided scope prefix (e.g., "/SNDS_ID_scope_") to form the complete scope name.
 *
 * @param id The subscription identifier.
 * @param prefix The scope prefix to use (for example, "/SNDS_ID_scope_").
 * @param number_of_snds_services The total number of SNDS servers.
 * @return A string representing the scope name for the SNDS node.
 */
std::string getSNDSNodeScopeNameForSubscription(const std::string& id, const std::string& prefix, const size_t number_of_snds_services);

/**
 * @brief Determines if the GET request URL indicates a subscription request.
 *
 * This inline function extracts the value associated with the "isSub" parameter from the URL.
 * If the extracted value is "true", it returns true; otherwise, it returns false.
 *
 * @param url The GET request URL as a std::string_view.
 * @return true if the "isSub" parameter equals "true", false otherwise.
 */
inline bool is_subscription_request(std::string_view url, std::string_view is_sub_tag) {
    return get_from_get_request(url, is_sub_tag) == "true";
}

/**
 * @brief Extracts the subscription URL from a GET request.
 *
 * This inline function extracts and returns the value associated with the "suburl" parameter from the URL.
 * If the parameter is not present, an empty string is returned.
 *
 * @param url The GET request URL as a std::string_view.
 * @param subscription_url_tag The GET request tag for the URL as a std::string_view.
 * @return A std::string containing the subscription URL.
 */
inline std::string_view get_url_of_subscription(std::string_view url, std::string_view subscription_url_tag) {
    return get_from_get_request(url, subscription_url_tag);
}

//TODO these functions below require their own data type
/**
 @brief TODO I have no clue how is this used originally I assume its something
like (@yannisT): 

- Receive a sample payload:

- Header: "##" (2 bytes, ignored)

- Next byte: number of IDs (2)

- Then, for each ID: one byte for the length and then the ID string.

- First ID: length 3, content "car"

- Second ID: length 3, content "bus"
*/ 
std::vector<std::string> parse_ids_from_registry_file_payload(std::span<const uint8_t> payload, std::shared_ptr<spdlog::logger> logger = nullptr);

/**
 * @brief Serializes a set of IDs into a binary payload.
 *
 * The payload is structured as follows:
 *   [number_of_ids (2 bytes, big‐endian)]
 *   [id1_length (2 bytes, big‐endian)] [id1 bytes...]
 *   [id2_length (2 bytes, big‐endian)] [id2 bytes...]
 *   ...
 *
 * - The first 2 bytes hold the number of IDs (max 65 535).
 * - Each ID is preceded by a 2-byte length (max length 65 535).
 *
 * @param ids An unordered_set of strings representing the IDs.
 * @return A vector of std::byte containing the binary payload.
 *
 * @throws std::length_error if the number of IDs exceeds 65 535 or any ID's length exceeds 65 535.
 */
std::vector<std::byte> serialize_registry_file_payload(const std::unordered_set<std::string>& ids);

/**
 * @brief Checks whether a binary payload is marked as a registry response.
 *
 * This function looks for a leading byte equal to 'R' (0x52) as a magic identifier.
 * You can extend it later to support more types with additional marker bytes.
 *
 * @param payload A span of bytes containing the full payload.
 * @return true if it's a registry response, false otherwise.
 */
inline bool isRegistryPayload(std::span<const std::byte> payload) {
    return payload.size() >= 1 && payload[0] == std::byte{'R'};
}

/**
 * @brief Computes the MD5 hash of a data buffer and returns it as a hexadecimal string.
 *
 * This function uses OpenSSL's EVP interface to compute the MD5 digest of the provided
 * data buffer. The result is returned as a lowercase hexadecimal string.
 *
 * @param data Pointer to the input data buffer.
 * @param len The length (in bytes) of the input data buffer.
 * @return A hexadecimal string representing the MD5 hash of the input data.
 *
 * @throws std::runtime_error If any OpenSSL hashing step fails.
 */
std::string md5_hex(const void* data, std::size_t len);
