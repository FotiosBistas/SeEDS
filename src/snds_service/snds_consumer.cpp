#include "snds_consumer.h"

void SNDSConsumer::init(){
    m_logger->info("Consumer: Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }

    // Run the loop until 'running' is false.
    while (running.load()) {

        m_logger->debug("Consumer: Reading packet...");
        std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_consumer->readMessage();

        if (message == nullptr) {
            m_logger->debug("Consumer: Queue shutdown detected, exiting loop.");
            break;
        }

        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();

        std::string name = m_sndsPrefix + std::string(content, length);
        ndn::Name interestName(name);
        m_logger->info("Consumer: Created interest name {}", interestName.toUri());
        interestName.appendVersion();
        ndn::Interest interest(interestName);
        interest.setMustBeFresh(true);
        interest.setInterestLifetime(ndn::time::milliseconds(6000)); // ✅ Correct way


        m_logger->info("Consumer: Sending Interest: {}", message.get()->printDetails());
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
    m_logger->info("Data packet payload: \"{}\"", payload);

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

    // Extract the second component of the Data name, which is used as the intra-message name.
    std::string messageName = data.getName().at(1).toUri();

    // Create a new intra-service message with the payload.
    auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
        std::move(payload),
        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE, //TODO not sure this is correct
        SNDS_CLIENT_TYPES::CONSUMER,
        messageName.c_str()
    );

    // Dispatch the intra-service message to the service queue for further processing.
    v_to_service->writeMessage(std::move(message));
}
