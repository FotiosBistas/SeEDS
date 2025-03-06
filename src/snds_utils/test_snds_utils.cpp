// src/snds_utils/test_snds_utils.cpp
#include <iostream>
#include "snds_utils.h"

int main() {
    // Test getSNDSNodeForSubscription:
    std::string testName = "example";
    int numServices = 5;
    unsigned nodeId = getSNDSNodeForSubscription(testName, numServices);
    std::cout << "Node for subscription '" << testName << "': " << nodeId << std::endl;

    // Test getFromGETRequest:
    std::string getRequest = "/tag=value1&another=val2";
    std::string tag = getFromGETRequest(getRequest, "tag");
    std::cout << "Value for 'tag': " << tag << std::endl;

    std::string another = getFromGETRequest(getRequest, "another");
    std::cout << "Value for 'another': " << another << std::endl;

    // Also test a case where the tag is not found:
    std::string missing = getFromGETRequest(getRequest, "missing");
    std::cout << "Value for 'missing': " << missing << std::endl;

    return 0;
}