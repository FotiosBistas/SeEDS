#pragma once

#include <ndn-cxx/name.hpp>
#include <string>
#include <vector>

//TODO rename to ParsedInterestName

/**
 * @brief Represents a parsed NDN Interest name with named fields.
 */
struct ParsedInterestName {

    std::string prefix;              ///< Typically the first component (e.g., "/SNDS")
    std::string command;             ///< The command or type (e.g., "CAR" or "SNDS_ID_scope_X")
    std::vector<std::string> params; ///< Any additional parameters (e.g., IDs, node identifiers)

    /**
     * @brief Returns a string representation of the parsed interest.
    */
    std::string toString() const {
        std::ostringstream oss;
        oss << "Prefix: " << prefix << ", Command: " << command << ", Params: [";
        for (size_t i = 0; i < params.size(); ++i) {
        oss << params[i];
        if (i + 1 < params.size())
            oss << ", ";
        }
        oss << "]";
        return oss.str();
    }

    /**
     * @brief Safely retrieves the parameter at the given index.
     *
     * @param index The parameter index (0-based).
     * @return std::optional<std::string> containing the parameter if it exists; std::nullopt otherwise.
    */
    std::optional<std::string> getParam(size_t index) const {
        if (index < params.size())
            return params[index];
        return std::nullopt;
    }
};


static ParsedInterestName makeMetafileInterest(const std::string& id,
                                           const std::string& timestamp)
{
    ParsedInterestName pi;
    pi.prefix  = id;
    pi.command = timestamp;
    pi.params  = {};
    return pi;
}

/**
 * 
 * @brief Parses an ndn::Name into a ParsedInterest structure.
 *
 * For example, an Interest name such as:
 *     /SNDS/CAR/car1
 * would be parsed as:
 *     prefix  = "/SNDS"
 *     command = "CAR"
 *     params  = ["car1"]
 *
 * @param name The NDN name to parse.
 * @return ParsedInterestName The structure containing the parsed fields.
 * @warning This is not robust and might fail
 */
ParsedInterestName parseInterestName(const ndn::Name& name);

/**
 * @brief Reconstructs the full NDN name from a ParsedInterestName object.
 *
 * For example:
 *   ParsedInterestName with:
 *     prefix  = "/SNDS"
 *     command = "CAR"
 *     params  = ["car1", "123"]
 *
 * Produces:
 *     /SNDS/CAR/car1/123
 *
 * @return ndn::Name The reconstructed NDN name.
 */
ndn::Name toName(const ParsedInterestName& pi);

/// @brief  This assumes that the parameters already have their tag appended to them.
/// For example id=car1.
/// @param pi 
/// @return 
std::string toQueryString(const ParsedInterestName& pi);