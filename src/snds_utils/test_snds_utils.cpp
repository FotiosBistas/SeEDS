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
    // Test get_from_get_request:
    // ---------------------------------------------------
    std::string getRequest = "/tag=value1&another=val2";
    std::string_view valueForTag = get_from_get_request(getRequest, "tag");
    std::cout << "Value for 'tag': " << valueForTag << std::endl;

    std::string_view valueForAnother = get_from_get_request(getRequest, "another");
    std::cout << "Value for 'another': " << valueForAnother << std::endl;

    std::string_view missingValue = get_from_get_request(getRequest, "missing");
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
    // Test inline functions: is_subscription_request and get_url_of_subscription
    // ---------------------------------------------------
    // Create a GET request string that includes subscription parameters.
    std::string subRequest = "/isSub=true&suburl=http://example.com";
    bool isSub = is_subscription_request(subRequest, "isSub");
    std::cout << "Is subscription request: " << std::boolalpha << isSub << std::endl;

    std::string_view subURL = get_url_of_subscription(subRequest, "subUrl");
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
            0x00, 0x02,               // Number of IDs = 2 (big-endian)
            0x00, 0x03, 'c', 'a', 'r',// First ID length = 3, then "car"
            0x00, 0x03, 'b', 'u', 's' // Second ID length = 3, then "bus"
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
        // --- read two bytes for number_of_ids (big-endian) ---
        uint16_t numIDs = (static_cast<uint16_t>(std::to_integer<uint8_t>(serializedPayload[offset])) << 8)
                        |  static_cast<uint16_t>(std::to_integer<uint8_t>(serializedPayload[offset + 1]));
        offset += 2;
        std::cout << "Number of IDs: " << static_cast<int>(numIDs) << std::endl;
        for (uint16_t i = 0; i < numIDs; ++i)
        {
            // --- Read two bytes for the next ID length ---
            uint16_t idLength = (static_cast<uint16_t>(std::to_integer<uint8_t>(serializedPayload[offset])) << 8)
                              |  static_cast<uint16_t>(std::to_integer<uint8_t>(serializedPayload[offset + 1]));
            offset += 2;
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
