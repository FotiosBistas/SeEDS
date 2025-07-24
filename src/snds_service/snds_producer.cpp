#include "snds_producer.h"

void SNDSProducer::init(){
    m_logger->info("Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Thread started.");

    //[2025-03-04 08:35:24.437] [info] Announcing ID name: /SNDS_0
    m_logger->info("Announcing SNDS ID name: {}", snds_id_name);
    m_face.setInterestFilter
    (
        snds_id_name,
        std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
        std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
        std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
    );

    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/SNDS_ID_scope_0
    m_logger->info("Announcing ID Scope name: {}", snds_prefix + "/" + snds_id_scope_name);
    m_face.setInterestFilter
    (
        snds_prefix + "/" + snds_id_scope_name,
        std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
        std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
        std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
    );

    // Advertise for each type in the registry map
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR_registry
    for (const auto &type: type_registry) 
    {
        auto tmp_name = parsed_interest_name::toCustomNDNName
        (
            parsed_interest_name::makeParsedInterestName
            (
                snds_prefix, 
                type.first,
                interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY,
                {}
            )
        );

        m_logger->info("Announcing name: {}", tmp_name);
        m_face.setInterestFilter
        (
            tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
        );

        tmp_name = parsed_interest_name::toCustomNDNName
        (
            parsed_interest_name::makeParsedInterestName
            (
                snds_prefix, 
                type.first + m_registryFilePostfix,
                interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY,
                {}
            )
        );

        // For the registry file
        m_logger->info("Announcing name: {}", tmp_name);
        m_face.setInterestFilter
        (
            tmp_name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
        );
    }

    // Optionally register a certificate filter (if needed)
    ndn::security::Certificate cert;
    try 
    {
        cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
        m_certServeHandle = m_face.setInterestFilter
        (
            ndn::security::extractIdentityFromCertName(cert.getName()),
            [this, cert] (auto&&...) 
            {
                m_face.put(cert);
            },
            std::bind(&SNDSProducer::onRegisterFailed, this, _1, _2)
        );
    } 
    catch (const ndn::security::pib::Pib::Error &e) 
    {
        m_logger->error("No default identity found: {}", e.what());
        ndn::Name identityName("Producer");
        // create in PIB, which also installs a key and certificate
        auto identity = m_keyChain.createIdentity(identityName);
        // mark it as the default
        m_keyChain.setDefaultIdentity(identity);
        m_logger->info("Created and set default identity: {}", identityName.toUri());
    }

    // Process events in a blocking mode once (if needed)
    m_face.processEvents(ndn::time::milliseconds(-1));

    // Run the loop until 'running' is false.
    while (running.load())
    {
        // m_logger->debug("Reading packet...");
        // Process events non-blockingly (or with a timeout)
        m_face.processEvents(ndn::time::seconds(2)); // Process for 1 second

        //// Check if there are messages to process for advertisement
        //std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_producer->readMessage();

        //if (message == nullptr) {
        //    m_logger->debug("Queue shutdown detected, exiting loop.");
        //    break;
        //}

        // Check if there are messages to process for advertisement
        std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> message = v_to_producer->tryReadMessage();

        if (message == nullptr) 
        {
            continue;
        }

        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();
        if(message.get()->getMessageType() == snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST)
        {
            m_logger->info("Got new Temporal Query request...\n{}", message.get()->printDetails());
            auto query = deserialize_temporal_query(content);
            //query.date = md5_hex(&query.date, query.date.size());
            auto name = to_path(query, false, false, false);
            name = "/SNDS" + name;
            m_logger->info("Annoucing name {}", name);

            m_face.setInterestFilter
            (
                name,
                std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
                std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
                std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
            );

            //TODO possible spot for signing
            //TODO insert more payload here
            std::string payload = serialize_temporal_query(query);
            //TODO receive data from the broker here?
            httplib::Client cli(broker_ip.c_str(), broker_port);
            auto res = cli.Post("/add", payload, "application/json");
            if (res && res->status == 200) {
                m_logger->info("Successfully POSTed to /add: {}", res->body);
            } else {
                m_logger->error("Failed to POST to /add: {}", res ? res->body : "No response");
            }
        }
        else if(message.get()->getMessageType() == snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT)
        {
            m_logger->info("Got new ID advertisement request...\n{}", message.get()->printDetails());
            auto parsed_interest = parsed_interest_name::parseFromString(content);
            m_logger->debug("Parsed interest {} from string", parsed_interest.toString());

            // we know that the structure of the interest should be /prefix/id
            auto to_be_announced_name = parsed_interest_name::toCustomNDNName
            (
                parsed_interest_name::makeParsedInterestName
                (
                    parsed_interest.prefix, 
                    parsed_interest.name, 
                    interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY, 
                    {}
                )
            );

            m_logger->info("Announcing name: {}", to_be_announced_name);

            m_face.setInterestFilter
            (
                to_be_announced_name,
                std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
                std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
                std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
            );

        }
    }

    m_logger->info("Producer thread exiting...");
}

void SNDSProducer::onInterest(const ndn::Interest& interest) 
{
    const ndn::Name name = interest.getName();
    m_logger->info("Received interest in function: {0}, name: {1}", __FUNCTION__, name.toUri());
    parsed_interest_name::ParsedInterestName pi = parsed_interest_name::parseFromString(url_decode(name.toUri()));
    m_logger->info("Parsed interest: {}", pi.toString());


    switch (pi.command)
    {
    case interest_message_type::INTEREST_MESSAGE_TYPE::SCOPE_NOTIFICATION:
        processNotification(pi);
        break;
    case interest_message_type::INTEREST_MESSAGE_TYPE::SUBSCRIPTION:
        processSubscription(pi);
        break;
    case interest_message_type::INTEREST_MESSAGE_TYPE::GET_ID:
        processByIDInterest(pi);
        break;
    default:
        break;
    }

    //if(pi.params.size() > 1 && hasDateTag(pi.params[0]))
    //{

    //    if(hasDateTag(pi.params[0]) && !isMetaFileTrue(pi.params[1]))
    //    { 
    //        m_logger->info("Received Temporal Query Interest...");
    //        handleTemporalQuery(pi);
    //        return;
    //    }

    //    if(isMetaFileTrue(pi.params[1]))
    //    {
    //        m_logger->info("Received Temporal Query Interest from Metafile...");
    //        processLocalIdRequest(pi);
    //        return;
    //    }
    //}


    //// Type advertisements
    //if (myTypesRegistryMap.find("/" + pi.name) != myTypesRegistryMap.end()) 
    //{
    //    processTypeAdvertisement(pi);
    //    return;
    //}

    //// If the name contains the registry postfix, handle registry requests.
    //if (pi.name.find(m_registryFilePostfix) != std::string::npos) {
    //    processRegistryRequest(pi);
    //    return;
    //}

    ////TODO I don't think this is correct
    ////TODO figure out the new if statement here
    //processByIDInterest(pi);
    //m_logger->warn("Interest left unprocessed. No matching handler found.");
}

//void SNDSProducer::handleTemporalQuery(parsed_interest_name::ParsedInterestName& interest)
//{
//
//    std::string date = url_decode(interest.params[0]);
//    std::string id = interest.name;
//
//    // build a map<string,string> out of interest.params,
//    // expecting each param to be in the form "key=value"
//    // currently params are count=,timeframe=,
//	// parameters are url encoded by convention
//    std::unordered_map<std::string, std::string> parameter_map;
//    for (const auto& parameter : interest.params) 
//    {
//		auto decoded_parameter = url_decode(parameter);
//        auto equal_sign_index = decoded_parameter.find('=');
//        if (equal_sign_index == std::string::npos) continue;
//        auto key = decoded_parameter.substr(0, equal_sign_index);
//        auto val = decoded_parameter.substr(equal_sign_index + 1);
//        parameter_map.emplace(key, val);
//    }
//
//	std::string type      = parameter_map.count("type")      ? parameter_map["type"]      : "";
//    std::string timeframe = parameter_map.count("timeframe") ? parameter_map["timeframe"] : "";
//    int count             = parameter_map.count("count")     ? std::stoi(parameter_map["count"]) : 0;
//
//	m_logger->debug("TemporalQuery → id: {}, date: {}, type: {}, timeframe: {}, count: {}", id, date, type, timeframe, count);
//    httplib::Client cli(broker_ip.c_str(), broker_port);
//    std::string endpoint = fmt::format
//    (
// 		"/temporal-query?id={}&type={}&count={}&date={}&timeframe={}",
//        url_encode(id),
//        url_encode(type),
//        count,
//        url_encode(date),
//        url_encode(timeframe)
//    );
//
//	m_logger->debug("Requesting to endpoint: {}", endpoint);
//
//    auto res = cli.Get(endpoint.c_str());
//    if (res && res->status == 200) {
//        m_logger->info("Successfully queried /temporal-query");
//        sendData(toName(interest), nlohmann::json::parse(res->body).dump(2));
//    } else {
//        m_logger->error("Failed to query /temporal-query: {}", res ? res->body : "No response");
//    }
//}
//
//
//void SNDSProducer::processTypeAdvertisement(ParsedInterestName& interest) {
//
//  	m_logger->info("Processing Type Advertisement");
//  	// Attempt to get the advertised ID from the parsed parameters.
//  	// TODO hopefully one ID received here
//  	std::string id_for_type = interest.params[0];
//  	std::string type = interest.name;
//  	ensureLeadingSlash(type);
//  	if (id_for_type.empty()) {
//  	  	m_logger->warn("No ID provided in type advertisement. Ignoring.");
//  	  	return;
//  	}
//
//  	m_logger->info("Type advertisement: name(registry) = {0}, ID = {1}", type, id_for_type);
//
//  	// Look up the content type (with a preceding slash) in the registry map.
//  	auto iter = myTypesRegistryMap.find(type);
//
//  	if (iter == myTypesRegistryMap.end()){
//  	  	m_logger->warn("Type {} not found in registry. Ignoring advertisement.", type);
//  	}
//
//  	// Access the vector of IDs stored by value.
//  	auto& ids = iter->second;
//
//  	// Try to insert the ID; check if it was already present
//  	auto [_, inserted] = ids.insert(id_for_type);
//
//  	if (!inserted) {
//  	  	m_logger->warn("ID already registered for type {}. Skipping...", type);
//  	  	std::string warn_message = "WARNING_ALREADY_REGISTERED::ID already registered for type {" + type+ "}";
//  	  	sendData(toName(interest), warn_message);
//  	  	return;
//  	}
//
//  	m_logger->info("Registered new ID {0} for type {1}. Sending ACK.", id_for_type, type);
//  	sendData(toName(interest), m_ackTag);
//}
//

void SNDSProducer::processSubscription(const parsed_interest_name::ParsedInterestName& interest) 
{
    auto maybe_name         = interest.getParam(0);
    auto maybe_snds_node_id = interest.getParam(1);
    auto interest_response = parsed_interest_name::toCustomNDNName(interest, true);

    if (!maybe_name.has_value() || !maybe_snds_node_id.has_value())
    {
        m_logger->error("In function {} got Name or SNDS Node ID with no value.", __func__);
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_ACK_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        return;
    }

    auto name = *maybe_name;
    auto snds_node_id = *maybe_snds_node_id;

    m_logger->info("Got interest indicating new subscription for name: {0}, and snds node: {1}", name, snds_node_id);
    auto& subscribers = subscribed_nodes_for_id[name];
    // Try to insert the ID into the set
    auto [_, inserted] = subscribers.insert(snds_node_id);
    if (inserted) 
    {
        m_logger->info("New subscription: {} subscribed to {}", snds_node_id, name);
        m_logger->info("Responding to interest: {}", interest_response);
        //TODO proper ack messages
        auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_ACK_MESSAGE_TYPE::ACK_SUBSCRIPTION_NEW);
        sendData(interest_response, ack_message);
    }
}
//
//void SNDSProducer::processRegistryRequest(ParsedInterestName& interest) {
//
//  	std::string name = interest.name;
//  	// Parse the incoming Interest name.
//  	m_logger->info("Registry request received: name = {}", name);
//
//  	// Remove the registry postfix from the name to get the base type.
//  	std::string base_type = name.substr(0, name.find(m_registryFilePostfix));
//
//  	// Look up the registry for the base type (the keys in myTypesRegistryMap are stored with a leading slash).
//  	auto iter = myTypesRegistryMap.find("/" + base_type);
//  	if (iter == myTypesRegistryMap.end()) {
//  	  	m_logger->warn("Unknown registry for type {}.", base_type);
//  	  	return;
//  	}
//
//  	std::vector<std::byte> byte_vector;
//  	byte_vector.push_back(std::byte{'R'});  // registry response marker
//  	byte_vector.push_back(std::byte{0x00}); // reserved/future use
//  	byte_vector = serializeRegistryFilePayload(iter->second);
//
//  	std::string payload(reinterpret_cast<const char*>(byte_vector.data()), byte_vector.size());
//
//  	m_logger->info("Sending registry file payload for type {}.", base_type);
//  	sendData(toName(interest), payload);
//}
//

void SNDSProducer::processNotification(const parsed_interest_name::ParsedInterestName& interest)
{

    auto interest_response = parsed_interest_name::toCustomNDNName(interest, true);
    // this name is an ID like car1
    auto maybe_name = interest.getParam(0);
    if (!maybe_name.has_value())
    {
        m_logger->error("Received interest in {} with incorrect value for parameter.", __func__);
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_ACK_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        return;
    }

    auto name = *maybe_name;

    m_logger->info("Subscription notification for ID name {}.", name);

    auto iterator = subscribed_nodes_for_id.find(name);
    if(iterator == subscribed_nodes_for_id.end())
    {
        m_logger->info("No subscribers found for ID {}.", name);
        m_logger->info("Responding to interest: {}", interest_response);
        //TODO proper ack messages
        auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_ACK_MESSAGE_TYPE::ACK_SUBSCRIPTION_NO_SUBSCRIBERS);
        sendData(interest_response, ack_message);
        return;
    }

    //TODO need to check
    const auto& subscribers = iterator->second;
    for (const auto& snds_node : subscribers) 
    {
        auto interest_name = parsed_interest_name::toCustomNDNName
        (
            parsed_interest_name::makeParsedInterestName
            (
                snds_prefix, 
                snds_node + "/" + name,
                interest_message_type::INTEREST_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION,
                {}
            )
        );

        m_logger->info("Notifying SNDS node: {}", interest_name);

        auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::move(interest_name),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION,
            snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER,
            ""
        );
        v_to_consumer->writeMessage(std::move(message));
    }
    //TODO proper ack meesages
    auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_ACK_MESSAGE_TYPE::ACK_SUBSCRIPTION_NOTIFICATION);
    sendData(interest_response, ack_message);
}

void SNDSProducer::processByIDInterest(const parsed_interest_name::ParsedInterestName& interest) 
{
    auto interest_response = parsed_interest_name::toCustomNDNName(interest, true);
    m_logger->info("ID request received: id = {}", interest.name);
    // In local ID requests, we assume the name field itself is the requested ID.
    //TODO send data directly from the broker
    httplib::Client cli(broker_ip.c_str(), broker_port);
    std::string endpoint = fmt::format
    (
      	"/get?id={}",
        url_encode(removeLeadingSlash(interest.name))
    );

    auto res = cli.Get(endpoint.c_str());

    if (res && res->status == 200) 
    {
        //get the most recent one theoritically
        m_logger->info("Successfully queried /get");
        //TODO SIGNING 
        sendData(interest_response, res->body);
    } 
    else 
    {
        m_logger->error("Failed to query /get: {}", res ? res->body : "No response");
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_ACK_MESSAGE_TYPE::ACK_SUBSCRIPTION_NOTIFICATION);
        sendData(interest_response, nack_message);
    }

}


