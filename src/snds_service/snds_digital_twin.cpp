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


        getFromGETRequest(std::string_view(content), "id");
    }
}