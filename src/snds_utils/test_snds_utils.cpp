#include <iostream>
#include <iomanip> // for std::hex, std::setw, std::setfill
#include <arpa/inet.h> // for ntohs()
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
    // Format: [count (2 bytes)][id1_length (2 bytes)][id1 bytes…][id2_length (2 bytes)][id2 bytes…]
    // - count: 2 bytes (big-endian) = 2
    // - ID 1: length 2-byte (big-endian) = 3, then "car"
    // - ID 2: length 2-byte (big-endian) = 3, then "bus"
    //
    // In hex, the payload is:
    //   00 02       ← count = 2
    //   00 03 63 61 72  ← length=3, "car"
    //   00 03 62 75 73  ← length=3, "bus"

    try
    {
        
        std::vector<uint8_t> testPayload = {
            0x00, 0x02,             // count = 2 (big-endian)
            0x00, 0x03, 'c','a','r',// first ID ("car")
            0x00, 0x03, 'b','u','s' // second ID ("bus")
        };
        auto extractedIDs = parseIDsFromRegistryFilePayload(std::span<const uint8_t>(testPayload), nullptr);

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

        // Re-parse the serialized payload by reading 2-byte count/lengths,
        // to confirm correctness.
        const uint8_t *data = reinterpret_cast<const uint8_t *>(serializedPayload.data());
        size_t dataLen = serializedPayload.size();
        size_t offset = 0;

        if (dataLen < 2) {
            std::cerr << "Payload too short to read count\n";
            return 1;
        }

        // Read first 2 bytes as big-endian count
        uint16_t netCount;
        std::memcpy(&netCount, data + offset, 2);
        uint16_t numIDs = ntohs(netCount);
        offset += 2;
        std::cout << "Number of IDs: " << numIDs << std::endl;

        for (uint16_t i = 0; i < numIDs; ++i)
        {
            if (offset + 2 > dataLen) {
                std::cerr << "Payload truncated before reading ID length\n";
                return 1;
            }
            uint16_t netLen;
            std::memcpy(&netLen, data + offset, 2);
            uint16_t idLength = ntohs(netLen);
            offset += 2;

            if (offset + idLength > dataLen) {
                std::cerr << "Payload truncated before reading ID bytes\n";
                return 1;
            }

            std::string idStr(reinterpret_cast<const char *>(data + offset), idLength);
            offset += idLength;
            std::cout << "ID " << (i + 1) << ": " << idStr << std::endl;
        }

    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception during serialization/parsing: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
