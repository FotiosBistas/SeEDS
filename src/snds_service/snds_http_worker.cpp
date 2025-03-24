#include "snds_http_worker.h"

void HTTPWorker::init() {
    m_logger->info("Starting service: {}", serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Thread started.");

    while (running.load()) {
        m_logger->info("Reading packet...");

        auto message = v_to_service.get()->readMessage();
        if (!message) {
            m_logger->debug("Queue shutdown detected, exiting loop.");
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
            m_logger->warn("Subscription request with empty URL, treating as regular request.");
            isSub = false;
        }

        if (id.empty() && type.empty()) {
            m_logger->warn("Both ID and Type parameters are empty in GET request.");
            continue; // Skip processing this message
        }

        m_logger->debug("Extracted ID: {}", id);
        m_logger->debug("Extracted TYPE: {}", type);

        // Process according to sender type.
        switch (message->getSender()) {
            case SNDS_CLIENT_TYPES::HTTP_SRV: {
                m_logger->info("Received HTTP SERVER message.");
                // First, handle subscription requests.
                if (isSub && !id.empty()) {
                    m_logger->info("Processing subscription request...");
                    processSubscriptionRequest(id, subUrl);
                }
                // Next, handle regular getByID requests.
                if (!id.empty()) {
                    m_logger->info("Processing ID request...");
                    processByIDRequest(id);
                }
                // Otherwise, handle getByType requests.
                if (!type.empty()) {
                    m_logger->info("Processing TYPE request...");
                    processByTYPERequest(type);
                }
                break;
            }
            case SNDS_CLIENT_TYPES::CONSUMER:
                //Note here that all the responses have to do with the name
                m_logger->info("Received consumer message.");
                // First, handle registry file responses.
                if (name.find(m_registryFilePostfix) != std::string::npos) {
                    m_logger->info("Processing registry file response...");
                    processRegistryFileRequest(content, name);
                }
                // A request can be both byID and byTYPE
                // Next, handle getById responses.
                if (!id.empty()) {
                    m_logger->info("Processing getByIDConsumer request...");
                    processByIDRequest(name, true);
                }
                // Otherwise, handle getByType responses.
                if (!type.empty()) {
                    m_logger->info("Processing getByTYPEConsumer response...");
                    processByTYPERequest(name, true);
                }
                break;
            default:
                m_logger->info("Received unprocessable client type...");
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

        m_logger->info("Pushing getByID subscription interest for ID: {}", preparedID);

        // Build the interest name using the scope name and the prepared ID.
        std::string interestName = getSNDSNodeScopeNameForSubscription(preparedID, m_idScopePrefix, m_numberOfSndsSrvs) + preparedID;


        m_logger->info("Created interest name: {}", interestName);
        // Create the intra-SNDS message (using the proper constructor)
        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(interestName),
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::SERVICE,
            ""
        );

        m_logger->debug("Sending message to consumer...{}\n", message.get()->printDetails());
        v_to_consumer.get()->writeMessage(std::move(message));
    }
}

void HTTPWorker::processByIDRequest(const std::string& id, const bool subscription){

    if(subscription){
        processByIDRequestSub(id);
        return;
    }

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

void HTTPWorker::processByIDRequestSub(const std::string& id){
    m_logger->info("Subscription request received...");
    if(pendingGetByIDreqs.erase(id) > 0){
        m_logger->info("Got successfully data for previous requestByID. Removing state...");
        return; 
    }
}

void HTTPWorker::processByTYPERequest(const std::string& type, const bool subscription){

    if(subscription){
        processByTYPERequestSub(type);
        return;
    }

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

void HTTPWorker::processByTYPERequestSub(const std::string& type){
    //TODO yes here type is an ID
    // i'm not sure why
    m_logger->info("Subscription request received...");
    if(!pendingTypesForIDMap.contains(type)){
        m_logger->info("Type {} is not in pendingIDsToTypeMap", type);
        return; 
    }

    const auto& types = pendingTypesForIDMap.at(type);

    for(const auto& type: types){

        auto& ids = pendingIDsForTypeMap.at(type);
        //TODO continue to remove this type
    }
}

void HTTPWorker::processRegistryFileRequest(const std::string& payload, const std::string& name) {

    // Parse the payload into a vector of IDs.
    std::vector<std::string> ids = parseIDsFromRegistryFilePayload(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(payload.data()),
            payload.size()
        )
    );
    
    m_logger->info("Size of ids in registry file: {}", ids.size());
    
    // Create a set to hold all the IDs from the registry file.
    std::unordered_set<std::string> idSet;
    
    // Process each extracted ID.
    for (auto& id : ids) {

        ensureLeadingSlash(id);
        m_logger->debug("Processing registry id: {}", id);
        
        // Insert the id into the set.
        idSet.insert(id);
        
        // Update pendingIDsToTypesMap: for this id, add the type name.
        pendingIDsToTypeMap[id].insert(name);
        
        // Create a message for this id.
        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::string(id),  // Using the processed id. Note we need to copy here
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::SERVICE,
            std::string("")   // Empty message name.
        );
        
        m_logger->info("Pushing request to the consumer for id {}", id);
        v_to_consumer.get()->writeMessage(std::move(message));
    }
    
    // Update pendingIDsForTypeMap with the registry file name as key and the set of ids.
    pendingIDsForTypeMap[name] = std::move(idSet);
}