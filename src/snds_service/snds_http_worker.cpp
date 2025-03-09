#include "snds_http_worker.h"

void HTTPWorker::processSubscriptionRequest(const std::string& id, const std::string& subURL){
    // Insert new subscription or update existing.
    auto [iter, inserted] = subscribedBYID_users.emplace(id, std::vector<std::string>{});
    if (inserted) {
        iter->second.push_back(subURL);
        m_logger->info("Stored new getByID subscription for ID: {}", id);
    } else {
        // Avoid duplicates.
        auto &urls = iter->second;
        if (std::find(urls.begin(), urls.end(), subURL) == urls.end()) {
            urls.push_back(subURL);
            m_logger->info("Stored additional getByID subscription for ID: {}", id);
        } else {
            m_logger->info("Duplicate subscription for ID: {} ignored.", id);
        }
    }

    // Forward subscription interest to responsible SNDS node.
    std::string preparedID = id;  // Create a modifiable copy
    ensureLeadingSlash(preparedID);

    std::string interestName = getSNDSNodeScopeNameForSubscription(preparedID, m_idScopePrefix, m_numberOfSndsSrvs) + preparedID;
    
    m_logger->info("Pushing getByID subscription interest for ID: {}", preparedID);
    
    auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(interestName), 
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::SERVICE,
            ""
    );

    v_to_consumer.get()->writeMessage(std::move(message));
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
                    processSubscriptionRequest(id, subUrl);
                }
                // Next, handle regular getByID requests.
                if (!id.empty()) {
                    processByIDRequest(id);
                }
                // Otherwise, handle getByType requests.
                else if (!type.empty()) {
                    processByTYPERequest(type);
                }
                break;
            }
            case SNDS_CLIENT_TYPES::CONSUMER:
                m_logger->info("HTTPWorker: Received consumer message, no further action taken.");
                break;
            default:
                m_logger->info("HTTPWorker: Received unprocessable client type...");
                break;
        }
    }

    m_logger->info("HTTPWorker thread stopping.");
}
