#pragma once

#include <ndn-cxx/name.hpp>
#include <string>
#include <vector>

//TODO place it inside the interests
namespace interest_message_type{

    enum class InterestMessageType 
    {
        EMPTY,
        TEMPORAL_QUERY, 
        VERSION,
    };


    inline std::string toString(const InterestMessageType type)
    {
        switch (type) {
            case InterestMessageType::TEMPORAL_QUERY:
                return "temporalQuery:";
            case InterestMessageType::VERSION:
                return "version:";
            default:
                return "";
        }
    }

    InterestMessageType parseInterestMessageType(const std::string& message_component);
};

namespace parsed_interest_name{
    /**
     * @brief Represents a parsed NDN Interest name with named fields.
     */
    struct ParsedInterestName {

        // SNDS/<name>/<params>
        // SNDS/<name>/temporalQuery:date=<value>&timeframe=<value>&count=<value>&?ndn_params
        // SNDS/<name>/version:date=<value>?our_params?ndn_params
        // TODO below 
        // SNDS/<name>_registry/
        // transfer to this SNDS/<name>/registry
        std::string                                prefix;               ///< Typically the first component (e.g., "/SNDS")
        std::string                                name;                 ///< The name (e.g., "CAR" or "SNDS_ID_scope_X" or "car2")
        interest_message_type::InterestMessageType command;              /// This can be version:, temporalQuery: etc
        std::vector<std::string> params;                                 ///< Any additional parameters (e.g., IDs, node identifiers)



        /**
         * @brief Returns a string representation of the parsed interest.
        */
        std::string toString() const {
            std::ostringstream oss;
            oss << "Prefix: " << prefix << ", Name: " << name << ", Command: " << interest_message_type::toString(command) << ", Params: [";
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



    static ParsedInterestName makeInterestNameFromMetafile
    (
        const std::string& prefix,
        const std::string& id,
        const std::string& command,
        const std::string& timestamp
    )
    {
        ParsedInterestName pi;
        pi.prefix  = prefix;
        pi.name    = id;
        pi.command = interest_message_type::parseInterestMessageType(command);
        pi.params  = {};
        pi.params.push_back(timestamp);
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
     *     name    = "CAR"
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
     *     name    = "CAR"
     *     params  = ["car1", "123"]
     *
     * Produces:
     *     /SNDS/CAR/car1/123
     *
     * @return ndn::Name The reconstructed NDN name.
     */
    std::string toCustomNDNName(const ParsedInterestName& pi);

    /// @brief  This assumes that the parameters already have their tag appended to them.
    /// For example id=car1.
    /// @param pi 
    /// @return 
    std::string toQueryString(const ParsedInterestName& pi);

}