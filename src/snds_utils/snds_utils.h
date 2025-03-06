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
