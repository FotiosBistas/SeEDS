#pragma once
/**
 * @file snds_intra_message.hpp
 * @brief Intra-service message taxonomy and helper container for SNDS.
 *
 * @details
 * What this file gives you:
 *  1) A *single source of truth* for all intra-service message kinds,
 *     HTTP verbs, control/ACK types, and client roles — defined with
 *     X-macros so names and their string forms never drift.
 *  2) A tiny, value-type container (#snds_intra_message::SNDS_INTRA_MESSAGE) that carries:
 *        - message (primary body) + payload (secondary body)
 *        - strongly-typed enums (message type, sender role, HTTP verb)
 *        - a generated set of optional query parameters (via QUERY_PARAM_LIST)
 *     …plus utilities to:
 *        - infer message type from HTTP verb + present parameters
 *        - pack/unpack parameters into a compact “{k=v,k2=v2}” wire blob
 *        - pretty-print the message for debugging/logging
 *
 * ### Why X-macros?
 * - You only edit the lists below to add/remove types; enums, string tables,
 *   and to_string()/from_string() stay correct automatically.
 * - It’s easy to expose the same taxonomy in other components by reusing
 *   these lists.
 *
 * ### Thread-safety & lifetime
 * - SNDS_INTRA_MESSAGE is a simple, non-thread-safe value type; pass/own it
 *   per thread or protect externally.
 * - All strings are owned (std::string). Move APIs are provided for zero-copy.
 *
 * ### Parameter packing format
 * - Human-readable and robust to missing keys:
 *   `"{key=value,key2=value2}"` — no escaping; intended for simple tokens.
 *   Use JSON in `payload` if you need structured values.
 */

#include <sstream>
#include <cstring>
#include <iostream>
#include <cstddef> 
#include <optional>
#include <unordered_map>
#include <string_view>
#include <type_traits>

#include "query_params.hpp"

//-----------------------------------------------------------------------------
// 1) Message type taxonomies (X-macros)
//-----------------------------------------------------------------------------
// Add/remove lines in the lists below to extend the taxonomy across the codebase.
// Each X(name,"STRING") entry becomes an enum symbol and a string table entry.
#define SNDS_MESSAGE_TYPE_LIST \
    X(GENERIC,                                 "GENERIC") \
    X(BY_ID_ADVERTISEMENT,                     "BY_ID_ADVERTISEMENT") \
    X(BY_TYPE_ADVERTISEMENT,                   "BY_TYPE_ADVERTISEMENT") \
    X(BY_ID_AND_TYPE_ADVERTISEMENT,            "BY_ID_AND_TYPE_ADVERTISEMENT") \
    X(BY_ID_WITH_SCOPE_ADVERTISEMENT,          "BY_ID_WITH_SCOPE_ADVERTISEMENT") \
    X(BY_ID_AND_TYPE_WITH_SCOPE_ADVERTISEMENT, "BY_ID_AND_TYPE_WITH_SCOPE_ADVERTISEMENT") \
    X(BY_ID_SUBSCRIPTION,                      "BY_ID_SUBSCRIPTION") \
    X(BY_TYPE_SUBSCRIPTION,                    "BY_TYPE_SUBSCRIPTION") \
    X(GET_BY_ID_RESPONSE,                      "GET_BY_ID_RESPONSE") \
    X(GET_BY_TYPE_RESPONSE,                    "GET_BY_TYPE_RESPONSE") \
    X(GET_BY_ID_REQUEST,                       "GET_BY_ID_REQUEST") \
    X(GET_BY_ID_FOR_TYPE_REQUEST,              "GET_BY_ID_FOR_TYPE_REQUEST") \
    X(GET_BY_ID_FOR_TYPE_RESPONSE,             "GET_BY_ID_FOR_TYPE_RESPONSE") \
    X(GET_BY_TYPE_REQUEST,                     "GET_BY_TYPE_REQUEST") \
    X(GET_BY_ID_AND_TYPE_REQUEST,              "GET_BY_ID_AND_TYPE_REQUEST") \
    X(GET_BY_ID_FROM_NODE_REQUEST,             "GET_BY_ID_FROM_NODE_REQUEST") \
    X(GET_BY_ID_FROM_NODE_RESPONSE,            "GET_BY_ID_FROM_NODE_RESPONSE") \
    X(VERSION_RESPONSE,                        "VERSION_RESPONSE") \
    X(REGISTRY_PAYLOAD,                        "REGISTRY_PAYLOAD") \
    X(ERASE_PENDING_ID,                        "ERASE_PENDING_ID") \
    X(SUBSCRIPTION_NOTIFICATION,               "SUBSCRIPTION_NOTIFICATION") \
    X(SUBSCRIPTION_NOTIFICATION_DATA,          "SUBSCRIPTION_NOTIFICATION_DATA") \
    X(TEMPORAL_QUERY_REQUEST,                  "TEMPORAL_QUERY_REQUEST") \
    X(TEMPORAL_QUERY_RESPONSE,                 "TEMPORAL_QUERY_RESPONSE") \
    X(TEMPORAL_QUERY_ADVERTISEMENT,            "TEMPORAL_QUERY_ADVERTISEMENT") \
    X(CONTROL_MESSAGE,                         "CONTROL_MESSAGE") 

#define HTTP_MESSAGE_TYPE_LIST \
    X(HTTP_GET,  "HTTP_GET") \
    X(HTTP_POST, "HTTP_POST") \
    X(HTTP_EMPTY,"HTTP_EMPTY") \

#define SNDS_CONTROL_MESSAGE_TYPE_LIST \
    X(ACK_SUBSCRIPTION_NEW,            "ACK_SUBSCRIPTION_NEW") \
    X(ACK_SUBSCRIPTION_EXISTS,         "ACK_SUBSCRIPTION_EXISTS") \
    X(ACK_SUBSCRIPTION_NOTIFICATION,   "ACK_SUBSCRIPTION_NOTIFICATION") \
    X(ACK_SUBSCRIPTION_NO_SUBSCRIBERS, "ACK_SUBSCRIPTION_NO_SUBSCRIBERS") \
    X(ACK_SCOPE_NOTIFICATION,          "ACK_SCOPE_NOTIFICATION") \
    X(ACK_ID_INTEREST,                 "ACK_ID_INTEREST") \
    X(ACK_ID_REGISTERED,               "ACK_ID_REGISTERED") \
    X(ACK_NODE_NOTIFIED,               "ACK_NODE_NOTIFIED") \
    X(ACK_ID_ADVERTISED,               "ACK_ID_ADVERTISED") \
    X(ACK_TEMPORAL_QUERY_ADVERTISED,   "ACK_TEMPORAL_QUERY_ADVERTISED") \
    X(NACK_ERROR,                      "NACK_ERROR") \
    X(TIMEOUT,                         "TIMEOUT") \
    X(WARNING_ALREADY_REGISTERED,      "WARNING_ALREADY_REGISTERED")

#define SNDS_CLIENT_TYPE_LIST \
    X(PROXY,         "PROXY") \
    X(PRODUCER,      "PRODUCER") \
    X(CONSUMER,      "CONSUMER") \
    X(DIGITAL_TWIN,  "DIGITAL_TWIN") \
    X(SERVICE,       "SERVICE") \
    X(HTTP_SRV,      "HTTP_SRV") \
    X(WORKER,        "WORKER") \
    X(ON_INTEREST,   "ON_INTEREST")

namespace snds_intra_message
{

    //-------------------------------------------------------------------------
    // 2) Enums + string tables (auto-generated from the lists)
    //-------------------------------------------------------------------------

    /**
     * @enum HTTP_MESSAGE_TYPE
     * @brief HTTP verbs used to interpret queries/ads.
     */
    enum class HTTP_MESSAGE_TYPE 
    {
        #define X(name, str) name, 
            HTTP_MESSAGE_TYPE_LIST
        #undef X
    };

    /** @brief Parallel string table for #HTTP_MESSAGE_TYPE. */
    static constexpr const char* HTTP_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            HTTP_MESSAGE_TYPE_LIST
        #undef X
    };

    /**
     * @enum SNDS_INTRA_MESSAGE_TYPE
     * @brief Primary intra-service message kinds (requests/responses/ads/etc).
     */
    enum class SNDS_INTRA_MESSAGE_TYPE 
    {
        #define X(name, str) name,
            SNDS_MESSAGE_TYPE_LIST
        #undef X
    };

    /** @brief Parallel string table for #SNDS_INTRA_MESSAGE_TYPE. */
    static constexpr const char* SNDS_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_MESSAGE_TYPE_LIST
        #undef X
    };

    /**
     * @enum SNDS_CONTROL_MESSAGE_TYPE
     * @brief Control/acknowledgement messages (ACK/NACK, warnings).
     */
    enum class SNDS_CONTROL_MESSAGE_TYPE
    {
        #define X(name, str) name,
            SNDS_CONTROL_MESSAGE_TYPE_LIST
        #undef X
    };

    /** @brief Parallel string table for #SNDS_CONTROL_MESSAGE_TYPE. */
    static constexpr const char* SNDS_CONTROL_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_CONTROL_MESSAGE_TYPE_LIST
        #undef X
    };

    /**
     * @enum SNDS_CLIENT_TYPES
     * @brief Logical client roles participating in the bus.
     */
    enum class SNDS_CLIENT_TYPES 
    {
        #define X(name, str) name,
            SNDS_CLIENT_TYPE_LIST
        #undef X
    };

    /** @brief Parallel string table for #SNDS_CLIENT_TYPES. */
    static constexpr const char* SNDS_CLIENT_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_CLIENT_TYPE_LIST
        #undef X
    };

    //-------------------------------------------------------------------------
    // 3) enum_traits<E> — compile-time mapping enum → string table
    //-------------------------------------------------------------------------

    /**
     * @brief Primary template for enum → string-table traits.
     * @tparam E Enum type.
     */
    template<typename E> struct enum_traits;

    /** @brief Traits for #SNDS_INTRA_MESSAGE_TYPE. */
    template<> struct enum_traits<SNDS_INTRA_MESSAGE_TYPE> 
    {
        /// @return pointer to the string table for #SNDS_INTRA_MESSAGE_TYPE.
        static constexpr const char* const* data() noexcept 
        { 
            return SNDS_MESSAGE_TYPE_STRINGS; 
        }
        /// @return number of entries in the string table.
        static constexpr std::size_t size() noexcept 
        {
            return sizeof(SNDS_MESSAGE_TYPE_STRINGS) / sizeof(*SNDS_MESSAGE_TYPE_STRINGS);
        }
    };

    /** @brief Traits for #SNDS_CONTROL_MESSAGE_TYPE. */
    template<> struct enum_traits<SNDS_CONTROL_MESSAGE_TYPE> 
    {
        static constexpr const char* const* data() noexcept 
        { 
            return SNDS_CONTROL_MESSAGE_TYPE_STRINGS; 
        }
        static constexpr std::size_t size() noexcept 
        {
            return sizeof(SNDS_CONTROL_MESSAGE_TYPE_STRINGS) / sizeof(*SNDS_CONTROL_MESSAGE_TYPE_STRINGS);
        }
    };

    /** @brief Traits for #SNDS_CLIENT_TYPES. */
    template<> struct enum_traits<SNDS_CLIENT_TYPES> 
    {
        static constexpr const char* const* data() noexcept 
        { 
            return SNDS_CLIENT_TYPE_STRINGS; 
        }
        static constexpr std::size_t size() noexcept 
        {
            return sizeof(SNDS_CLIENT_TYPE_STRINGS) / sizeof(*SNDS_CLIENT_TYPE_STRINGS);
        }
    };

    /** @brief Traits for #HTTP_MESSAGE_TYPE. */
    template<> struct enum_traits<HTTP_MESSAGE_TYPE> 
    {
        static constexpr const char* const* data() noexcept 
        { 
            return HTTP_MESSAGE_TYPE_STRINGS; 
        }
        static constexpr std::size_t size() noexcept 
        {
            return sizeof(HTTP_MESSAGE_TYPE_STRINGS) / sizeof(*HTTP_MESSAGE_TYPE_STRINGS);
        }
    };

    //-------------------------------------------------------------------------
    // 4) Enum <-> string helpers (generic for any E with enum_traits<E>)
    //-------------------------------------------------------------------------

    /**
     * @brief Convert an enum value to its string representation.
     * @tparam E Enum type with a matching enum_traits<E>.
     * @param e Enum value.
     * @return String view for the enum value, or `"UNKNOWN"` if out-of-range.
     */
    template<typename E>
    constexpr const char* to_string(E e) noexcept
    {
        using U = std::underlying_type_t<E>;
        U idx = static_cast<U>(e);

        constexpr auto N = enum_traits<E>::size();
        return idx < N ? enum_traits<E>::data()[idx] : "UNKNOWN";
    }

    /**
     * @brief Parse a string into an enum value.
     * @tparam E Enum type with a matching enum_traits<E>.
     * @param s Input string.
     * @return Parsed enum wrapped in std::optional; std::nullopt if no match.
     */
    template<typename E>
    std::optional<E> from_string(std::string_view s) noexcept 
    {
        using U = std::underlying_type_t<E>;
        constexpr auto* data = enum_traits<E>::data();
        constexpr std::size_t N = enum_traits<E>::size();
        for (U i = 0; i < N; ++i) 
        {
            if (s == data[i])
            {
                return static_cast<E>(i);
            }
        }
        return std::nullopt;
    }

    //-------------------------------------------------------------------------
    // 5) Small token helpers used by ACK/NACK sniffing
    //-------------------------------------------------------------------------

    /**
     * @brief Extract the first non-whitespace word; strips any NUL suffix.
     * @param s Raw input text (may contain embedded NULs).
     * @return First token (as a string_view into @p s) or empty if none.
     */
    static std::string_view extract_token(std::string_view s) noexcept 
    {
        // If there’s a '\0' anywhere, lose it and everything after.
        if (auto nz = s.find('\0'); nz != std::string_view::npos) 
        {
            s.remove_suffix(s.size() - nz);
        }

        // Skip leading whitespace (spaces, tabs, CR/LF).
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) 
        {
            return {};  // all whitespace or empty
        }

        // Find the end of that first word (up to the next whitespace).
        auto end = s.find_first_of(" \t\r\n", start);
        if (end == std::string_view::npos) 
        {
            end = s.size();
        }

        return s.substr(start, end - start);
    }

    /**
     * @brief Quick check: does a raw line start with a known control message?
     * @param raw Raw line to test.
     * @return true if the first token maps to a #SNDS_CONTROL_MESSAGE_TYPE.
     */
    inline bool is_ack_message(std::string_view raw) noexcept 
    {
        auto tok = extract_token(raw);
        return from_string<SNDS_CONTROL_MESSAGE_TYPE>(tok).has_value();
    }

    //-------------------------------------------------------------------------
    // 6) SNDS_INTRA_MESSAGE — light container with typed params & utilities
    //
    // Typical flow:
    //   1) Construct and set http_type/sender
    //   2) Set relevant params (e.g., set_id_("ABC"), set_type_("CAR"))
    //   3) infer_type_from_params()  → classifies the message
    //   4) pack_query_params_into_message() to embed params into message
    //
    // Example (GET by id):
    //   SNDS_INTRA_MESSAGE m;
    //   m.set_http_type(HTTP_MESSAGE_TYPE::HTTP_GET);
    //   m.set_id_("vehicle-42");
    //   m.infer_type_from_params();  // → GET_BY_ID_REQUEST
    //   m.pack_query_params_into_message(); // message = "{id=vehicle-42}"
    //
    // Example (POST advertisement with id+type):
    //   SNDS_INTRA_MESSAGE m;
    //   m.set_http_type(HTTP_MESSAGE_TYPE::HTTP_POST);
    //   m.set_id_("vehicle-42"); m.set_type_("CAR");
    //   m.infer_type_from_params(); // → BY_ID_AND_TYPE_ADVERTISEMENT
    //-------------------------------------------------------------------------

    /**
     * @brief Represents an intra-service message with content, type, sender, and optional parameters.
     *
     * @details
     * Carries two bodies (`message` and `payload`), three enums (message kind,
     * sender role, HTTP verb), and a generated set of optional parameters defined
     * by `QUERY_PARAM_LIST`. Includes helpers to infer message kind, pack/unpack
     * parameters, and pretty-print for debugging.
     */
    struct SNDS_INTRA_MESSAGE {
    private:
        std::string message;                 ///< Message content (owned by the object).
        std::string payload;                 ///< Optional extra payload for the message.
        SNDS_INTRA_MESSAGE_TYPE type;        ///< Type of the message (e.g., GENERIC, BY_ID_RESPONSE).
        SNDS_CLIENT_TYPES sender_component;  ///< Sender component responsible for this message.
        HTTP_MESSAGE_TYPE http_type;         ///< HTTP method associated with this message.

        // For each (var,"tag",dec,type) in QUERY_PARAM_LIST we create:
        //    std::optional<type> var_;
        // This allows the message to carry strongly-typed optional params without
        // templating the entire container.
        #define DECLARE_FIELD(var, tag, dec, type) std::optional<type> var##_;
            QUERY_PARAM_LIST(DECLARE_FIELD)
        #undef DECLARE_FIELD

        /**
         * @brief Inference rules for HTTP GET requests.
         * @return Derived #SNDS_INTRA_MESSAGE_TYPE based on current parameters.
         */
        SNDS_INTRA_MESSAGE_TYPE infer_from_get() const
        {
            if (id_.has_value() && is_subscription_.has_value() && is_subscription_.value())
            {
                // NOTE: subscription_url is currently not considered in inference.
                return SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION;
            }

            if (type_.has_value() && is_subscription_.has_value() && is_subscription_.value())
            {
                return SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_SUBSCRIPTION;
            }

            if (date_.has_value() || timeframe_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST;

            // if (id_.has_value() && type_.has_value())
            //     return SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_AND_TYPE_REQUEST;

            if (type_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::GET_BY_TYPE_REQUEST;

            if (id_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST;

            return SNDS_INTRA_MESSAGE_TYPE::GENERIC;
        }

        /**
         * @brief Inference rules for HTTP POST requests.
         * @return Derived #SNDS_INTRA_MESSAGE_TYPE based on current parameters.
         */
        SNDS_INTRA_MESSAGE_TYPE infer_from_post() const
        {
            if (date_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_ADVERTISEMENT;

            if (id_.has_value() && type_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT;

            if (id_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT;

            if (type_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_ADVERTISEMENT;

            return SNDS_INTRA_MESSAGE_TYPE::GENERIC;
        }

        /// Delimiter used between `k=v` pairs in packed params.
        static constexpr const char parameter_packing_delimeter    = ',';
        /// Leading brace in packed params.
        static constexpr const char parameter_prefix_delimeter     = '{';
        /// Trailing brace in packed params.
        static constexpr const char parameter_postfix_delimeter    = '}';
        /// Assignment character between key and value in packed params.
        static constexpr const char parameter_assignment_delimeter = '=';

    public:
        //-------------------------------------------------------------------------
        // Constructors / basic metadata mutators
        //-------------------------------------------------------------------------

        /**
         * @brief Construct a new message with explicit fields (rvalue-friendly).
         * @param message      Primary message content.
         * @param message_type Intra message type.
         * @param sender       Sender component role.
         * @param payload      Secondary payload body.
         * @param http_type    HTTP verb associated with this message.
         */
        SNDS_INTRA_MESSAGE
        (
            std::string&& message, 
            SNDS_INTRA_MESSAGE_TYPE message_type, 
            SNDS_CLIENT_TYPES sender, 
            std::string&& payload,
            HTTP_MESSAGE_TYPE http_type
        ):
            message(std::move(message)), 
            type(message_type), 
            sender_component(sender), 
            payload(std::move(payload)),
            http_type(http_type)
        {}

        /**
         * @brief Construct a default message (empty bodies, GENERIC type, PROXY sender).
         */
        SNDS_INTRA_MESSAGE()
            : message(""),
              payload(""),
              type(SNDS_INTRA_MESSAGE_TYPE::GENERIC),
              sender_component(SNDS_CLIENT_TYPES::PROXY),
              http_type(HTTP_MESSAGE_TYPE::HTTP_EMPTY)
        {}

        /**
         * @brief Set the payload (move-only).
         * @param payload New payload string to move in.
         */
        void set_payload(std::string&& payload) 
        {
            this->payload = std::move(payload);
        }

        /**
         * @brief Set the sender role.
         * @param sender Sender component (e.g., PROXY, SERVICE).
         */
        void set_sender(SNDS_CLIENT_TYPES sender) 
        {
            this->sender_component = sender;
        }

        /**
         * @brief Set the HTTP message type.
         * @param http_type HTTP verb.
         */
        void set_http_type(HTTP_MESSAGE_TYPE http_type) 
        {
            this->http_type = http_type;
        }

        /// @note Deleted to enforce move-only construction for `message`/`payload`.
        SNDS_INTRA_MESSAGE(const std::string& message, SNDS_INTRA_MESSAGE_TYPE message_type, SNDS_CLIENT_TYPES sender, const std::string& message_name, HTTP_MESSAGE_TYPE http_type) = delete;
        /// @note Deleted to avoid accidental copies into payload.
        void set_payload(const std::string& message) = delete;

        /**
         * @brief Get the length of the #message content.
         * @return Number of characters in #message.
         */
        size_t get_message_length() const 
        {
            return message.size();
        }

        /**
         * @brief Get the length of the #payload content.
         * @return Number of characters in #payload.
         */
        size_t get_payload_length() const 
        {
            return payload.size();
        }

        /**
         * @brief Access the #message content.
         * @return Reference to the message string.
         */
        const std::string& get_message() const 
        {
            return message;
        }

        /**
         * @brief Move the #message content out, leaving it empty.
         * @return Rvalue string previously stored in #message.
         */
        std::string&& move_message()
        {
            return std::move(message);
        }

        /**
         * @brief Move the #payload content out, leaving it empty.
         * @return Rvalue string previously stored in #payload.
         */
        std::string&& move_payload()
        {
            return std::move(payload);
        }

        /**
         * @brief Access the #payload content.
         * @return Reference to the payload string.
         */
        const std::string& get_payload() const 
        {
            return payload;
        }

        /**
         * @brief Get the current intra message type.
         * @return #SNDS_INTRA_MESSAGE_TYPE value.
         */
        const SNDS_INTRA_MESSAGE_TYPE get_message_type() const 
        {
            return type;
        }

        /**
         * @brief Get the sender component role.
         * @return #SNDS_CLIENT_TYPES value.
         */
        SNDS_CLIENT_TYPES get_sender() const 
        {
            return sender_component;
        }

        /**
         * @brief Pretty-print message fields and present parameters, and return the same text.
         * @return A string with all fields and present parameters.
         */
        std::string print_details() const
        {
            std::ostringstream oss;
            oss << "Message: "     << message
                << "\nType: "      << to_string(type)
                << "\nSender: "    << to_string(sender_component)
                << "\nPayload: "   << payload
                << "\nHTTP Type: " << to_string(http_type)
                << "\nParams:\n";

            // For every field in QUERY_PARAM_LIST, print "  <tag> = <value>" if it has a value.
            #define PRINT_FIELD(var, tag, dec, type) \
                if (var##_.has_value()) {            \
                    oss << tag << " = ";             \
                    oss << var##_.value();           \
                    oss << "\n";                     \
                }
            QUERY_PARAM_LIST(PRINT_FIELD)
            #undef PRINT_FIELD

            std::string details = oss.str();
            std::cout << details;
            return details;
        }

        //-------------------------------------------------------------------------
        // Generated setters/getters for every QUERY_PARAM_LIST entry
        //
        // For each (var,"tag",dec,type):
        //   set_var_(type)               — stores value into std::optional<type>
        //   get_var_() -> optional<type> — returns copy if set
        //   get_param_name_var_()        — returns "tag" (compile-time constant)
        //-------------------------------------------------------------------------

        /** @name Generated setters for optional parameters */
        ///@{
        #define DECLARE_SETTER(var, tag, dec, type)    \
        void set_##var##_(type v) {                    \
            var##_ = std::move(v);                     \
        } 
        QUERY_PARAM_LIST(DECLARE_SETTER) 
        #undef DECLARE_SETTER
        ///@}

        /** @name Generated getters for optional parameters */
        ///@{
        #define DECLARE_GETTER(var, tag, dec, type)    \
        std::optional<type> get_##var##_() const {     \
            if(!var##_.has_value()){                   \
                return std::nullopt;                   \
            }                                          \
            return var##_;                             \
        } 
        QUERY_PARAM_LIST(DECLARE_GETTER) 
        #undef DECLARE_GETTER
        ///@}

        /** @name Generated name-accessors for parameter tags */
        ///@{
        #define DECLARE_NAME_GETTER(var, tag, dec, type)           \
        constexpr std::string_view get_param_name_##var##_() const \
        {                                                          \
            return tag;                                            \
        }                                                                      
        QUERY_PARAM_LIST(DECLARE_NAME_GETTER)
        #undef DECLARE_NAME_GETTER
        ///@}

        //-------------------------------------------------------------------------
        // Parameter packing/unpacking: compact, human-readable blob
        //-------------------------------------------------------------------------

        /**
         * @brief Serialize all present parameters into #message as "{k=v,k2=v2}".
         * @warning Values are not escaped — keep them simple (no commas/equals/braces).
         *          For complex payloads, use JSON in #payload instead.
         */
        void pack_query_params_into_message() 
        {
            std::ostringstream oss;
            oss << parameter_prefix_delimeter;
            bool first = true;

            // Use the X-macro to generate one append-if-present per field
            #define APPEND_FIELD(var, tag, dec, type)                                       \
                if (var##_.has_value())                                                     \
                {                                                                           \
                    if (!first) oss << parameter_packing_delimeter;                         \
                    oss << tag << parameter_assignment_delimeter << var##_.value();         \
                    first = false;                                                          \
                }                                                                                                                                                        
            QUERY_PARAM_LIST(APPEND_FIELD)
            #undef APPEND_FIELD

            oss << parameter_postfix_delimeter;
            // Store it in the message body
            this->message = oss.str();
        }

        /**
         * @brief Interpret #message as "{k=v,k2=v2}" and return a key/value map.
         * @return Unordered map of parameter key→value; missing '=' yields empty value.
         */
        std::unordered_map<std::string, std::string> unpack_query_params_from_message() const
        {
            std::unordered_map<std::string, std::string> params;
        
            std::string_view payload = message;
        
            // Trim prefix and postfix
            if (!payload.empty() && payload.front() == parameter_prefix_delimeter) payload.remove_prefix(1);
            if (!payload.empty() && payload.back() == parameter_postfix_delimeter) payload.remove_suffix(1);
        
            // Helper: extract next token
            auto next_token = [&](std::string_view& sv) -> std::string_view 
            {
                if (sv.empty()) return {};
                size_t pos = sv.find(parameter_packing_delimeter);
                if (pos == std::string_view::npos) 
                {
                    std::string_view out = sv;
                    sv = {};
                    return out;
                }
                std::string_view out = sv.substr(0, pos);
                sv.remove_prefix(pos + 1);
                return out;
            };
        
            while (!payload.empty())
            {
                std::string_view pair_str = next_token(payload);
                if (pair_str.empty()) continue;
            
                // Split key and value
                size_t eq_pos = pair_str.find(parameter_assignment_delimeter);
                if (eq_pos == std::string_view::npos) 
                {
                    // Malformed — no '=' found; store empty value
                    params.insert({std::string(pair_str), ""});
                    continue;
                }
            
                std::string_view key = pair_str.substr(0, eq_pos);
                std::string_view val = pair_str.substr(eq_pos + 1);
                params.insert({std::string(key), std::string(val)});
            }
        
            return params;
        }

        //-------------------------------------------------------------------------
        // Message type inference
        //-------------------------------------------------------------------------

        /**
         * @brief Decide #type based on current #http_type and present parameters.
         *
         * @details
         * **GET rules**
         *  - id + is_subscription=true  → BY_ID_SUBSCRIPTION
         *  - type + is_subscription     → BY_TYPE_SUBSCRIPTION
         *  - date + timeframe           → TEMPORAL_QUERY_REQUEST
         *  - type                       → GET_BY_TYPE_REQUEST
         *  - id                         → GET_BY_ID_REQUEST
         *  - (else)                     → GENERIC
         *
         * **POST rules**
         *  - date                       → TEMPORAL_QUERY_ADVERTISEMENT
         *  - id + type                  → BY_ID_AND_TYPE_ADVERTISEMENT
         *  - id                         → BY_ID_ADVERTISEMENT
         *  - type                       → BY_TYPE_ADVERTISEMENT
         *  - (else)                     → GENERIC
         */
        void infer_type_from_params() 
        {
            using Fn = SNDS_INTRA_MESSAGE_TYPE (SNDS_INTRA_MESSAGE::*)() const;

            // Dispatch table indexed by underlying enum value of HTTP_MESSAGE_TYPE
            static constexpr Fn DISPATCH[] = 
            {
                &SNDS_INTRA_MESSAGE::infer_from_get,    // HTTP_GET
                &SNDS_INTRA_MESSAGE::infer_from_post,   // HTTP_POST
            };

            // Compile-time guard in case HTTP_MESSAGE_TYPE grows.
            static_assert
            (
                sizeof(DISPATCH)/sizeof(DISPATCH[0]) ==
                static_cast<std::size_t>(snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY),
                "DISPATCH table must match HTTP_MESSAGE_TYPE enum size/order"
            );

            const auto index = static_cast<std::underlying_type_t<HTTP_MESSAGE_TYPE>>(http_type);
            // Defensive check (in case of bad input):
            if (index < 0 || index >= static_cast<decltype(index)>(std::size(DISPATCH))) 
            {
                type = SNDS_INTRA_MESSAGE_TYPE::GENERIC;
                return;
            }

            type = (this->*DISPATCH[index])();
        }

        //-------------------------------------------------------------------------
        // Raw URI query parsing (k=v&k2=v2); tolerant of missing '=' tokens
        //-------------------------------------------------------------------------

        /**
         * @brief Convert a URI (or raw query string) into a key/value map.
         * @param uri Either a full URI ("...?...") or just the query string.
         * @return Unordered map with all pairs; items without '=' map to empty value.
         * @note This function does not URL-decode; use upstream parsing for that.
         */
        std::unordered_map<std::string, std::string> parse_uri_query_to_map_raw(std::string_view uri)
        {
            std::unordered_map<std::string, std::string> out;
        
            auto qpos = uri.find('?');
            std::string_view qs = (qpos == std::string_view::npos) ? uri : uri.substr(qpos + 1);
            while (!qs.empty() && (qs.front() == '/' || qs.front() == '?')) qs.remove_prefix(1);
        
            auto next = [](std::string_view& sv)->std::string_view 
            {
                if (sv.empty()) return {};
                size_t pos = sv.find('&');
                if (pos == std::string_view::npos) 
                { 
                    auto r = sv; 
                    sv = {}; 
                    return r; 
                }
                auto r = sv.substr(0,pos);
                sv.remove_prefix(pos+1);
                return r;
            };
        
            while (!qs.empty()) 
            {
                std::string_view pair = next(qs);
                if (pair.empty()) continue;
            
                size_t eq = pair.find('=');
                std::string key = std::string((eq == std::string_view::npos) ? pair : pair.substr(0,eq));
                std::string val = std::string((eq == std::string_view::npos) ? std::string_view{} : pair.substr(eq+1));
                if (!key.empty()) out.emplace(std::move(key), std::move(val));
            }
            return out;
        }

    };
}; // namespace snds_intra_message
