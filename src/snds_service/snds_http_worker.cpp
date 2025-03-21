#include "snds_http_worker.h"

void HTTPWorker::init() {
    m_logger->info("HTTPWorker: Starting service: {}", serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("HTTPWorker thread started.");

    while (running.load()) {
        m_logger->info("HTTPWorker: Reading packet...");

        auto message = v_to_service.get()->readMessage();
        if (!message) {
            m_logger->debug("HTTPWorker: Queue shutdown detected, exiting loop.");
            break;
        }

        // Retrieve the message content and extract parameters using snds_utils.
        const std::string content = message->getMessage();
        const std::string name    = message->getName();
        const std::string id      = getFromGETRequest(content, PARAM_ID_TAG);
        const std::string type    = getFromGETRequest(content, PARAM_TYPE_TAG);
        std::string subUrl        = getURLfromSubscription(content);
        bool isSub                = isSubscriptionRequest(content);

        // If subscription flag is set but no subUrl provided, log a warning and treat as a regular request.
        if (isSub && subUrl.empty()) {
            m_logger->warn("HTTPWorker: Subscription request with empty URL, treating as regular request.");
            isSub = false;
        }

        if (id.empty() && type.empty()) {
            m_logger->warn("HTTPWorker: Both ID and Type parameters are empty in GET request.");
            continue; // Skip processing this message
        }

        m_logger->debug("Extracted ID: {}", id);
        m_logger->debug("Extracted TYPE: {}", type);

        // Process according to sender type.
        switch (message->getSender()) {
            case SNDS_CLIENT_TYPES::HTTP_SRV: {
                m_logger->info("HTTPWorker: Received HTTP SERVER message.");
                // First, handle subscription requests.
                if (isSub && !id.empty()) {
                    m_logger->info("HTTPWorker: Processing subscription request...");
                    processSubscriptionRequest(id, subUrl);
                }
                // Next, handle regular getByID requests.
                else if (!id.empty()) {
                    m_logger->info("HTTPWorker: Processing ID request...");
                    processByIDRequest(id);
                }
                // Otherwise, handle getByType requests.
                else if (!type.empty()) {
                    m_logger->info("HTTPWorker: Processing TYPE request...");
                    processByTYPERequest(type);
                }
                break;
            }
            case SNDS_CLIENT_TYPES::CONSUMER:
                m_logger->info("HTTPWorker: Received consumer message.");
                // First, handle registry file responses.
                if (name.find(m_registryFilePostfix) != std::string::npos) {
                    m_logger->info("HTTPWorker: Processing registry file response...");
                    //processRegistryFileResponse(id, content);
                }
                // Next, handle getById responses.
                else if (!id.empty()) {
                    m_logger->info("HTTPWorker: Processing getByID response...");
                    //processByIDResponse(id);
                }
                // Otherwise, handle getByType responses.
                else if (!type.empty()) {
                    m_logger->info("HTTPWorker: Processing getByTYPE response...");
                    //processByTYPEResponse(type);
                }
                break;
            default:
                m_logger->info("HTTPWorker: Received unprocessable client type...");
                break;
        }
    }

    m_logger->info("HTTPWorker thread stopping.");
}

void HTTPWorker::processSubscriptionRequest(const std::string& id, const std::string& subURL) {
    // Retrieve the subscription set for this ID.
    // If 'id' is new, operator[] creates an empty set.
    auto& subs = subscribedBYID_users[id];

    // Record whether this is the first subscription for the ID.
    bool wasEmpty = subs.empty();

    // Insert the new subscription URL into the set.
    bool isNewSubscription = subs.insert(subURL).second;

    if (isNewSubscription) {
        m_logger->info("Stored subscription for ID: {}", id);
    } else {
        m_logger->info("Duplicate subscription for ID: {} ignored.", id);
    }

    // Only send the subscription interest if this is the first subscription for the ID.
    if (wasEmpty && isNewSubscription) {
        // Prepare the ID (ensure it starts with '/')
        std::string preparedID = id; // Copy for modification
        ensureLeadingSlash(preparedID);

        // Build the interest name using the scope name and the prepared ID.
        std::string interestName = getSNDSNodeScopeNameForSubscription(preparedID, m_idScopePrefix, m_numberOfSndsSrvs) + preparedID;

        m_logger->info("Pushing getByID subscription interest for ID: {}", preparedID);

        // Create the intra-SNDS message (using the proper constructor)
        auto msg = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(interestName),
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::SERVICE,
            ""
        );

        v_to_consumer.get()->writeMessage(std::move(msg));
    }
}

void HTTPWorker::processByIDRequest(const std::string& id){
    auto [_, newRequest] = pendingGetByIDreqs.insert(id);
    if(!newRequest){
        m_logger->info("Duplicate getByID request for ID: {} ignored.", id);
        return;
    }

    std::string modifiableId = id;  // Create a modifiable copy
    ensureLeadingSlash(modifiableId);
    m_logger->info("Pushing regular getByID request for ID: {}", modifiableId);

    auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
        std::move(modifiableId),
        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
        SNDS_CLIENT_TYPES::SERVICE,
        ""
    );

    v_to_consumer.get()->writeMessage(std::move(message));
}

void HTTPWorker::processByTYPERequest(const std::string& type){
    auto [_, newRequest] = pendingGetByTYPEreqs.insert(type);

    if (!newRequest){
        m_logger->info("Duplicate getByType request for type: {} ignored.", type);
        return;
    }

    std::string fullType = type + m_registryFilePostfix;
    ensureLeadingSlash(fullType);
    m_logger->info("Pushing getByType request for type: {}", fullType);

    auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
        std::move(fullType),
        SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_RESPONSE,
        SNDS_CLIENT_TYPES::SERVICE,
        ""
    );

    v_to_consumer.get()->writeMessage(std::move(message));
}

std::vector<std::string> HTTPWorker::parseIDsFromRegistryFilePayload(std::span<const uint8_t> payload) {
    //Points to the place where the header ends
    constexpr size_t header_size = 2; 
    if (payload.size() <= header_size) {
        throw std::invalid_argument("Payload too short to contain header.");
    }

    size_t offset = header_size;
    // TODO 2^8 maximum IDS good idea?
    // Use uint8_t here to ensure you can read only 1 byte
    const uint8_t number_of_ids = payload[offset++];
    std::vector<std::string> ids;
    // Preallocate space in the vector
    ids.reserve(number_of_ids);

    for(uint8_t i=0; i<number_of_ids; i++){
        if (offset >= payload.size()) {
            throw std::out_of_range("Payload truncated before reading ID size.");
        }
        // Request should contain the size of the ID after
        // For example if the size is 3 we should read 3 bytes
        const uint8_t size_of_id = payload[offset++];

        if(offset + size_of_id > payload.size()){
            throw::std::out_of_range("Payload truncated before reading entire ID.");
        }
        // Parse the payload from the index you are currently at 
        // Until the size of the id
        std::string_view id(reinterpret_cast<const char*>(payload.data()+offset), size_of_id);
        // Advance the index
        offset = offset + size_of_id;
        // Push back
        ids.emplace_back(id);
    }
}

std::vector<std::byte> HTTPWorker::serializeRegistryFilePayload(const std::unordered_set<std::string>& ids){

    //TODO huge limitation on how many IDs you can have @yannisT
    if(ids.size() > 255){
        throw std::length_error("Limited description size for the ID set provided.");
    }

    size_t offset = 0;
    // We are about to create the payload
    // so we need to know the size of the byte buffer
    size_t payload_length = 1;
    for(const auto& id: ids){
        if (id.size() > 255){
            throw std::length_error("ID exceeds the size that can be described by 1 byte (2^8).");
        }
        // Allocate size for describing the length of the ID and the ID itself
        payload_length += 1 + id.size();
    }

    // Allocate needed memory for our payload
    std::vector<std::byte> payload(payload_length);
    // First byte should be allocated to hold the number_of_ids
    payload[offset++] = static_cast<std::byte>(ids.size());

    for(const auto& id: ids){
        payload[offset++] = static_cast<std::byte>(id.size());
        std::memcpy(&payload, id.data(), id.size());
        offset += id.size();
    }

    return payload;
}