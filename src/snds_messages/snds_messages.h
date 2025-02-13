#include <string>
#include <iostream>
#include <cstddef> 

/// @brief Defines the type of intra-service messages.
enum class SNDS_INTRA_MESSAGE_TYPE {
    GENERIC,           ///< Generic message type.
    BY_ID_RESPONSE,    ///< Message is a response identified by ID.
    BY_TYPE_RESPONSE   ///< Message is a response identified by type.
};

/// @brief Represents an intra-service message with content, type, and an optional name.
struct SNDS_INTRA_MESSAGE {
private:
    const std::string message; ///< Immutable message content.
    std::string name;          ///< Optional name or identifier for the message.
    SNDS_INTRA_MESSAGE_TYPE type; ///< Type of the message.

public:
    /**
     * @brief Constructs a new SNDS_INTRA_MESSAGE with specified content, type, and name.
     * @param msg The content of the message.
     * @param msgType The type of the message (e.g., GENERIC, BY_ID_RESPONSE).
     * @param msgName The name or identifier associated with the message.
     */
    SNDS_INTRA_MESSAGE(const std::string& msg, 
                       SNDS_INTRA_MESSAGE_TYPE msgType, 
                       const std::string& msgName)
        : message(msg), type(msgType), name(msgName) {}

    /**
     * @brief Constructs a default SNDS_INTRA_MESSAGE with empty content and name,
     * and a type of GENERIC.
     */
    SNDS_INTRA_MESSAGE() 
        : message(""), type(SNDS_INTRA_MESSAGE_TYPE::GENERIC), name("") {}

    /**
     * @brief Sets the name or identifier for the message.
     * @param msg The new name or identifier (can be empty).
     */
    void setName(std::string&& msg) {
        this->name = std::move(msg); 
    }

    /**
     * @brief Gets the length of the message content.
     * @return The number of characters in the message content.
     */
    size_t getMessageLength() const {
        return message.size();
    }

    /**
     * @brief Gets the length of the name or identifier.
     * @return The number of characters in the name.
     */
    size_t getNameLength() const {
        return name.size();
    }
};
