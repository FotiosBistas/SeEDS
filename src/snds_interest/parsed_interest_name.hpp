#pragma once

#include <string>
#include <sstream>
#include <type_traits>
#include <cstddef>
#include <cassert>
#include <vector>

#include <ndn-cxx/name.hpp>

#define INTEREST_MESSAGE_TYPE_LIST \
    X(EMPTY,                          "") \
    X(TEMPORAL_QUERY,                 "temporalQuery") \
    X(VERSION,                        "version") \
    X(METAFILE,                       "metafile") \
    X(POST_ID,                        "postID") \
    X(BY_TYPE_ADVERTISEMENT,          "byTypeAdvertisement") \
    X(BY_ID_AND_TYPE_ADVERTISEMENT,   "byIDAndTypeAdvertisement") \
    X(SCOPE,                          "scope") \
    X(SCOPE_NOTIFICATION,             "scopeNotification") \
    X(NEW_ID_SUBSCRIPTION,            "newIdSubscription") \
    X(NEW_TYPE_SUBSCRIPTION,          "newTypeSubscription") \
    X(SUBSCRIPTION_NOTIFICATION,      "subscriptionNotification") \
    X(GET_BY_TYPE,                    "getByType") \
    X(GET_BY_ID,                      "getByID") \
    X(GET_BY_ID_FOR_TYPE,             "getByIDForType") \
    X(GET_BY_ID_FROM_NODE,            "getByIDFromNode")

namespace parsed_interest_name {

    namespace interest_message_type {

        /// @brief Defines the type of intra-service messages.
        enum class INTEREST_MESSAGE_TYPE
        {
            #define X(name, str) name,
                INTEREST_MESSAGE_TYPE_LIST
            #undef X
        };

        static constexpr const char* INTEREST_MESSAGE_TYPE_STRINGS[] = 
        {
            #define X(name, str) str,
                INTEREST_MESSAGE_TYPE_LIST
            #undef X
        };


        constexpr const char* to_string(const INTEREST_MESSAGE_TYPE type) noexcept
        {
            using U = std::underlying_type_t<INTEREST_MESSAGE_TYPE>;
            constexpr std::size_t N = sizeof(INTEREST_MESSAGE_TYPE_STRINGS) / sizeof(*INTEREST_MESSAGE_TYPE_STRINGS);

            U idx = static_cast<U>(type);

            return idx < N ? INTEREST_MESSAGE_TYPE_STRINGS[idx]: "";
        }

        constexpr INTEREST_MESSAGE_TYPE from_string(std::string_view message_component) noexcept
        {
            constexpr std::string_view delimiter = ":";
            auto pos = message_component.find(delimiter);
            if (pos == std::string::npos) 
            {
                return INTEREST_MESSAGE_TYPE::EMPTY;
            }

            std::string_view message_string_type = message_component.substr(0, pos);

            constexpr std::size_t N = sizeof(INTEREST_MESSAGE_TYPE_STRINGS) / sizeof(*INTEREST_MESSAGE_TYPE_STRINGS);

            for (std::size_t i = 0; i < N; ++i)
            {
                if (message_string_type == INTEREST_MESSAGE_TYPE_STRINGS[i])
                {
                    return static_cast<INTEREST_MESSAGE_TYPE>(i);
                }
            }

            return INTEREST_MESSAGE_TYPE::EMPTY;
        }
    };


    //TODO I can't be bothered to include the snds utils lib
    static std::string url_encode(const std::string& value) 
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) 
        {
            std::string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') 
            {
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

    //TODO I can't be bothered to include the snds utils lib
    static int hex_val(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        throw std::invalid_argument(std::string("Invalid hex digit: ") + c);
    }

    // Decode percent‐escapes in a URL‐query fragment.
    // Does NOT treat '+' as space; '+' remains '+'. 
    //TODO I can't be bothered to include the snds utils lib
    static std::string url_decode(std::string_view in) 
    {
        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) 
        {
            char c = in[i];
            if (c == '%' && i + 2 < in.size()) 
            {
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
            else 
            {
                out += c;
            }
        }
        return out;
    }
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
        std::string                                 prefix;   ///< Typically the first component (e.g., "SNDS")
        //TODO should probably store as a vector of scopes
        std::string                                  name;     ///< The name (e.g., "CAR" or "SNDS_ID_scope_X" or "car2")
        interest_message_type::INTEREST_MESSAGE_TYPE command;  /// This can be version:, temporalQuery: etc
        std::vector<std::string>                     params;   ///< Any additional parameters (e.g., IDs, node identifiers)

        /**
         * @brief Returns a string representation of the parsed interest.
        */
        std::string to_string() const {
            std::ostringstream oss;
            oss << "Prefix: " << prefix << ", Name: " << name << ", Command: " << interest_message_type::to_string(command) << ", Params: [";
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
        std::optional<std::string> get_interest_parameter(size_t index) const {
            if (index < params.size())
                return params[index];
            return std::nullopt;
        }
    };



    static ParsedInterestName make_interest_name_from_metafile
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
        pi.command = interest_message_type::from_string(command);
        pi.params  = {};
        pi.params.push_back(timestamp);

        return pi;
    }

    static ParsedInterestName make_interest_name_from_metafile
    (
        const std::string& prefix,
        const std::string& id,
        const interest_message_type::INTEREST_MESSAGE_TYPE command,
        const std::string& timestamp
    )
    {
        ParsedInterestName pi;
        pi.prefix  = prefix;
        pi.name    = id;
        pi.command = command;
        pi.params  = {};
        pi.params.push_back(timestamp);

        return pi;
    }

    static ParsedInterestName make_interest_name_from_metafile
    (
        const std::string& prefix,
        const std::string& id,
        const interest_message_type::INTEREST_MESSAGE_TYPE command,
        const std::vector<std::string> params
    )
    {
        ParsedInterestName pi;
        pi.prefix  = prefix;
        pi.name    = id;
        pi.command = command;
        pi.params  = {};
        for(auto param: params)
        {
            pi.params.push_back(param);
        }

        return pi;
    }

    /**
     * @brief Splits a "key=value" parameter string into key and value.
     *
     * If the '=' is missing, the whole string is returned as the key and
     * the value will be empty.
     *
     * @param param A string like "date=2025-06-29T08:15:22.123+00:00"
     * @return std::pair<key, value>
    */
    std::pair<std::string, std::string> parse_param(const std::string& param);

    /**
     * @brief Convenience method to parse all params into a map.
     *
     * @param pi ParsedInterestName with .params already filled
     * @return std::map<key, value>
    */
    std::map<std::string, std::string> parse_params_map(const ParsedInterestName& pi);

    void add_param(ParsedInterestName& pi, std::string key, std::string value);
    std::string build_query_uri(std::string_view base_path, const std::vector<std::string>& kv_tokens, bool encode_values = false);

    static inline std::pair<std::string, std::string>
    parse_param_kv(std::string_view p)
    {
        const auto pos = p.find('=');
        if (pos == std::string_view::npos) return { std::string(p), std::string() };     // no '=' → value empty

        return 
        {
            std::string(p.substr(0, pos)),                 // key
            std::string(p.substr(pos + 1))                 // value (may contain '=')
        };
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
    ParsedInterestName parse_interest_name(const ndn::Name& name);

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
    std::string to_custom_ndn_name(const ParsedInterestName& pi, const bool url_encode_flag = false);

    /// @brief  This assumes that the parameters already have their tag appended to them.
    /// For example id=car1.
    /// @param pi 
    /// @return 
    std::string to_query_string(const ParsedInterestName& pi);

    /**
    * Build a ParsedInterestName from its individual parts.
    *
    * @param prefix        first component (e.g. "SNDS" or "SNDS/service")
    * @param name          second component (e.g. "car2")
    * @param command       ENUM identifying temporalQuery vs. version
    * @param params        any extra parameters (already URI-encoded, without leading '/')
    * @returns             a filled-in ParsedInterestName
    */
    inline ParsedInterestName
    make_parsed_interest_name
    (
        const std::string& prefix,
        const std::string& name,
        interest_message_type::INTEREST_MESSAGE_TYPE command,
        const std::vector<std::string>& params
    )
    {
        return ParsedInterestName{prefix, name, command, params};
    }

    std::vector<std::string> split(std::string_view view, const char delimeter);
    ParsedInterestName parse_from_string(const std::string& name);

}