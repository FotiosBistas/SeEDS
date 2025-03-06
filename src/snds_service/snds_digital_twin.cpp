#include "snds_digital_twin.h"


void DigitalTwin::init(){
    m_logger->info("DigitalTwin: Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Digital Twin thread started.");


    while(running.load()){
        m_logger->debug("DigitalTwin: Reading packet...");

        std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_dt->readMessage();

        if (message==nullptr){
            // No message available; sleep for a short while before checking again.
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();


        std::string id = getFromGETRequest(content, "id");

        if(!id.empty()){
            m_logger->info("DigitalTwin: Received by ID get request...");
            ensureLeadingSlash(id);
            announcedIDsVec.push_back(id);
            auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
                std::move(id),
                SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
                SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );
            m_logger->info("DigitalTwin: Pushing byID advertisement to the Producer...");
            v_to_producer->writeMessage(std::move(message));
        }

        std::string type = getFromGETRequest(content, "type");

        if(!type.empty()){
            m_logger->info("DigitalTwin: Received by Type get request...");
        }
    }
}