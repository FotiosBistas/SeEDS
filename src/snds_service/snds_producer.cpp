#include "snds_producer.h"

void SNDSProducer::init(){
    m_logger->info("Producer: Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Producer thread started.");
        
    // Set up your initial interest filters
    std::string tmp_name = m_mySndsIdName;
    //[2025-03-04 08:35:24.437] [info] Announcing ID name: /SNDS_0
    m_logger->info("Announcing ID name: {}", tmp_name);
    m_face.setInterestFilter(tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

    tmp_name = m_mySndsIdScopeName;
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/SNDS_ID_scope_0
    m_logger->info("Announcing ID Scope name: {}", tmp_name);
    m_face.setInterestFilter(tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

    // Advertise for each type in your registry map
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR_registry
    for (const auto &myPair : myTypesRegistryMap) {
        tmp_name = m_sndsPrefix + myPair.first;
        m_logger->info("Announcing name: {}", tmp_name);
        m_face.setInterestFilter(tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

        // For the registry file
        tmp_name += m_registryFilePostfix;
        m_logger->info("Announcing name: {}", tmp_name);
        m_face.setInterestFilter(tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));
    }

    // Optionally register a certificate filter (if needed)
    ndn::security::Certificate cert;
    try {
        cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
        m_certServeHandle = m_face.setInterestFilter(
            ndn::security::extractIdentityFromCertName(cert.getName()),
            [this, cert] (auto&&...) { m_face.put(cert); }, // Ensure correct lambda signature
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
        );
    } catch (const ndn::security::pib::Pib::Error &e) {
        m_logger->error("No default identity found: {}", e.what());
        return;
    }

    // Process events in a blocking mode once (if needed)
    m_face.processEvents(ndn::time::milliseconds(-1));

    // Run the loop until 'running' is false.
    while (running.load()) {
        m_logger->debug("Producer loop iteration; running = {}", running.load());
        // Process events non-blockingly (or with a timeout)
        m_face.processEvents(ndn::time::milliseconds(1000)); // Process for 1 second

        // Check if there are messages to process for advertisement
        std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_producer->readMessage();
        if(message == nullptr){
            m_logger->debug("Producer: empty queue, continuing...");
            continue;
        }

        m_logger->info("Got new advertisement request. Message: {}", message.get()->printDetails());
        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();
        tmp_name = m_sndsPrefix + std::string(content, length);
        m_logger->info("Announcing name: {}", tmp_name);
        locallyAnnouncedIDsMap[std::string(content, length)] = "SNDS_dummy_data";

        m_face.setInterestFilter(tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));
    }

    m_logger->info("Producer thread exiting.");
}



void SNDSProducer::onInterest(const ndn::Interest& interest) {
}

std::vector<char> SNDSProducer::getRegistryFilePayload(const std::vector<std::string>& ids) {
}

void SNDSProducer::processTypeAdvertisement(const ndn::Name& name, const ndn::Interest& interest) {
}


void SNDSProducer::processLocalIdRequest(const ndn::Name& name, const ndn::Interest& interest) {
}

void SNDSProducer::processSubscription(const ndn::Name& name, const ndn::Interest& interest) {
}

void SNDSProducer::processRegistryRequest(const ndn::Name& name, const ndn::Interest& interest) {
}
