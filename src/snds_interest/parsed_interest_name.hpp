#pragma once
/**
 * @file parsed_interest_name.hpp
 * @brief Utilities to model and parse SNDS-style NDN Interest names.
 *
 * @details
 * This header provides:
 *  - A taxonomy of Interest “command” components (e.g., `temporalQuery`, `version`)
 *    via the INTEREST_MESSAGE_TYPE enum and string table.
 *  - A lightweight value type (#parsed_interest_name::ParsedInterestName) that
 *    holds a decomposed Interest name (prefix, name, command, params).
 *  - Helpers to build, parse, and serialize Interest names and query fragments,
 *    including URL encode/decode for parameters.
 *
 * Naming layout (conceptual examples):
 *  - `/SNDS/<name>/<params...>`
 *  - `/SNDS/<name>/temporalQuery:date=<val>&timeframe=<val>&count=<val>`
 *  - `/SNDS/<name>/version:date=<val>`
*/
#include <string>
#include <sstream>
#include <type_traits>
#include <cstddef>
#include <cassert>
#include <vector>

#include <ndn-cxx/name.hpp>
/**
 * @def INTEREST_MESSAGE_TYPE_LIST
 * @brief X-macro list for interest message command types.
 *
 * Each line defines one enum entry and its canonical string form.
 * Edit this list to add/remove commands; enums and string tables update in lockstep.
*/
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
    X(GET_BY_ID_FROM_NODE,            "getByIDFromNode") \
    X(GET_RESILIENCE_META,            "getResilienceMeta") \
    X(GET_RESILIENCE_PATCH,           "getResiliencePatch") \
    X(GET_RESILIENCE_IS_ALIVE,        "getResilienceIsAlive") \
    X(GET_RESILIENCE_SNAPSHOT,        "getResilienceSnapshot")

namespace parsed_interest_name {

    /**
     * @namespace parsed_interest_name::interest_message_type
     * @brief Enum and helpers for the Interest command taxonomy.
    */
    namespace interest_message_type {
        /**
         * @enum INTEREST_MESSAGE_TYPE
         * @brief Enumerates the supported Interest "command" components.
         *
         * These correspond to the string tokens present before a ':' in an
         * Interest name component (e.g., `temporalQuery:<kvpairs>`).
        */
        enum class INTEREST_MESSAGE_TYPE
        {
            #define X(name, str) name,
                INTEREST_MESSAGE_TYPE_LIST
            #undef X
        };
        /** @brief Parallel string table for #INTEREST_MESSAGE_TYPE. */
        static constexpr const char* INTEREST_MESSAGE_TYPE_STRINGS[] = 
        {
            #define X(name, str) str,
                INTEREST_MESSAGE_TYPE_LIST
            #undef X
        };
        /**
         * @brief Convert an #INTEREST_MESSAGE_TYPE to its canonical string.
         * @param type Enum value.
         * @return Pointer to constant string (empty string for out-of-range).
        */
        constexpr const char* to_string(const INTEREST_MESSAGE_TYPE type) noexcept
        {
            using U = std::underlying_type_t<INTEREST_MESSAGE_TYPE>;
            constexpr std::size_t N = sizeof(INTEREST_MESSAGE_TYPE_STRINGS) / sizeof(*INTEREST_MESSAGE_TYPE_STRINGS);

            U idx = static_cast<U>(type);

            return idx < N ? INTEREST_MESSAGE_TYPE_STRINGS[idx]: "";
        }
        /**
         * @brief Parse the command from a component of the form `"token:rest"`.
         *
         * @param message_component A component that may contain a colon (e.g., `"version:date=..."`).
         * @return Parsed enum or #INTEREST_MESSAGE_TYPE::EMPTY if no match or missing colon.
         *
         * @note Only the token before the first ':' is considered.
        */
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
    /**
     * @brief URL-encode a value for query contexts.
     *
     * @param value Raw string to encode.
     * @return Encoded string suitable for inclusion in URLs.
     * @note Leaves alnum, '-', '_', '.', '~' unchanged; encodes all others as %HH.
    */
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
    /**
     * @brief Convert a single hex digit to its numeric value.
     * @param c Hex digit ('0'-'9','a'-'f','A'-'F').
     * @return Value in [0,15].
     * @throws std::invalid_argument if @p c is not a valid hex digit.
    */
    static int hex_val(char c) 
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        throw std::invalid_argument(std::string("Invalid hex digit: ") + c);
    }

    // Decode percent‐escapes in a URL‐query fragment.
    // Does NOT treat '+' as space; '+' remains '+'. 
    //TODO I can't be bothered to include the snds utils lib
    /**
     * @brief URL-decode a string (percent-escapes only).
     *
     * @param in Input view (e.g., `"date%3A2025-06-29"`).
     * @return Decoded string. `'+'` remains `'+'` (no space conversion).
     *
     * @note This routine does not treat '+' as space; it only handles `%HH`.
    */
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
     *
     * @details
     * Decomposes an Interest into:
     *  - `prefix`  : leading component(s), typically `"/SNDS"` (without the leading slash in the stored string)
     *  - `name`    : entity or collection name (e.g., `"CAR"`)
     *  - `command` : parsed command token (see #interest_message_type::INTEREST_MESSAGE_TYPE)
     *  - `params`  : trailing parameters (positional tokens or `key=value` pairs depending on command)
    */
    struct ParsedInterestName {

        // SNDS/<name>/<params>
        // SNDS/<name>/temporalQuery:date=<value>&timeframe=<value>&count=<value>&?ndn_params
        // SNDS/<name>/version:date=<value>?our_params?ndn_params
        // TODO below 
        // SNDS/<name>_registry/
        // transfer to this SNDS/<name>/registry
        std::string                                  prefix;   ///< First major component (e.g., "SNDS").
        std::string                                  name;     ///< Entity/collection name (e.g., "CAR", "car2", or a scope).
        interest_message_type::INTEREST_MESSAGE_TYPE command;  ///< Parsed command (e.g., TEMPORAL_QUERY, VERSION).
        std::vector<std::string>                     params;   ///< Additional parameters (e.g., IDs, node identifiers, kv pairs).

        /**
         * @brief Convert the parsed structure to a human-readable string.
         *
         * @return String with fields and parameters (for debugging/logging).
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



    /**
     * @name Convenience constructors for metafile-style interests
     * @brief Helpers producing #ParsedInterestName variants for metafile commands.
     */
    ///@{
    /**
     * @brief Build a ParsedInterestName from string command and timestamp.
     * @param prefix    Leading component, e.g., `"SNDS"`.
     * @param id        Object identifier (stored into #ParsedInterestName::name).
     * @param command   Command as string, e.g., `"version:..."` (token before ':' is used).
     * @param timestamp Timestamp parameter appended to #ParsedInterestName::params.
     * @return Filled #ParsedInterestName.
    */
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
    /**
     * @brief Build a ParsedInterestName from enum command and timestamp.
     * @param prefix    Leading component.
     * @param id        Object identifier.
     * @param command   Command as enum.
     * @param timestamp Timestamp parameter appended to params.
     * @return Filled #ParsedInterestName.
    */
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
    /**
     * @brief Build a ParsedInterestName from enum command and an explicit parameter list.
     * @param prefix  Leading component.
     * @param id      Object identifier.
     * @param command Command as enum.
     * @param params  Parameters vector (copied).
     * @return Filled #ParsedInterestName.
    */
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
    ///@}

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
     * @brief Parse all `params` of a #ParsedInterestName into a key→value map.
     *
     * @param pi Parsed interest with `params` already populated.
     * @return Map of `key -> value` for each parameter token.
    */
    std::map<std::string, std::string> parse_params_map(const ParsedInterestName& pi);
    /**
     * @brief Append a `key=value` pair to a ParsedInterestName’s parameter list.
     * @param pi    Target instance.
     * @param key   Parameter key (without `=`).
     * @param value Parameter value.
    */
    void add_param(ParsedInterestName& pi, std::string key, std::string value);
    /**
     * @brief Build a query URI string from base path and `k=v` tokens.
     *
     * @param base_path   Path prefix (no trailing '?' required).
     * @param kv_tokens   Vector of `"key=value"` strings.
     * @param encode_values If true, percent-encode the value part of each token.
     * @return Assembled URI string (e.g., `base?key=value&k2=v2`).
    */
    std::string build_query_uri(std::string_view base_path, const std::vector<std::string>& kv_tokens, bool encode_values = false);
    /**
     * @brief Split a `"key=value"` view, keeping empty values and values containing '='.
     * @param p Input token view.
     * @return `{key, value}` (value may be empty).
    */
    static inline std::pair<std::string, std::string> parse_param_kv(std::string_view p)
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
     * @brief Split a string on a delimiter, preserving empty segments.
     * @param s     Input view.
     * @param delim Delimiter character.
     * @return Vector of segments (empty segments included).
    */
    static inline std::vector<std::string> split_keep_empty(std::string_view s, char delim)
    {
        std::vector<std::string> out;
        size_t start = 0;
        while (true) 
        {
            size_t pos = s.find(delim, start);
            if (pos == std::string_view::npos) 
            {
                out.emplace_back(s.substr(start));         // final segment (may be empty)
                break;
            }
            out.emplace_back(s.substr(start, pos - start)); // may be empty when pos==start
            start = pos + 1;
        }
        return out;
    }
    /**
     * @brief Parse an NDN name into a #ParsedInterestName.
     *
     * @code
     *   /SNDS/CAR/car1
     *   → prefix="SNDS", name="CAR", params=["car1"], command=EMPTY
     * @endcode
     *
     * @param name The NDN name to parse.
     * @return Parsed interest with extracted fields.
     * @warning Heuristic parser; may not handle all edge cases.
    */
    ParsedInterestName parse_interest_name(const ndn::Name& name);
    /**
     * @brief Reconstruct a custom NDN name string from a #ParsedInterestName.
     *
     * @details
     * Produces a path of the form:
     * `/prefix/name[/command:kvpairs][/param1[/param2...]]`
     *
     * @param pi               Source structure.
     * @param url_encode_flag  If true, URL-encode params/values.
     * @return Custom NDN name string.
    */
    std::string to_custom_ndn_name(const ParsedInterestName& pi, const bool url_encode_flag = false);
    /**
     * @brief Join parameters of @p pi into a query string.
     *
     * @details
     * Assumes parameters already carry their `key=value` form where appropriate.
     * E.g., `"id=car1&date=2025-06-29"`.
     *
     * @param pi Source instance.
     * @return Query string (without leading '?').
    */
    std::string to_query_string(const ParsedInterestName& pi);
    /**
     * @brief Construct a #ParsedInterestName from individual parts.
     *
     * @param prefix  First component (e.g., `"SNDS"` or `"SNDS/service"`).
     * @param name    Second component (e.g., `"car2"`).
     * @param command Enum identifying command (e.g., TEMPORAL_QUERY, VERSION).
     * @param params  Extra parameters (already URI-encoded if needed, without leading '/').
     * @return Filled #ParsedInterestName.
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
    /**
     * @brief Split a view on a delimiter (drops empties).
     * @param view Input view.
     * @param delimeter Delimiter character.
     * @return Vector of non-empty tokens.
    */
    std::vector<std::string> split(std::string_view view, const char delimeter);
    /**
     * @brief Parse a string representation of a name into #ParsedInterestName.
     *
     * @param name Path-like string, e.g., `"/SNDS/CAR/car1"`.
     * @return Parsed interest structure.
    */
    //ParsedInterestName parse_from_string(const std::string& name);

    ParsedInterestName parse_from_string(const std::string& name, std::string_view known_prefix);

}