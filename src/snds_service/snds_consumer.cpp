#include "snds_consumer.h"

void SNDSConsumer::init(){
    m_logger->info("Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }

    // Run the loop until 'running' is false.
    while (running.load()) {

        m_logger->info("Reading packet...");
        std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_consumer->readMessage();

        if (message == nullptr) {
            m_logger->debug("Queue shutdown detected, exiting loop.");
            break;
        }

        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();
        m_logger->debug("Extracted content from message {}", content);
        m_logger->debug("Extracted content length from message {}", length);

        std::string name = m_sndsPrefix + content.substr(0, length);
        m_logger->info("Created name {}", name);
        ndn::Name interestName(name);
        m_logger->info("Created interest name {}", interestName.toUri().c_str());
        interestName.appendVersion();
        ndn::Interest interest(interestName);
        interest.setMustBeFresh(true);
        interest.setCanBePrefix(true);
        interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
        m_logger->info("Sending actual Interest {}", interest.toUri().c_str());

        m_face.expressInterest(interest,
                                std::bind(&SNDSConsumer::onData, this,  _1, _2),
                                std::bind(&SNDSConsumer::onNack, this, _1, _2),
                                std::bind(&SNDSConsumer::onTimeout, this, _1));
		m_face.processEvents();
    }
}

void SNDSConsumer::onData(const ndn::Interest&, const ndn::Data& data) {
    // Log the receipt of the Data packet.
    m_logger->info("Received Data with name: {0} in function {1}", data.getName().toUri(), __FUNCTION__);

    // Convert the data content into a string.
    std::string payload(reinterpret_cast<const char*>(data.getContent().data()), data.getContent().size());
    m_logger->info("Data packet payload: {0} in function {1}", payload, __FUNCTION__);

    // Ensure the Data name has at least two components.
    if (data.getName().size() < 2) {
        m_logger->warn("WARNING: Got unexpectedly small content name. Skipping.");
        return;
    }

    // If the payload contains the ACK tag, then no further processing is needed.
    if (payload.find(m_ackTag) != std::string::npos) {
        m_logger->info("Got ACK in function {}. Skipping.", __FUNCTION__);
        return;
    }

    if (data.getName().toUri().find(m_registryFilePostfix) != std::string::npos){
        m_logger->info("Received Registry Request");
        std::string message_name = data.getName().at(1).toUri();
        message_name = "type=" + message_name;
        m_logger->info("In Function: {0} sending name {1} to VTOService {2}", __FUNCTION__, message_name, payload);
        // Create a new intra-service message with the payload.
        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(payload),
            SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_RESPONSE, //TODO not sure this is correct
            SNDS_CLIENT_TYPES::CONSUMER,
            message_name.c_str()
        );
        // Dispatch the intra-service message to the service queue for further processing.
        v_to_service->writeMessage(std::move(message));
        return;
    }

    // Extract the second component of the Data name, which is used as the intra-message name.
    std::string message_name = data.getName().at(1).toUri();
    // TODO currently this sends only IDs
    message_name = "id=" + message_name;

    m_logger->info("In Function: {0} sending name {1} to VTOService {2}", __FUNCTION__, message_name, payload);
    // Create a new intra-service message with the payload.
    auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
        std::move(payload),
        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE, //TODO not sure this is correct
        SNDS_CLIENT_TYPES::CONSUMER,
        message_name.c_str()
    );

    // Dispatch the intra-service message to the service queue for further processing.
    v_to_service->writeMessage(std::move(message));
}
