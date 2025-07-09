#include "worker.h"

void Worker::init() {
    m_logger->info("Starting service: {}", serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Thread started.");

    while (running.load()) {
        m_logger->info("Reading packet...");

        auto message = v_to_worker.get()->readMessage();
        if (!message) {
            m_logger->debug("Queue shutdown detected, exiting loop.");
            break;
        }

        // Retrieve the message content and extract parameters using snds_utils.
        std::string content            = message->getMessage();
        m_logger->info("Received content: {}", content);
        std::string name               = message->getName();
        m_logger->info("Received name: {}", name);
        std::string id                 = std::string(get_from_get_request(content, GET_PARAMETER_ID_TAG));
        std::string type               = std::string(get_from_get_request(content, GET_PARAMETER_TYPE_TAG));
        std::string count              = std::string(get_from_get_request(content, GET_PARAMETER_COUNT_TAG));
        std::string date               = url_decode(get_from_get_request(content, GET_PARAMETER_DATE_TAG));
        std::string timeframe          = url_decode(get_from_get_request(content, GET_PARAMETER_TIMEFRAME_TAG));
        std::string fields_csv         = url_decode(get_from_get_request(content, GET_PARAMETER_FIELDS_TAG));
        std::string parent_query_hash  = url_decode(get_from_get_request(content, GET_PARAMETER_PARENT_QUERY_HASH_TAG));
        std::string subUrl             = std::string(getURLfromSubscription(content));
        bool isSub                     = isSubscriptionRequest(content);

        // If subscription flag is set but no subUrl provided, log a warning and treat as a regular request.
        if (isSub && subUrl.empty()) {
            m_logger->warn("Subscription request with empty URL, treating as regular request.");
            isSub = false;
        }

        if (id.empty() && type.empty()) {
            m_logger->warn("Both ID and Type parameters are empty in GET request. Hopefully name is filled...");
            // currently this is a message only sent by the Consumer
            // received here. It is essentially the data we got from the NDN
            // sends back the ID to remove it from pending IDs if its not a temporal query
            id                 = get_from_get_request(name, GET_PARAMETER_ID_TAG);
            type               = get_from_get_request(name, GET_PARAMETER_TYPE_TAG);
            parent_query_hash  = get_from_get_request(name, GET_PARAMETER_PARENT_QUERY_HASH_TAG);
            date               = get_from_get_request(name, GET_PARAMETER_DATE_TAG);
            //continue; // Skip processing this message
        }

        m_logger->debug("Extracted ID: {}", id);
        m_logger->debug("Extracted TYPE: {}", type);
        m_logger->debug("Extracted subURL: {}", subUrl);
        m_logger->debug("Extracted isSub: {}", isSub);
        m_logger->debug("Temporal query related data");
        m_logger->debug("Extracted Date: {}", date);
        m_logger->debug("Extracted Timeframe: {}", timeframe);
        m_logger->debug("Extracted Count: {}", count);
        m_logger->debug("Fields: {}", fields_csv);
        m_logger->debug("Parent Query Hash: {}", parent_query_hash);

        //TODO we can differentiate messages based on type
        //I don't know why are we doing this
        // Process according to sender type.
        switch (message->getSender()) {
            case SNDS_CLIENT_TYPES::HTTP_SRV: {
                m_logger->info("Received HTTP SERVER message.");
                // Give priority to temporal queries
                if(!timeframe.empty()){
                    m_logger->info("Processing temporal query request...");
                    TemporalQuery query = 
                    {
                        string_to_timeframe(timeframe), 
                        date,
                        id,
                        type,
                        count,
                    };
                    query.filters = parse_filters(fields_csv);
                    processTemporalQueryRequest(query);
                    break;
                }
                // Handle subscription requests.
                if (isSub && !id.empty()) {
                    m_logger->info("Processing subscription request...");
                    processSubscriptionRequest(id.data(), subUrl.data());
                }
                // Next, handle regular getByID requests.
                if (!id.empty()) {
                    m_logger->info("Processing ID request...");
                    processByIDRequest(id.data());
                }
                // Otherwise, handle getByType requests.
                if (!type.empty()) {
                    m_logger->info("Processing TYPE request...");
                    processByTYPERequest(type.data());
                }
                break;
            }
            case SNDS_CLIENT_TYPES::CONSUMER:
                //Note here that all the responses have to do with the name
                m_logger->info("Received consumer message.");

                if(!date.empty()){
                    m_logger->info("Processing temporal query response...");
                    processTemporalQueryResponse(parent_query_hash, content);
                    break;
                }

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

    m_logger->info("Worker thread stopping.");
}


void Worker::processTemporalQueryRequest(const TemporalQuery& temporal_query){

    auto query_hash = temporal_query_hash_string(temporal_query);
    auto [_, inserted] = pending_temporal_queries.emplace(query_hash, temporal_query);
    if (!inserted) 
    {
        m_logger->info("Duplicate TemporalQuery {} for ID: {}", serialize_temporal_query(temporal_query), temporal_query.id);
        return;
    }

    m_logger->info("Got new temporal query with hash: {}", temporal_query_hash_string(temporal_query));
    auto id = temporal_query.id;

    ensureLeadingSlash(id);
    m_logger->info("Pushing TemporalQuery {} for ID: {}", serialize_temporal_query(temporal_query), id);

    auto message = std::make_unique<SNDS_INTRA_MESSAGE>
    (
        std::move(serialize_temporal_query(temporal_query)),
        SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST,
        SNDS_CLIENT_TYPES::SERVICE,
        ""
    );

    v_to_consumer.get()->writeMessage(std::move(message));
}

void Worker::processTemporalQueryResponse(const std::string& parent_query_hash, const std::string& content){

    auto it = pending_temporal_queries.find(parent_query_hash);
    if (it == pending_temporal_queries.end()) {
        m_logger->error("No pending query for hash {}", parent_query_hash);
        return;
    }

    auto parent_query = it->second;
    //weird thing where the data received have some weird characters in front
    //    �q[
    //{
    //"before": [
    //  {
    //    "_id": {
    //      "_id": "car2",
    //      "timestamp": {
    //        "$date": 1750835111321
    //      }
    //    }
    //  }
    //]

    auto pos = content.find_first_of("[{");
    std::string payload = content;
    if (pos != std::string::npos) {
        payload = content.substr(pos);
        m_logger->debug("Removed extra characters from payload:\n{}", payload);
    }

    m_logger->info("Found stored temporal query: {}", serialize_temporal_query(parent_query));

    if(!parent_query.filters.empty()){
        m_logger->info("Applying filters to the data received");

        nlohmann::json document = nlohmann::json::parse(payload);

        std::unordered_set<std::string> keep;
        for (auto const& f : parent_query.filters) {
            keep.insert(f.key);
        }

        nlohmann::json filtered = nlohmann::json::object();
        for (auto const& key : keep) {
            if (document.contains(key)) {
                filtered[key] = document[key];
            }
        }

        payload = filtered.dump();
    }

    m_logger->debug("Sending data to HTTP server: {}", payload);

    auto message = std::make_unique<SNDS_INTRA_MESSAGE>
    (
        std::move(payload),
        SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_RESPONSE,
        SNDS_CLIENT_TYPES::WORKER,
        ""
    );

    v_to_service->writeMessage(std::move(message));
}

void Worker::processSubscriptionRequest(const std::string& id, const std::string& subURL) {
    // Retrieve the subscription set for this ID.
    // If 'id' is new, operator[] creates an empty set.
    auto& subs = subscribedBYID_users[id];

    // Record whether this is the first subscription for the ID.
    bool wasEmpty = subs.empty();

    // Insert the new subscription URL into the set.
    bool isNewSubscription = subs.insert(subURL).second;

    if (isNewSubscription) {
        m_logger->info("Stored new subscription for ID: {0} and URL {1}", id, subURL);
    } else {
        m_logger->info("Duplicate subscription for ID: {0} and URL {1}. Ignored...", id, subURL);
    }

    // Only send the subscription interest if this is the first subscription for the ID.
    if (wasEmpty && isNewSubscription) {
        // Prepare the ID (ensure it starts with '/')
        std::string preparedID = id; // Copy for modification
        ensureLeadingSlash(preparedID);

        m_logger->info("Pushing getByID subscription interest for ID: {}", preparedID);

        // Build the interest name using the scope name and the prepared ID.
        // NOTE: you need to append you own SNDS ID that you want to subscribe to
        std::string interestName = getSNDSNodeScopeNameForSubscription(preparedID, m_idScopePrefix, m_numberOfSndsSrvs) + preparedID + m_idScopePrefix + m_mySndsId;


        m_logger->info("Created interest name: {}", interestName);
        // Create the intra-SNDS message (using the proper constructor)
        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(interestName),
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::SERVICE,
            ""
        );

        m_logger->debug("Sending message to consumer...\n{}", message.get()->printDetails());
        v_to_consumer.get()->writeMessage(std::move(message));
    }
}

void Worker::processByIDRequest(const std::string& id, const bool subscription){

    if(subscription){
        processByIDResponse(id);
        return;
    }

    auto [_, newRequest] = pendingGetByIDreqs.insert(id);
    if(!newRequest){
        m_logger->info("Duplicate getByID request for ID: {} redoing operation.", id);
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

void Worker::processByIDResponse(const std::string& name){
    m_logger->info("Subscription response with name {} received...", name);
    auto temp = std::string(name);
    removeIdPrefix(temp);
    m_logger->debug("Checking if PendingGetByIDreqs contains {}...", temp);

    if(pendingGetByIDreqs.erase(temp) > 0){
        m_logger->info("Got successfull data for previous requestByID. Removing state...");
        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            "erase=id=" + std::string(temp),
            SNDS_INTRA_MESSAGE_TYPE::GENERIC,
            SNDS_CLIENT_TYPES::SERVICE,
            ""
        );

        v_to_dt->writeMessage(std::move(message));
        return; 
    }
}

void Worker::processByTYPERequest(const std::string& type, const bool subscription){

    if(subscription){
        processByTYPEResponse(type);
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

void Worker::processByTYPEResponse(const std::string& name){
    //TODO yes here type is an ID
    // in the end I guess not
    // i'm not sure why
    m_logger->info("Subscription request received for name {}...", name);
    auto temp = std::string(name);
    //removeTypePrefix(temp);
    //ensureLeadingSlash(temp);
    if(!pendingIDsForTypeMap.contains(name)){
        m_logger->info("Name {} is not in pendingIDsForTypeMap", name);
        return; 
    }
    m_logger->info("Name {} is in pendingIDsForTypeMap", name);
    // This gets us the types for an ID
    auto& ids = pendingIDsForTypeMap.at(name);
    std::vector<std::string> idsToErase;


    for(const auto& id: ids){

        auto& types = pendingTypesForIDMap.at(id);
        if(types.erase(name) < 0){
            m_logger->info("Could not find name {0} for id {1}", name, id);
            //TODO I guess this was originally used for debugging purposes
            m_logger->error("State inconsistency...");
            return;
        }

        if(types.empty()){
            m_logger->info("Successfully removed name {0} from the pendingTypesforID for ID {1}", name, id);
            // There are no more pending types for the id
            // Can safely erase without check since we got the ids
            pendingTypesForIDMap.erase(id);
            // Mark this type to remove from ID
            idsToErase.push_back(id);      

        }
    }

    // Now remove the empty types from the set for this ID
    for (const auto& id: idsToErase) {
        m_logger->debug("Removing ID {0} from pendingIDsForTypeMap for name {1}", id, name);
        ids.erase(id);

        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            "erase=id=" + std::string(id),
            SNDS_INTRA_MESSAGE_TYPE::GENERIC,
            SNDS_CLIENT_TYPES::SERVICE,
            ""
        );

        v_to_dt->writeMessage(std::move(message));
    }

    if (ids.empty()){
        m_logger->info("Removing name {} from pendingIDsForType since IDs are empty", name);
        // Can safely remove without check 
        pendingIDsForTypeMap.erase(name);
    }
}

void Worker::processRegistryFileRequest(const std::string& payload, const std::string& name) {

    // Parse the payload into a vector of IDs.
    std::vector<std::string> ids = parseIDsFromRegistryFilePayload(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(payload.data()),
            payload.size()
        ),
        m_logger
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
        
        // Update pendingTypesForIDMap: for this id, add the type name.
        pendingTypesForIDMap[id].insert(name);
        
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