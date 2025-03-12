#include "snds_utils.h"

unsigned getSNDSNodeForSubscription(const std::string& name, int numberOfSNDSServices) {
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
