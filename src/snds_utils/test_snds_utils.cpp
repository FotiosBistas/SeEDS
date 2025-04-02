#include <iostream>
#include <iomanip> // for std::hex, std::setw, std::setfill

#include "snds_utils.h"

int main()
{
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

    // ========================
    // Test parsing function:
    // ========================
    // Construct a binary payload as a vector<uint8_t>:
    // Format: [header][number_of_ids][id_len][id_bytes]...
    // Header: 2 bytes (e.g., 0xAA, 0xBB), then:
    // Number of IDs: 2
    // ID 1: length 3, "car"
    // ID 2: length 3, "bus"

    try
    {
        std::vector<uint8_t> testPayload = {
            0xAA, 0xBB,          // Header (ignored)
            0x02,                // Number of IDs
            0x03, 'c', 'a', 'r', // First ID ("car")
            0x03, 'b', 'u', 's'  // Second ID ("bus")
        };
        auto extractedIDs = parseIDsFromRegistryFilePayload(std::span<const uint8_t>(testPayload));

        std::cout << "Extracted IDs from payload:" << std::endl;
        for (const auto &id : extractedIDs)
        {
            std::cout << "- " << id << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing payload: " << e.what() << std::endl;
    }

    std::cout << std::endl;

    // ================================
    // Test serialization function:
    // ================================
    try
    {
        // Create a test set of IDs.
        std::unordered_set<std::string> idSet = {"car", "bus", "bike"};

        // Serialize the IDs into a binary payload.
        std::vector<std::byte> serializedPayload = serializeRegistryFilePayload(idSet);

        // Print the complete payload in hexadecimal.
        std::cout << "Serialized payload (" << serializedPayload.size() << " bytes):" << std::endl;
        for (size_t i = 0; i < serializedPayload.size(); ++i)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << std::to_integer<int>(serializedPayload[i]) << " ";
        }
        std::cout << std::dec << std::endl
                  << std::endl; // Reset to decimal

        // Re-parse the serialized payload to confirm correctness.
        size_t offset = 0;
        uint8_t numIDs = std::to_integer<uint8_t>(serializedPayload[offset]);
        offset++;
        std::cout << "Number of IDs: " << static_cast<int>(numIDs) << std::endl;
        for (uint8_t i = 0; i < numIDs; ++i)
        {
            uint8_t idLength = std::to_integer<uint8_t>(serializedPayload[offset]);
            offset++;
            std::string id(reinterpret_cast<const char *>(serializedPayload.data() + offset), idLength);
            offset += idLength;
            std::cout << "ID " << static_cast<int>(i + 1) << ": " << id << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception during serialization/parsing: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
