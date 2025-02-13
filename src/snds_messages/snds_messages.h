#include <string>
#include <iostream>
#include <cstddef> 

/// @brief Defines the type of intra-service messages.
enum class SNDS_INTRA_MESSAGE_TYPE {
    GENERIC,           ///< Generic message type, used for general-purpose communication.
    BY_ID_RESPONSE,    ///< Message is a response identified by a specific ID.
    BY_TYPE_RESPONSE   ///< Message is a response identified by a specific type.
};

/// @brief Defines the different components or clients in the SNDS system. Allegendly correctly explained by me
enum class SNDS_CLIENT_TYPES {
    PROXY,            ///< Proxy component in the service.
    PRODUCER,         ///< Producer component responsible for publishing data.
    CONSUMER,         ///< Consumer component that fetches or processes data.
    DIGITAL_TWIN,     ///< Digital twin component for managing digital replicas of entities.
    SERVICE,          ///< Service component for inter-service communication and orchestration.
    HTTP_SRV,         ///< HTTP server component for handling external requests.
    ON_INTEREST       ///< Component triggered on receiving an interest (NDN-specific).
};

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
    SNDS_INTRA_MESSAGE(std::string&& msg, 
                       SNDS_INTRA_MESSAGE_TYPE msgType, 
                       SNDS_CLIENT_TYPES sender, 
                       std::string&& msgName)
        : message(std::move(msg)), type(msgType), sender_component(sender), name(std::move(msgName)) {}

    /**
     * @brief Constructs a default SNDS_INTRA_MESSAGE with empty content and name,
     * a type of GENERIC, and sender set to PROXY.
     */
    SNDS_INTRA_MESSAGE() 
        : message(""), type(SNDS_INTRA_MESSAGE_TYPE::GENERIC), sender_component(SNDS_CLIENT_TYPES::PROXY), name("") {}

    /**
     * @brief Sets the name or identifier for the message (rvalue only).
     * @param msg The new name or identifier (can be empty).
     */
    void setName(std::string&& msg) {
        this->name = std::move(msg);
    }

    /**
     * @brief Sets the sender component for the message.
     * @param sender The sender component (e.g., PROXY, SERVICE).
     */
    void setSender(SNDS_CLIENT_TYPES sender) {
        this->sender_component = sender;
    }

    // Deleted constructors and setters for lvalues (forces explicit move)
    SNDS_INTRA_MESSAGE(const std::string& msg, SNDS_INTRA_MESSAGE_TYPE msgType, SNDS_CLIENT_TYPES sender, const std::string& msgName) = delete;
    void setName(const std::string& msg) = delete;

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

    /**
     * @brief Gets the sender component of the message.
     * @return The sender component (e.g., PROXY, SERVICE).
     */
    SNDS_CLIENT_TYPES getSender() const {
        return sender_component;
    }

    /**
     * @brief Prints the current message and its details (for debugging).
     */
    void printDetails() const {
        std::cout << "Message: " << message
                  << "\nType: " << static_cast<int>(type)
                  << "\nSender: " << static_cast<int>(sender_component)
                  << "\nName: " << name
                  << std::endl;
    }
};
