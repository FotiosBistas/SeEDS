#pragma once

#include <sstream>
#include <cstring>
#include <iostream>
#include <cstddef> 
#include <optional>
#include <type_traits>

#include "query_params.hpp"

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

    enum class HTTP_MESSAGE_TYPE 
    {

        #define X(name, str) name, 
            HTTP_MESSAGE_TYPE_LIST
        #undef X
    };

    static constexpr const char* HTTP_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            HTTP_MESSAGE_TYPE_LIST
        #undef X
    };


    /// @brief Defines the type of intra-service messages.
    enum class SNDS_INTRA_MESSAGE_TYPE 
    {
        #define X(name, str) name,
            SNDS_MESSAGE_TYPE_LIST
        #undef X
    };

    static constexpr const char* SNDS_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_MESSAGE_TYPE_LIST
        #undef X
    };

    enum class SNDS_CONTROL_MESSAGE_TYPE
    {
        #define X(name, str) name,
            SNDS_CONTROL_MESSAGE_TYPE_LIST
        #undef X
    };

    static constexpr const char* SNDS_CONTROL_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_CONTROL_MESSAGE_TYPE_LIST
        #undef X
    };

    enum class SNDS_CLIENT_TYPES 
    {
        #define X(name, str) name,
            SNDS_CLIENT_TYPE_LIST
        #undef X
    };

    static constexpr const char* SNDS_CLIENT_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_CLIENT_TYPE_LIST
        #undef X
    };

    // typename E gets replaced when enum_traits<SNDS_INTRA...> is called
    template<typename E> struct enum_traits;

    template<> struct enum_traits<SNDS_INTRA_MESSAGE_TYPE> 
    {
        static constexpr const char* const* data() noexcept 
        { 
            return SNDS_MESSAGE_TYPE_STRINGS; 
        }
        static constexpr std::size_t size() noexcept 
        {
            return sizeof(SNDS_MESSAGE_TYPE_STRINGS) / sizeof(*SNDS_MESSAGE_TYPE_STRINGS);
        }
    };

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

    template<typename E>
    constexpr const char* to_string(E e) noexcept
    {
        using U = std::underlying_type_t<E>;
        U idx = static_cast<U>(e);

        constexpr auto N = enum_traits<E>::size();
        return idx < N ? enum_traits<E>::data()[idx] : "UNKNOWN";
    }

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

    inline bool is_ack_message(std::string_view raw) noexcept 
    {
        auto tok = extract_token(raw);
        return from_string<SNDS_CONTROL_MESSAGE_TYPE>(tok).has_value();
    }

    /// @brief Represents an intra-service message with content, type, sender, and an optional name.
    struct SNDS_INTRA_MESSAGE {
    private:
        std::string message;                 ///< Message content (owned by the object).
        std::string payload;                 ///< Optional extra payload for the message.
        SNDS_INTRA_MESSAGE_TYPE type;        ///< Type of the message (e.g., GENERIC, BY_ID_RESPONSE).
        SNDS_CLIENT_TYPES sender_component;  ///< Sender component responsible for this message.
        HTTP_MESSAGE_TYPE http_type;         ///< HTTP type of them message.

        #define DECLARE_FIELD(var, tag, dec, type) std::optional<type> var##_;
            QUERY_PARAM_LIST(DECLARE_FIELD)
        #undef DECLARE_FIELD


        SNDS_INTRA_MESSAGE_TYPE infer_from_get() const
        {


            if (id_.has_value() && is_subscription_.has_value() && is_subscription_.value())
            {
                //TODO we don't take account URLs
                return SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION;
            }

            if(type_.has_value() && is_subscription_.has_value() && is_subscription_.value())
            {
                return SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_SUBSCRIPTION;
            }

            if (date_.has_value() && timeframe_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST;

            //if (id_.has_value() && type_.has_value())
            //    return SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_AND_TYPE_REQUEST;

            //new implementation requires routing through the temporal query API
            if (id_.has_value())
                //return SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST;
                return SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST;

            if (type_.has_value())
                return SNDS_INTRA_MESSAGE_TYPE::GET_BY_TYPE_REQUEST;


            return SNDS_INTRA_MESSAGE_TYPE::GENERIC;
        }

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

        static constexpr const char parameter_packing_delimeter    = ',';
        static constexpr const char parameter_prefix_delimeter     = '{';
        static constexpr const char parameter_postfix_delimeter    = '}';
        static constexpr const char parameter_assignment_delimeter = '=';

    public:
        /**
         * @brief Constructs a new SNDS_INTRA_MESSAGE with specified content, type, sender, and payload (rvalues only).
         * @param message The content of the message (rvalue).
         * @param message_type The type of the message (e.g., GENERIC, BY_ID_RESPONSE).
         * @param sender The sender component of the message (e.g., PROXY, SERVICE).
         * @param payload Extra payload for the message.
         * @param http_type The HTTP method that the message was received from
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
         * @brief Constructs a default SNDS_INTRA_MESSAGE with empty content and payload,
         * a type of GENERIC, and sender set to PROXY.
         */
        SNDS_INTRA_MESSAGE(): message(""), type(SNDS_INTRA_MESSAGE_TYPE::GENERIC), sender_component(SNDS_CLIENT_TYPES::PROXY), payload(""), http_type(HTTP_MESSAGE_TYPE::HTTP_EMPTY){}

        /**
         * @brief Sets the payload for the message (rvalue only).
         * @param payload The new payload or identifier (can be empty).
         */
        void set_payload(std::string&& payload) 
        {
            this->payload = std::move(payload);
        }

        /**
         * @brief Sets the sender component for the message.
         * @param sender The sender component (e.g., PROXY, SERVICE).
         */
        void set_sender(SNDS_CLIENT_TYPES sender) 
        {
            this->sender_component = sender;
        }

        /**
         * @brief Sets the HTTP message type for the message.
         * @param http_type The HTTP message type.
         */
        void set_http_type(HTTP_MESSAGE_TYPE http_type) 
        {
            this->http_type = http_type;
        }

        // Deleted constructors and setters for lvalues (forces explicit move)
        SNDS_INTRA_MESSAGE(const std::string& message, SNDS_INTRA_MESSAGE_TYPE message_type, SNDS_CLIENT_TYPES sender, const std::string& message_name, HTTP_MESSAGE_TYPE http_type) = delete;
        void set_payload(const std::string& message) = delete;

        /**
         * @brief Gets the length of the message content.
         * @return The number of characters in the message content.
         */
        size_t get_message_length() const 
        {
            return message.size();
        }

        /**
         * @brief Gets the length of payload.
         */
        size_t get_payload_length() const 
        {
            return payload.size();
        }

        /**
         * @brief Gets the message content of the class.
         * @return The reference to the string of the message class.
         */
        const std::string& get_message() const 
        {
            return message;
        }

        /**
         * @brief Moves out the message content of the class.
         * @return The string that was stored in the message (message is left empty).
         */
        std::string&& move_message()
        {
            return std::move(message);
        }

        /**
         * @brief Moves out the payload content of the class.
         * @return The string that was stored in the payload (payload is left empty).
         */
        std::string&& move_payload()
        {
            return std::move(payload);
        }

        /**
         * @brief Gets the payload content of the class.
         * @return The reference to the string of the message class.
         */
        const std::string& get_payload() const 
        {
            return payload;
        }

        /**
         * @brief Get the message type of the class.
         * @return Returns the enum value of the message type.
         */
        const SNDS_INTRA_MESSAGE_TYPE get_message_type() const 
        {
            return type;
        }

        /**
         * @brief Gets the sender component of the message.
         * @return The sender component (e.g., PROXY, SERVICE).
         */
        SNDS_CLIENT_TYPES get_sender() const 
        {
            return sender_component;
        }

        /**
         * @brief Prints the current message and its details (for debugging),
         *        and also returns the same information as a string.
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

        //CARE CARE CARE Adding new content (query params) fields
        //adds a setter for it
        #define DECLARE_SETTER(var, tag, dec, type)    \
        void set_##var##_(type v) {                    \
            var##_ = std::move(v);                     \
        } 
        QUERY_PARAM_LIST(DECLARE_SETTER) 
        #undef DECLARE_SETTER

        //CARE CARE CARE Adding new content (query params) fields
        //adds a getter for it
        #define DECLARE_GETTER(var, tag, dec, type)    \
        std::optional<type> get_##var##_() const {     \
            if(!var##_.has_value()){                   \
                return std::nullopt;                   \
            }                                          \
            return var##_;                             \
        } 
        QUERY_PARAM_LIST(DECLARE_GETTER) 
        #undef DECLARE_GETTER

        //CARE CARE CARE Adding new content (query params) fields
        //adds a name getter for it
        // Getter for parameter name
        #define DECLARE_NAME_GETTER(var, tag, dec, type)           \
        constexpr std::string_view get_param_name_##var##_() const \
        {                                                          \
            return tag;                                            \
        }                                                                      
        QUERY_PARAM_LIST(DECLARE_NAME_GETTER)
        #undef DECLARE_NAME_GETTER

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
            // Store it in the message payload
            this->message = oss.str();
        }

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

        void infer_type_from_params() 
        {
            using Fn = SNDS_INTRA_MESSAGE_TYPE (SNDS_INTRA_MESSAGE::*)() const;

            // Dispatch table indexed by underlying enum value of HTTP_MESSAGE_TYPE
            static constexpr Fn DISPATCH[] = 
            {
                &SNDS_INTRA_MESSAGE::infer_from_get,    // HTTP_GET
                &SNDS_INTRA_MESSAGE::infer_from_post,   // HTTP_POST
            };

            // Compile-time guard in case HTTP_MESSAGE_TYPE grows
            //Assumes:
            //the enum is 0-based and contiguous, and
            //HTTP_EMPTY is the last enumerator.
            //If both are true, then its underlying value is “last index".
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
};