#include "snds_http_worker.h"


void HTTPWorker::init(){
    m_logger->info("HTTPWorker: Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("HTTPWorker thread started.");

    while(running.load()){
		m_logger->info("HTTPWorker: Reading packet..");

        auto message = v_to_service.get()->readMessage();

        if (message == nullptr){
            m_logger->debug("HTTPWorker: Queue shutdown detected, exiting loop.");
            break;
        }

        switch(message->getSender()){
            case SNDS_CLIENT_TYPES::CONSUMER: 
                m_logger->info("HTTPWorker: Received consumer message...");
                std::string name(message->getName(), message->getName().length());
                ensureLeadingSlash(name);
			    m_logger->info("Got Data packet for CONSUMER with name: {}", name);
                break;
            case SNDS_CLIENT_TYPES::HTTP_SRV: 
                m_logger->info("HTTPWorker: Received HTTP Server message...");
                break;
            default:
                m_logger->info("HTTPWorker: Received unprocessable client type...");
        }
    }

    m_logger->info("HTTPWorker thread stopping.");
}