#include <iostream>
#include "snds_utils.h"

int main() {
    // ---------------------------------------------------
    // Test getSNDSNodeForSubscription:
    // ---------------------------------------------------
    std::string testName = "example";
    int numServices = 5;
    unsigned nodeId = getSNDSNodeForSubscription(testName, numServices);
    std::cout << "Node for subscription '" << testName << "': " << nodeId << std::endl;

    // ---------------------------------------------------
    // Test getFromGETRequest:
    // ---------------------------------------------------
    std::string getRequest = "/tag=value1&another=val2";
    std::string valueForTag = getFromGETRequest(getRequest, "tag");
    std::cout << "Value for 'tag': " << valueForTag << std::endl;

    std::string valueForAnother = getFromGETRequest(getRequest, "another");
    std::cout << "Value for 'another': " << valueForAnother << std::endl;

    std::string missingValue = getFromGETRequest(getRequest, "missing");
    std::cout << "Value for 'missing': " << missingValue << std::endl;

    // ---------------------------------------------------
    // Test ensureLeadingSlash:
    // ---------------------------------------------------
    std::string nameWithoutSlash = "testName";
    ensureLeadingSlash(nameWithoutSlash);
    std::cout << "After ensureLeadingSlash (was 'testName'): " << nameWithoutSlash << std::endl;

    std::string alreadySlashed = "/alreadySlashed";
    ensureLeadingSlash(alreadySlashed);
    std::cout << "After ensureLeadingSlash (was '/alreadySlashed'): " << alreadySlashed << std::endl;

    // ---------------------------------------------------
    // Test getSNDSNodeScopeNameForSubscription:
    // ---------------------------------------------------
    // Assuming ID_SCOPE_PREFIX is defined somewhere (or pass a prefix explicitly)
    std::string id = "exampleID";
    std::string prefix = "/SNDS_ID_scope_";
    std::string scopeName = getSNDSNodeScopeNameForSubscription(id, prefix, numServices);
    std::cout << "SNDS Node Scope Name for id 'exampleID': " << scopeName << std::endl;

    // ---------------------------------------------------
    // Test inline functions: isSubscriptionRequest and getURLfromSubscription
    // ---------------------------------------------------
    // Create a GET request string that includes subscription parameters.
    std::string subRequest = "/isSub=true&suburl=http://example.com";
    bool isSub = isSubscriptionRequest(subRequest);
    std::cout << "Is subscription request: " << std::boolalpha << isSub << std::endl;

    std::string subURL = getURLfromSubscription(subRequest);
    std::cout << "Subscription URL: " << subURL << std::endl;

    return 0;
}
