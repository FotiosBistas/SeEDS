#pragma once

#include <sstream>
#include <cstring>
#include <iostream>
#include <cstddef> 
#include <optional>
#include <type_traits>


#define SNDS_MESSAGE_TYPE_LIST \
    X(GENERIC,                       "GENERIC") \
    X(BY_ID_RESPONSE,                "BY_ID_RESPONSE") \
    X(BY_ID_ADVERTISEMENT,           "BY_ID_ADVERTISEMENT") \
    X(BY_ID_WITH_SCOPE_ADVERTISEMENT,"BY_ID_WITH_SCOPE_ADVERTISEMENT") \
    X(BY_ID_SUBSCRIPTION,            "BY_ID_SUBSCRIPTION") \
    X(BY_TYPE_RESPONSE,              "BY_TYPE_RESPONSE") \
    X(BY_ID_REQUEST,                 "BY_ID_REQUEST") \
    X(BY_TYPE_REQUEST,               "BY_TYPE_REQUEST") \
    X(SUBSCRIPTION_NOTIFICATION,     "SUBSCRIPTION_NOTIFICATION") \
    X(SUBSCRIPTION_NOTIFICATION_DATA,"SUBSCRIPTION_NOTIFICATION_DATA") \
    X(TEMPORAL_QUERY_REQUEST,        "TEMPORAL_QUERY_REQUEST") \
    X(TEMPORAL_QUERY_RESPONSE,       "TEMPORAL_QUERY_RESPONSE")

#define SNDS_ACK_MESSAGE_TYPE_LIST \
    X(ACK_SUBSCRIPTION_NEW,            "ACK_SUBSCRIPTION_NEW") \
    X(ACK_SUBSCRIPTION_NOTIFICATION,   "ACK_SUBSCRIPTION_NOTIFICATION") \
    X(ACK_SUBSCRIPTION_NO_SUBSCRIBERS, "ACK_SUBSCRIPTION_NO_SUBSCRIBERS") \
    X(ACK_SCOPE_NOTIFICATION,          "ACK_SCOPE_NOTIFICATION") \
    X(ACK_ID_INTEREST,                 "ACK_ID_INTEREST") \
    X(NACK_ERROR,                      "NACK_ERROR")

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

    enum class SNDS_ACK_MESSAGE_TYPE
    {
        #define X(name, str) name,
            SNDS_ACK_MESSAGE_TYPE_LIST
        #undef X
    };

    static constexpr const char* SNDS_ACK_MESSAGE_TYPE_STRINGS[] = 
    {
        #define X(name, str) str,
            SNDS_ACK_MESSAGE_TYPE_LIST
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

    template<> struct enum_traits<SNDS_ACK_MESSAGE_TYPE> 
    {
        static constexpr const char* const* data() noexcept 
        { 
            return SNDS_ACK_MESSAGE_TYPE_STRINGS; 
        }
        static constexpr std::size_t size() noexcept 
        {
            return sizeof(SNDS_ACK_MESSAGE_TYPE_STRINGS) / sizeof(*SNDS_ACK_MESSAGE_TYPE_STRINGS);
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
        return from_string<SNDS_ACK_MESSAGE_TYPE>(tok).has_value();
    }

    /// @brief Represents an intra-service message with content, type, sender, and an optional name.
    struct SNDS_INTRA_MESSAGE {
    private:
        std::string message;                 ///< Message content (owned by the object).
        std::string name;                    ///< Optional name or identifier for the message.
        SNDS_INTRA_MESSAGE_TYPE type;        ///< Type of the message (e.g., GENERIC, BY_ID_RESPONSE).
        SNDS_CLIENT_TYPES sender_component; ///< Sender component responsible for this message.

    public:
        /**
         * @brief Constructs a new SNDS_INTRA_MESSAGE with specified content, type, sender, and name (rvalues only).
         * @param msg The content of the message (rvalue).
         * @param msgType The type of the message (e.g., GENERIC, BY_ID_RESPONSE).
         * @param sender The sender component of the message (e.g., PROXY, SERVICE).
         * @param msgName The name or identifier associated with the message (rvalue).
         */
        SNDS_INTRA_MESSAGE
        (
            std::string&& msg, 
            SNDS_INTRA_MESSAGE_TYPE msgType, 
            SNDS_CLIENT_TYPES sender, 
            std::string&& msgName
        ):message
        (
            std::move(msg)), 
            type(msgType), 
            sender_component(sender), 
            name(std::move(msgName)
        ) 
        {}

        /**
         * @brief Constructs a default SNDS_INTRA_MESSAGE with empty content and name,
         * a type of GENERIC, and sender set to PROXY.
         */
        SNDS_INTRA_MESSAGE(): message(""), type(SNDS_INTRA_MESSAGE_TYPE::GENERIC), sender_component(SNDS_CLIENT_TYPES::PROXY), name("") {}

        /**
         * @brief Sets the name or identifier for the message (rvalue only).
         * @param msg The new name or identifier (can be empty).
         */
        void setName(std::string&& msg) 
        {
            this->name = std::move(msg);
        }

        /**
         * @brief Sets the sender component for the message.
         * @param sender The sender component (e.g., PROXY, SERVICE).
         */
        void setSender(SNDS_CLIENT_TYPES sender) 
        {
            this->sender_component = sender;
        }

        // Deleted constructors and setters for lvalues (forces explicit move)
        SNDS_INTRA_MESSAGE(const std::string& msg, SNDS_INTRA_MESSAGE_TYPE msgType, SNDS_CLIENT_TYPES sender, const std::string& msgName) = delete;
        void setName(const std::string& msg) = delete;

        /**
         * @brief Gets the length of the message content.
         * @return The number of characters in the message content.
         */
        size_t getMessageLength() const 
        {
            return message.size();
        }

        /**
         * @brief Gets the length of the name or identifier.
         * @return The number of characters in the name.
         */
        size_t getNameLength() const 
        {
            return name.size();
        }

        /**
         * @brief Gets the message content of the class.
         * @return The reference to the string of the message class.
         */
        const std::string& getMessage() const 
        {
            return message;
        }

        /**
         * @brief Gets the name content of the class.
         * @return The reference to the string of the message class.
         */
        const std::string& getName() const 
        {
            return name;
        }

        /**
         * @brief Get the message type of the class.
         * @return Returns the enum value of the message type.
         */
        const SNDS_INTRA_MESSAGE_TYPE getMessageType() const 
        {
            return type;
        }

        /**
         * @brief Gets the sender component of the message.
         * @return The sender component (e.g., PROXY, SERVICE).
         */
        SNDS_CLIENT_TYPES getSender() const 
        {
            return sender_component;
        }

        /**
         * @brief Prints the current message and its details (for debugging),
         *        and also returns the same information as a string.
         */
        std::string printDetails() const
        {
            std::ostringstream oss;
            oss << "Message: " << message
                << "\nType: " << to_string(type)
                << "\nSender: " << to_string(sender_component)
                << "\nName: " << name
                << std::endl;

            // Convert the stream to a string.
            std::string details = oss.str();

            // Print to console.
            std::cout << details;

            // Return the string.
            return details;
        }


    };
};