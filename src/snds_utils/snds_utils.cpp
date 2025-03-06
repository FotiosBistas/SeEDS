#include "snds_utils.h"




unsigned getSNDSNodeForSubscription(const std::string& name, const int numberOfSNDSServices){
    std::hash<std::string> hash_fn;
    size_t hash_value = hash_fn(name);
    return hash_value % numberOfSNDSServices;
}

std::string getFromGETRequest(std::string_view url, std::string_view tag) {
    constexpr std::string_view delimiter = "&";
    constexpr std::string_view assignSymbol = "=";
    std::string pattern = std::string(tag) + std::string(assignSymbol);
    std::size_t start = url.find(pattern);
    if (start == std::string_view::npos) {
        // Tag not found.
        return "";
    }
    start += pattern.size();
    std::size_t end = url.find(delimiter, start);
    if (end == std::string_view::npos) {
        end = url.size();
    }
    std::string_view valueView = url.substr(start, end - start);
    return std::string(valueView);
}


void ensureLeadingSlash(std::string &nm) {
    if (nm.empty()) {
        // If the name is empty, set it to a single slash.
        nm = "/";
    } else if (nm.front() != '/') {
        // Insert a leading slash if it does not already start with one.
        nm.insert(nm.begin(), '/');
    }
}

std::string getSNDSNodeScopeNameForSubscription(const std::string& id, const std::string& prefix) {
    unsigned nodeIndex = getSNDSNodeForSubscription(id);
    return prefix + std::to_string(nodeIndex);
}