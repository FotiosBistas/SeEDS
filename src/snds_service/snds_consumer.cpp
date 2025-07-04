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
        auto type = message.get()->getMessageType();
        TemporalQuery query = {};

        if(type == SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST){
            m_logger->debug("Extracted content from message {}", content);
            m_logger->debug("Extracted content length from message {}", length);
            query = deserialize_temporal_query(content);
            //query.date = md5_hex(&query.date, query.date.size());
            auto name = to_path(query);
            name = "SNDS" + name;
            m_logger->info("Created name {}", name);
            ndn::Name interestName(name);
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
            continue;
        }

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

void SNDSConsumer::parseTemporalQuery(const std::string& body){
    nlohmann::json root = nlohmann::json::parse(body);

	std::unordered_set<std::string> seenInterests;

    for (const auto& entry : root) {
        // entry is an object with a single key: "before" or "after" or "now"
        for (auto& [key, arr] : entry.items()) {
			m_logger->debug("Parsing key: {}", key);
            for (const auto& doc : arr) {
				// this contains both timestamp and id
                const auto&  id_object  = doc["_id"];
                std::string   id        = id_object["_id"].get<std::string>();
                long long raw_timestamp = id_object["timestamp"]["$date"].get<long long>();
				std::string timestamp = "date=" + format_timestamp_ms(raw_timestamp);
				timestamp = url_encode(timestamp);
                auto name = makeMetafileInterest("/SNDS/" + id, timestamp);
                name.params.push_back(url_encode("metafile=true"));
                auto interestUri = toName(name).toUri();

                // try to insert; if it's new, send it
                if (seenInterests.emplace(interestUri).second) {
                    m_logger->info("Built interest: {} sending actual interest: {}", name.toString(), interestUri);
                    ndn::Name interestName(interestUri);
                    m_logger->info("Created interest name {}", interestName.toUri().c_str());
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
                else {
                    m_logger->debug("Skipping duplicate interest: {}", interestUri);
                }
            }
        }
    }
}


void SNDSConsumer::onData(const ndn::Interest& interest, const ndn::Data& data) {
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

    if (payload.find(WARNING_ALREADY_REGISTERED) != std::string::npos){
        m_logger->info("Got warning for already registered type in {}", __FUNCTION__);
        return;
    }

    if(interest.toUri().find(temporalQueryPrefix) != std::string::npos){
        m_logger->info("Temporal Query Reponse...");
        auto pos = payload.find_first_of("[{");

        if (pos != std::string::npos) {
            payload = payload.substr(pos);
            m_logger->debug("Filtered payload to:\n{}", payload);
        }

        parseTemporalQuery(payload);
        return;
    }


    if (interest.toUri().find(m_registryFilePostfix) != std::string::npos){
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
