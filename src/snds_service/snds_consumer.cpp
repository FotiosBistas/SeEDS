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

        if (message == nullptr){
            // No message available; sleep for a short while before checking again.
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
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
}
