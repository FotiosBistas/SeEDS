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

        if (!message){
            m_logger->debug("HTTPWorker: Queue shutdown detected, exiting loop.");
            break;
        }

		m_logger->debug("HTTPWorker: Received packet {}...", message.get()->printDetails());

        auto content = message.get()->getMessage(); 
        auto id = getFromGETRequest(content, PARAM_ID_TAG);
        auto type = getFromGETRequest(content, PARAM_TYPE_TAG);


        if (id.empty() && type.empty()) {
            m_logger->warn("HTTPWorker: Both ID and Type parameters are empty in GET request.");
            continue; // Skip processing this message
        }

        m_logger->debug("Extracted ID from request: {}", id);
        m_logger->debug("Extracted TYPE from request: {}", type);

        switch(message->getSender()){
            case SNDS_CLIENT_TYPES::CONSUMER: 
                m_logger->info("HTTPWorker: Received consumer message...");
                break;
            case SNDS_CLIENT_TYPES::HTTP_SRV: 
                m_logger->info("HTTPWorker: Received HTTP Server message...");

                if(isSubscriptionRequest(content)){
                    m_logger->info("HTTPWorker: Received Subscription Request...");
                    break;
                }else if(!id.empty()){
                    m_logger->info("HTTPWorker: Received byID Request...");
                    break;
                }else if(!type.empty()){
                    m_logger->info("HTTPWorker: Received byType Request...");
                    break;
                }

                break;
            default:
                m_logger->info("HTTPWorker: Received unprocessable client type...");
        }
    }

    m_logger->info("HTTPWorker thread stopping.");
}