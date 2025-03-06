#include <string>
#include <string_view>

/**
 * @brief Computes the responsible SNDS node ID for a given subscription name.
 *
 * This function uses std::hash to hash the subscription name and then
 * calculates the node ID by taking the hash modulo the total number of SNDS servers.
 *
 *
 * @param name The subscription name for which to determine the responsible node.
 * @return The ID (an unsigned integer) of the responsible SNDS node.
 */
unsigned getSNDSNodeForSubscription(const std::string& name, int numberOfSNDSServices);

/**
 * @brief Extracts the value associated with a specific tag from a GET request URL.
 *
 * Assumes the URL is in the format:
 *    "/tag=value&tag=value&tag=value..."
 *
 * @param url The GET request string.
 * @param tag The tag whose value should be extracted.
 * @return The value corresponding to the tag. Returns an empty string if the tag is not found.
 */ 
std::string getFromGETRequest(std::string_view url, std::string_view tag);

/**
 * @brief Ensures that the given name starts with a forward slash ('/').
 *
 * If the string is empty, it will be set to "/" (you may change this behavior if needed).
 * If the string is non-empty and does not start with '/', a '/' is inserted at the beginning.
 *
 * @param nm The string that will be modified in-place.
 */
void ensureLeadingSlash(std::string &nm);


/**
 * @brief Computes and returns the scope name for the responsible SNDS node
 *        based on the given subscription identifier.
 *
 * This function calculates the responsible node index by hashing the provided
 * subscription identifier using getSNDSNodeForSubscription. It then converts the
 * node index to a string and concatenates it with a predefined scope prefix 
 * (ID_SCOPE_PREFIX). The resulting string is the complete scope name for that node.
 *
 * @param _id The subscription identifier.
 * @return A string representing the scope name for the SNDS node.
 */
std::string getSNDSNodeScopeNameForSubscription(const std::string& id, const std::string& prefix);


/**
 * @brief Determines if the given GET request URL indicates a subscription request.
 *
 * This function extracts the value associated with the "isSub" parameter in the URL.
 * If the extracted value equals "true", the function returns true.
 * Otherwise, it returns false.
 *
 * @param url The GET request URL as a std::string.
 * @return true if the "isSub" parameter equals "true", false otherwise.
 */
inline bool isSubscriptionRequest(const std::string &url) {
    constexpr std::string_view SUBtag = "isSub";
    return getfromGETRequest(url, SUBtag) == "true";
}

/**
 * @brief Extracts the subscription URL from a GET request.
 *
 * This function extracts and returns the value associated with the "suburl" parameter.
 * If the parameter is not present, an empty string is returned.
 *
 * @param url The GET request URL as a std::string.
 * @return A std::string containing the subscription URL.
 */
inline std::string getURLfromSubscription(const std::string &url) {
    constexpr std::string_view SUBURLtag = "suburl";
    return getfromGETRequest(url, SUBURLtag);
}