#include "snds_http_worker.h"

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

std::vector<std::string> HTTPWorker::getIDsFromRegistryFilePayload (const std::vector<char>& payload) {
    if (payload.empty()) {
      m_logger->warn("Payload is empty. Returning.");
      return {};
    }

    size_t offset = 0;
    // Get the number of IDs
    uint8_t numIds = static_cast<uint8_t>(payload[offset++]);

    std::vector<std::string> ids;
    ids.reserve(numIds);

    for (uint8_t i = 0; i < numIds; ++i) {
      if (offset >= payload.size()) {
        m_logger->warn("Unexpected end of payload while reading ID length.");
        return ids;
      }

      // Read the length of the next ID.
      uint8_t idLength = static_cast<uint8_t>(payload[offset++]);

      if (offset + idLength > payload.size()) {
        m_logger->warn("Unexpected end of payload while reading ID.");
        return ids;
      }

      // Construct the ID string and add it to the vector.
      ids.emplace_back(payload.data() + offset, idLength);
      offset += idLength;
    }

    return ids;
}

void HTTPWorker::processRegistryFileResponse(const std::string& registry, const std::string& payload) {
    std::vector<char> payloadVec(payload.begin(), payload.end());
    auto ids = getIDsFromRegistryFilePayload(payloadVec);
    m_logger->info("Registry file contains {} IDs", ids.size());

    // For each id in the registry file, update pending state and issue a new getByID request.
    for (const auto& id : ids) {
        std::string formattedId = id;
        ensureLeadingSlash(formattedId);

        // Update pendingIDsToTypesMap.
        if (pendingIDsToTypesMap.find(formattedId) == pendingIDsToTypesMap.end()) {
            // Create a new vector if it doesn't exist.
            auto tmpv = new std::vector<std::string>();
            tmpv->push_back(registry);
            pendingIDsToTypesMap[formattedId] = tmpv;
        } else {
            // Otherwise, append the registry name.
            pendingIDsToTypesMap[formattedId]->push_back(registry);
        }

        // Create a new getByID request message for the id.
        auto msg = std::make_unique<SNDS_INTRA_MESSAGE>(
                std::move(formattedId),
                SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
                SNDS_CLIENT_TYPES::SERVICE,
                ""
        );
        m_logger->info("Pushing getByID request for ID: {}", formattedId);
        v_to_consumer.get()->writeMessage(std::move(msg));
    }

   // Update pendingIDsForTypeMap with the full list of IDs, keyed by the registry.
   auto v_ids = new std::vector<std::string>();
   v_ids->assign(ids.begin(), ids.end());
   pendingIDsForTypeMap[registry] = v_ids;
}

void HTTPWorker::cleanUpPendingTypeForId(const std::string& id) {
    // Look up the id in pendingIDsToTypesMap (maps IDs to pending getByType requests)
    auto iter = pendingIDsToTypesMap.find(id);
    if (iter == pendingIDsToTypesMap.end()) {
        return; // ID not found, nothing to remove.
    }

    // Get the vector of requests associated with this id.
    std::vector<std::string>* types = iter->second;

    // Iterate through each request that the id is associated with.
    // We increment inside the loop, in case we remove an element.
    for (size_t k = 0; k < types->size();) {
        std::string typeKey = types->at(k);

        // Look up the vector of this type's pending ids.
        auto vidsIter = pendingIDsForTypeMap.find(typeKey);
        if (vidsIter == pendingIDsForTypeMap.end()) {
            m_logger->error("FATAL: Inconsistent state. Type {} not found for ID {}.", typeKey, id);
            exit(1);
        }

        // Get the vector of ids.
        std::vector<std::string>* vids = vidsIter->second;

        // Find the given id in the vector.
        auto itId = std::find(vids->begin(), vids->end(), id);
        if (itId == vids->end()) {
            m_logger->error("FATAL: Inconsistent state. ID {} not found in pending IDs for type {}.", id, typeKey);
            exit(1);
        }

        // Remove id from this type's pending list.
        vids->erase(itId);

        // If no more IDs remain for this type, clean up that type.
        if (vids->empty()) {
            delete vids;
            pendingIDsForTypeMap.erase(typeKey);
            // Erase the type from types.
            types->erase(types->begin() + k);
            // Do not increment k since vector elements shift left.
        } else {
            // Move to the next type
            ++k;
        }
    }

    // After cleaning up, if no type keys remain for this id, remove the id entry entirely.
    if (types->empty()) {
        delete types;
        pendingIDsToTypesMap.erase(iter);
    }
}

void HTTPWorker::processByIDResponse(const std::string& id) {
    bool handled = false;

    // Remove the id from the pending getByID requests.
    auto iter = std::find(pendingGetByIDreqs.begin(), pendingGetByIDreqs.end(), id);
    if (iter != pendingGetByIDreqs.end()) {
        m_logger->info("Got getByID response for ID: {}. Removing state.", id);
        pendingGetByIDreqs.erase(iter);
        handled = true;
    }

    // Remove the id from any type-based pending state.
    cleanUpPendingTypeForId(id);
    handled = true;

    // Warn if nothing was handled (should not normally happen).
    if (!handled) {
        m_logger->warn("Response for ID {} was not handled. Pending state might be inconsistent.", id);
    }
}

void HTTPWorker::processByTYPEResponse(const std::string& type) {
    // Remove the type from the pending getByTYPE requests.
    auto iter = std::find(pendingGetByTYPEreqs.begin(), pendingGetByTYPEreqs.end(), type);
    if (iter != pendingGetByTYPEreqs.end()) {
        m_logger->info("Got getByTYPE response for type: {}. Removing pending state.", type);
        pendingGetByTYPEreqs.erase(iter);
    } else {
        m_logger->warn("Received getByTYPE response for type {} but no pending request found.", type);
    }
}


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
        const std::string id   = getFromGETRequest(content, PARAM_ID_TAG);
        const std::string type = getFromGETRequest(content, PARAM_TYPE_TAG);
        std::string subUrl     = getURLfromSubscription(content);
        bool isSub             = isSubscriptionRequest(content);

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
                if (id.find(m_registryFilePostfix) != std::string::npos) {
                    m_logger->info("HTTPWorker: Processing registry file response...");
                    processRegistryFileResponse(id, content);
                }
                // Next, handle getById responses.
                else if (!id.empty()) {
                    m_logger->info("HTTPWorker: Processing getByID response...");
                    processByIDResponse(id);
                }
                // Otherwise, handle getByType responses.
                else if (!type.empty()) {
                    m_logger->info("HTTPWorker: Processing getByTYPE response...");
                    processByTYPEResponse(type);
                }
                break;
            default:
                m_logger->info("HTTPWorker: Received unprocessable client type...");
                break;
        }
    }

    m_logger->info("HTTPWorker thread stopping.");
}
