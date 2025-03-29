#include "snds_digital_twin.h"


void DigitalTwin::init(){
    m_logger->info("Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Thread started.");


    while(running.load()){
        m_logger->debug("DigitalTwin: Reading packet...");

        std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_dt->readMessage();
        if (message == nullptr) {
            m_logger->debug("DigitalTwin: Queue shutdown detected, exiting loop.");
            break;
        }

        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();
        m_logger->debug("Extracted content from message {}", content);
        m_logger->debug("Extracted content length from message {}", length);


        std::string id = getFromGETRequest(content, "id");
        std::string idScope = getSNDSNodeScopeNameForSubscription(id, m_idScopePrefix, m_numberOfSndsSrvs);

        m_logger->debug("Extracted id from message {}", id);
        m_logger->debug("Extracted idScope from message {}", idScope);

        if(content.find(ERASE_PREFIX) != std::string::npos){
            m_logger->info("Received erase ID command for ID {}...", id);
            ensureLeadingSlash(id);
            if(announcedIDsVec.erase(id) > 0){
                m_logger->debug("Successfully removed ID {}", id);
            };
            continue;
        }

        if(!id.empty()){
            m_logger->info("Received by ID get request...");
            ensureLeadingSlash(id);
            auto [_, new_request] = announcedIDsVec.insert(id);
            if(!new_request){
                m_logger->debug("ID {} is already in the announcedIDsVec. Skipping...", id);
                continue;
            }
            m_logger->debug("Pushed ID {} to the announced IDs vector...", id);
            auto message1 = std::make_unique<SNDS_INTRA_MESSAGE>(
                std::string(id),
                SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
                SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );
            m_logger->info("Pushing byID advertisement to the Producer...\n{}", message1.get()->printDetails());
            v_to_producer->writeMessage(std::move(message1));


            auto message2 = std::make_unique<SNDS_INTRA_MESSAGE>(
                std::string(idScope) + std::string(id),
                SNDS_INTRA_MESSAGE_TYPE::GENERIC,
                SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );

            m_logger->info("Pushing byID with Scope advertisement to the Consumer...\n{}", message2.get()->printDetails());
            v_to_consumer->writeMessage(std::move(message2));
        }

        std::string type = getFromGETRequest(content, "type");
        m_logger->debug("Extracted Type from message {}", type);

        if(!type.empty()){
            m_logger->info("Received by Type get request...");
            ensureLeadingSlash(type);
            m_logger->debug("The Type is the following: {}", type);
            ensureLeadingSlash(id);
            m_logger->debug("The ID is the following: {}", id);
            auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
                type+id,
                SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_RESPONSE,
                SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );
            m_logger->info("Pushing byType advertisement to the Consumer...\n{}", message.get()->printDetails());
            v_to_consumer->writeMessage(std::move(message));
        }
    }
}