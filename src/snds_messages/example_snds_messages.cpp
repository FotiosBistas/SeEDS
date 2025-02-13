#include <iostream>
#include <string>
#include "snds_messages.h"

int main() {
    // Example 1: Creating a message using rvalues
    std::cout << "Example 1: Creating a message using rvalues" << std::endl;
    SNDS_INTRA_MESSAGE msg1(std::string("Hello, World!"), 
                            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE, 
                            SNDS_CLIENT_TYPES::SERVICE, 
                            std::string("TestName"));
    msg1.printDetails();

    // Example 2: Updating the name using an rvalue
    std::cout << "\nExample 2: Updating the name using an rvalue" << std::endl;
    msg1.setName(std::string("UpdatedName"));
    msg1.printDetails();

    // Example 3: Explicitly moving lvalues into the constructor
    std::cout << "\nExample 3: Explicitly moving lvalues into the constructor" << std::endl;
    std::string messageContent = "LvalueMessage";
    std::string messageName = "LvalueName";
    SNDS_INTRA_MESSAGE msg2(std::move(messageContent), 
                            SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_RESPONSE, 
                            SNDS_CLIENT_TYPES::CONSUMER, 
                            std::move(messageName));
    msg2.printDetails();

    // Example 4: Attempting to use lvalues without std::move (will cause a compile-time error)
    // Uncommenting the following lines will result in compilation errors:
    // SNDS_INTRA_MESSAGE msg3(messageContent, SNDS_INTRA_MESSAGE_TYPE::GENERIC, SNDS_CLIENT_TYPES::PRODUCER, messageName);
    // msg2.setName(messageName);

    // Example 5: Creating a default message
    std::cout << "\nExample 5: Creating a default message" << std::endl;
    SNDS_INTRA_MESSAGE defaultMsg;
    defaultMsg.printDetails();

    // Example 6: Updating the name of the default message
    std::cout << "\nExample 6: Updating the name of the default message" << std::endl;
    defaultMsg.setName(std::string("DefaultMessageName"));
    defaultMsg.printDetails();

    // Example 7: Updating the sender component of a message
    std::cout << "\nExample 7: Updating the sender component of a message" << std::endl;
    defaultMsg.setSender(SNDS_CLIENT_TYPES::DIGITAL_TWIN);
    std::cout << "Updated Sender Component: " << static_cast<int>(defaultMsg.getSender()) << std::endl;
    defaultMsg.printDetails();

    return 0;
}
