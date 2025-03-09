#ifndef SNDS_UTILS_H
#define SNDS_UTILS_H

#include <string>
#include <string_view>
#include <functional>  // For std::hash
#include <spdlog/spdlog.h>

/**
 * @brief Computes the responsible SNDS node ID for a given subscription name.
 *
 * This function uses std::hash to hash the subscription name and then computes the node ID
 * by taking the hash modulo the total number of SNDS servers.
 *
 * @param name The subscription name for which to determine the responsible node.
 * @param numberOfSNDSServices The total number of SNDS servers.
 * @return The ID (an unsigned integer) of the responsible SNDS node.
 */
unsigned getSNDSNodeForSubscription(const std::string& name, int numberOfSNDSServices);

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
std::string getFromGETRequest(std::string_view url, std::string_view tag);

/**
 * @brief Ensures that the given name starts with a forward slash ('/').
 *
 * If the string is empty, it is set to "/". If the string is non-empty and does not
 * begin with '/', a '/' is inserted at the beginning.
 *
 * @param nm The string that will be modified in-place.
 */
void ensureLeadingSlash(std::string &nm);

/**
 * @brief Computes and returns the scope name for the responsible SNDS node based on the given subscription identifier.
 *
 * This function computes the responsible node index by hashing the provided subscription
 * identifier using getSNDSNodeForSubscription. It then converts the node index to a string and
 * concatenates it with a provided scope prefix (e.g., "/SNDS_ID_scope_") to form the complete scope name.
 *
 * @param id The subscription identifier.
 * @param prefix The scope prefix to use (for example, "/SNDS_ID_scope_").
 * @param numberOfSNDSServices The total number of SNDS servers.
 * @return A string representing the scope name for the SNDS node.
 */
std::string getSNDSNodeScopeNameForSubscription(const std::string& id, const std::string& prefix, int numberOfSNDSServices);

/**
 * @brief Determines if the GET request URL indicates a subscription request.
 *
 * This inline function extracts the value associated with the "isSub" parameter from the URL.
 * If the extracted value is "true", it returns true; otherwise, it returns false.
 *
 * @param url The GET request URL as a std::string.
 * @return true if the "isSub" parameter equals "true", false otherwise.
 */
inline bool isSubscriptionRequest(const std::string &url) {
    constexpr std::string_view SUBtag = "isSub";
    return getFromGETRequest(url, SUBtag) == "true";
}

/**
 * @brief Extracts the subscription URL from a GET request.
 *
 * This inline function extracts and returns the value associated with the "suburl" parameter from the URL.
 * If the parameter is not present, an empty string is returned.
 *
 * @param url The GET request URL as a std::string.
 * @return A std::string containing the subscription URL.
 */
inline std::string getURLfromSubscription(const std::string &url) {
    constexpr std::string_view SUBURLtag = "suburl";
    return getFromGETRequest(url, SUBURLtag);
}

#endif // SNDS_UTILS_H
