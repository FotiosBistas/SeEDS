#include "snds_producer.hpp"

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

    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/is_alive_0
    m_logger->info("Announcing name: {}", snds_prefix + "/" + snds_is_alive_with_id);
    m_face.setInterestFilter
    (
        snds_prefix + "/" + snds_is_alive_with_id,
        std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
        std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
        std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
    );

    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/is_alive_0
    m_logger->info("Announcing name: {}", snds_prefix + "/" + snds_state_with_id);
    m_face.setInterestFilter
    (
        snds_prefix + "/" + snds_state_with_id,
        std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
        std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
        std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
    );

    events_.on_type_add = [this](const std::string& type, const std::string& id) 
    {
        m_logger->info("[event] type_add: {} -> {}", type, id);
        upsert_type_id_(type, id);
        update_store_with_broker_state_(type);
    };

    events_.on_type_remove = [this](const std::string& type, const std::string& id) 
    {
        m_logger->info("[event] type_remove: {} -> {}", type, id);
        erase_type_id_(type, id);
    };

    events_.on_subscribe_id = [this](const std::string& id, const std::string& node) 
    {
        m_logger->info("[event] subscribe_id: node={} -> id={}", node, id);
        upsert_sub_by_id_(id, node);
    };

    events_.on_unsubscribe_id = [this](const std::string& id, const std::string& node) 
    {
        m_logger->info("[event] unsubscribe_id: node={} from id={}", node, id);
        erase_sub_by_id_(id, node);
    };

    events_.on_subscribe_type = [this](const std::string& type, const std::string& node) 
    {
        m_logger->info("[event] subscribe_type: node={} -> type={}", node, type);
        upsert_sub_by_type_(type, node);
    };

    events_.on_unsubscribe_type = [this](const std::string& type, const std::string& node) 
    {
        m_logger->info("[event] unsubscribe_type: node={} from type={}", node, type);
        erase_sub_by_type_(type, node);
    };

    events_.on_announce_add = [this](const std::string& name) 
    {
        m_logger->info("[event] announce_add: {}", name);
        store_announce_(name);
    };

    events_.on_announce_remove = [this](const std::string& name) 
    {
        m_logger->info("[event] announce_remove: {}", name);
        store_unannounce_(name);
    };

    // Advertise for each type in the registry map
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR
    // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR_registry
    for (const auto &type: type_registry) 
    {
        auto tmp_name = parsed_interest_name::to_custom_ndn_name
        (
            parsed_interest_name::make_parsed_interest_name
            (
                snds_prefix, 
                type.first,
                parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY,
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

        tmp_name = parsed_interest_name::to_custom_ndn_name
        (
            parsed_interest_name::make_parsed_interest_name
            (
                snds_prefix, 
                type.first + registry_file_postfix,
                parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY,
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
        //if (message == nullptr) 
        //{
        //    break;
        //}

        auto content = message.get()->get_message();
        auto length = message.get()->get_message_length();
        auto type = message.get()->get_message_type();
        switch(type)
        {
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_ADVERTISEMENT:
            {
                m_logger->info("Received Temporal Query advertisement...\n{}", message->print_details());
                process_temporal_query_advertisement(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST:
            {
                m_logger->info("Got new Temporal Query request...\n{}", message.get()->print_details());
                process_temporal_query_request(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT:
            {
                m_logger->info("Got new ID advertisement request...\n{}", message.get()->print_details());
                process_by_id_advertisement(content);
                break;
            }

        }
    }

    m_logger->info("Producer thread exiting...");
}

void SNDSProducer::process_temporal_query_advertisement(const std::string& content)
{
    auto query = temporal_query::deserialize_temporal_query(content);
    m_logger->debug("Deserialized temporal query to: {} in {}", temporal_query::serialize_temporal_query(query), __func__);
    //query.date = md5_hex(&query.date, query.date.size());

    m_logger->debug("Query.type: {}", query.type);
    m_logger->debug("Query.id: {}", query.id);
    // Use type if provided; otherwise fall back to id
    bool is_by_type = !query.type.empty() ? true : false;
    const std::string& entity = (is_by_type ? query.type : query.id);


    if(is_by_type)
    {
        auto type = query.type;
        auto id = query.id;
        // Try to insert the ID; check if it was already present
        bool inserted = register_id_for_type(type_registry, events_, type, id);

        if (!inserted) 
        {
            m_logger->warn("ID already registered for type {}. Skipping...", type);
            auto warn_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::WARNING_ALREADY_REGISTERED);
            return;
        }
        m_logger->info("Inserted: {} to: {} registry", id, type);
    }
    auto name = temporal_query::to_path(query, false, false, false);
    m_logger->debug("Created name from query: {} in {}", name, __func__);
    auto to_be_announced_name = parsed_interest_name::to_custom_ndn_name
    (

        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY,
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


    nlohmann::json doc = 
    {
        {"id",        query.id},
        {"type",      query.type},
        {"timeframe", query.frame},
        {"date",      query.date},
        {"count",     query.count}
    };

    _publish_to_broker(doc);

    auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_TEMPORAL_QUERY_ADVERTISED);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        "", 
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_ADVERTISEMENT,
        snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER,
        std::move(ack_message),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_worker->writeMessage(std::move(message));
}

void SNDSProducer::process_by_id_advertisement(const std::string& content)
{
    auto parsed_interest = parsed_interest_name::parse_from_string(content);
    m_logger->debug("Parsed interest {} from string", parsed_interest.to_string());

    // we know that the structure of the interest should be /prefix/id
    auto to_be_announced_name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            parsed_interest.prefix, 
            parsed_interest.name, 
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY, 
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


    auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_ID_ADVERTISED);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        "", 
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT,
        snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER,
        std::move(ack_message),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_worker->writeMessage(std::move(message));
}

void SNDSProducer::process_temporal_query_request(const std::string& content)
{
    auto query = temporal_query::deserialize_temporal_query(content);
    m_logger->debug("Deserialized temporal query to: {} in {}", temporal_query::serialize_temporal_query(query), __func__);
    //query.date = md5_hex(&query.date, query.date.size());
    auto name = temporal_query::to_path(query, false, false, false);
    m_logger->debug("Created name from query: {} in {}", name, __func__);

    auto to_be_announced_name = parsed_interest_name::to_custom_ndn_name
    (

        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY,
            {}
        )
    );

    m_logger->info("Annoucing name {} in {}", to_be_announced_name, __func__);

    m_face.setInterestFilter
    (
        to_be_announced_name,
        std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
        std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
        std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2)
    );

    //TODO possible spot for signing
    //TODO insert more payload here
    std::string payload = temporal_query::serialize_temporal_query(query);
    //TODO receive data from the broker here?
    httplib::Client cli(broker_ip.c_str(), broker_port);
    auto res = cli.Post("/add", payload, "application/json");
    if (res && res->status == 200) 
    {
        m_logger->info("Successfully POSTed to /add: {}", res->body);
    } 
    else 
    {
        m_logger->error("Failed to POST to /add: {}", res ? res->body : "No response");
    }

}

void SNDSProducer::onInterest(const ndn::Interest& interest) 
{
    const ndn::Name name = interest.getName();
    m_logger->info("Received interest in function: {0}, name: {1}", __FUNCTION__, name.toUri());
    parsed_interest_name::ParsedInterestName pi = parsed_interest_name::parse_from_string(url_decode(name.toUri()));
    m_logger->info("Parsed interest: {}", pi.to_string());


    switch (pi.command)
    {
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SCOPE_NOTIFICATION:
        process_scope_notification_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_ID_SUBSCRIPTION:
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_TYPE_SUBSCRIPTION:
        process_new_subscription_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FROM_NODE:
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID:
        process_get_by_id_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT:
        process_by_type_advertisement_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_TYPE:
        process_get_by_type_interest(pi); 
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE:
        process_get_by_id_for_type_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION:
        process_subscription_notification_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::TEMPORAL_QUERY:
        process_temporal_query_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::VERSION:
        process_version_metafile_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_IS_ALIVE:
        process_resilience_is_alive_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_META:
        process_resilience_meta_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_PATCH:
        process_resilience_patch_interest(pi);
        break;
    default:
        break;
    }

}

void SNDSProducer::process_resilience_is_alive_interest(const parsed_interest_name::ParsedInterestName &interest)
{

}

void SNDSProducer::process_resilience_patch_interest(const parsed_interest_name::ParsedInterestName &interest)
{

}

void SNDSProducer::process_resilience_meta_interest(const parsed_interest_name::ParsedInterestName &interest)
{

}

void SNDSProducer::process_version_metafile_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    m_logger->debug("Created interest response: {} in {}", interest_response, __func__);

    auto maybe_id = interest.get_interest_parameter(0);

    if(!maybe_id.has_value())
    {
        m_logger->error("Receveid ID with no value...");
        return;
    }

    auto pair = parsed_interest_name::parse_param(maybe_id.value());

    auto id_key = pair.first;
    auto id= pair.second;

    m_logger->info("ID request received: {} = {}", id_key, id);
    // In local ID requests, we assume the name field itself is the requested ID.
    httplib::Client cli(broker_ip.c_str(), broker_port);
    std::string endpoint = fmt::format
    (
      	"/get?id={}",
        url_encode(id)
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
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        //sendData(interest_response, "dummy_payload");
    }

}

void SNDSProducer::process_subscription_notification_interest
(
    const parsed_interest_name::ParsedInterestName& interest
)
{
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);

    const auto maybe_id   = interest.get_interest_parameter(0);
    const auto maybe_type = interest.get_interest_parameter(1);
    const auto maybe_cb   = interest.get_interest_parameter(2); // notifying scope (optional)

    const std::string our_snds_id_scope = interest.name;

    if (!maybe_id) {
        m_logger->warn("Received ID with no value: {}", __func__);
        return;
    }
    if (!maybe_type) {
        m_logger->warn("Received TYPE param missing (may be fine for by-ID): {}", __func__);
    }
    if (!maybe_cb) {
        m_logger->warn("Received notifying SNDS NODE missing (optional): {}", __func__);
    }

    const std::string id   = *maybe_id;
    const std::string type = maybe_type.value_or("");
    const std::string cb   = maybe_cb.value_or("");

    const bool has_type = !type.empty();

    // Decide command + params + intra type
    using IMT = parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE;
    using MSG = snds_intra_message::SNDS_INTRA_MESSAGE_TYPE;

    const IMT interest_command = has_type ? IMT::GET_BY_TYPE : IMT::GET_BY_ID;
    const MSG intra_message_type = has_type ? MSG::GET_BY_TYPE_REQUEST : MSG::GET_BY_ID_REQUEST;

    // Name component: put the thing you're querying as the 'name'
    const std::string name_component = has_type ? type : id;

    // Params: pass just the queried token (your handlers read it from params)
    const std::vector<std::string> params = has_type ? std::vector<std::string>{type} : std::vector<std::string>{id};

    auto get_interest = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            name_component,
            interest_command,
            params
        )
    );

    m_logger->info
    (
        "Subscription Notification: id='{}', type='{}' ({}), scope='{}', cb='{}' -> issuing {}",
        id, type, has_type ? "byTYPE" : "byID", our_snds_id_scope, cb,
        snds_intra_message::to_string(intra_message_type)
    );

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(get_interest),
        intra_message_type,
        snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->debug("Sending message:\n{}\nin func: {}", message->print_details(), __func__);
    v_to_consumer->writeMessage(std::move(message));

    // ACK back to whoever notified us
    const auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_NODE_NOTIFIED);
    sendData(interest_response, ack_message);
}


void SNDSProducer::process_get_by_id_for_type_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    auto maybe_id = interest.get_interest_parameter(0);
    auto maybe_type = interest.get_interest_parameter(1);

    if(!maybe_id.has_value())
    {
        m_logger->warn("Received ID with no value in {}", __func__);
        return;
    }

    if(!maybe_type.has_value())
    {
        m_logger->warn("Received TYPE with not value in {}", __func__);
        return;
    }

    auto id = maybe_id.value();
    auto type = maybe_type.value();

    m_logger->info("ID for TYPE request received: id = {}, type = {}", id, type);
    // In local ID requests, we assume the name field itself is the requested ID.
    httplib::Client cli(broker_ip.c_str(), broker_port);
    std::string endpoint = fmt::format
    (
      	"/get?id={}",
        url_encode(id)
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
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        //sendData(interest_response, "dummy_payload");
    }

}

void SNDSProducer::process_temporal_query_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    m_logger->debug("Created interest response: {} in: {}", interest_response, __func__);
    // Overlay with key=value params (split BEFORE decoding to keep "%3D" inside values)
    // Build a map<string,string> out of interest.params.
    // Parameters are URL-encoded by convention.
    // Seed with positional fields (if present), then overlay key=value params.
    // Later entries override earlier ones.
    std::unordered_map<std::string, std::string> parameter_map;

    for (const auto& raw : interest.params)
    {
        const auto eq = raw.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key_enc = raw.substr(0, eq);
        const std::string val_enc = raw.substr(eq + 1);

        std::string key = ::url_decode(key_enc);
        std::string val = ::url_decode(val_enc);

        if (!key.empty()) parameter_map[std::move(key)] = std::move(val);
    }

    auto id        = parameter_map["id"];
    auto date      = parameter_map["date"];
    auto type      = parameter_map["type"];
    auto timeframe = parameter_map["timeframe"];
    auto count     = parameter_map["count"];
    auto filters   = parameter_map["filters"];

	m_logger->debug("TemporalQuery → id: {}, date: {}, type: {}, timeframe: {}, count: {}, filters: {}", id, date, type, timeframe, count, filters);
    httplib::Client cli(broker_ip.c_str(), broker_port);
    std::string endpoint = fmt::format
    (
 		"/temporal-query?id={}&type={}&count={}&date={}&timeframe={}&filters={}",
        url_encode(id),
        url_encode(type),
        count,
        url_encode(date),
        url_encode(timeframe),
        filters
    );

	m_logger->debug("Requesting to endpoint: {}", endpoint);
    auto res = cli.Get(endpoint.c_str());
    if (res && res->status == 200) {
        m_logger->info("Successfully queried /temporal-query");
        sendData(interest_response, nlohmann::json::parse(res->body).dump(2));
    } else {
        m_logger->error("Failed to query /temporal-query: {}", res ? res->body : "No response");
    }
}

void SNDSProducer::process_by_type_advertisement_interest(const parsed_interest_name::ParsedInterestName& interest) {
  	m_logger->info("Processing Type Advertisement");
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
  	// Attempt to get the advertised ID from the parsed parameters.
  	// TODO hopefully one ID received here
  	auto maybe_id = interest.get_interest_parameter(0);
  	auto maybe_type = interest.get_interest_parameter(1);

    if(!maybe_id.has_value())
    {
        m_logger->warn("Received ID with no value in {}", __func__);
        return;
    }

    if(!maybe_type.has_value())
    {
        m_logger->warn("Received type with not value in {}", __func__);
        return;
    }

    std::string id = maybe_id.value();
    std::string type = maybe_type.value();

  	m_logger->info("Type advertisement: name(registry) = {0}, ID = {1}", type, id);
  	// Look up the content type (with a preceding slash) in the registry map.
  	// Try to insert the ID; check if it was already present
    bool inserted = register_id_for_type(type_registry, events_, type, id);

  	if (!inserted) 
    {
  	  	m_logger->warn("ID already registered for type {}. Skipping...", type);
        auto warn_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::WARNING_ALREADY_REGISTERED);
  	  	sendData(interest_response, warn_message);
  	  	return;
  	}

  	m_logger->info("Registered new ID {0} for type {1}. Sending CONTROL.", id, type);
    auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_ID_REGISTERED);
  	sendData(interest_response, ack_message);
}


void SNDSProducer::process_new_subscription_interest(const parsed_interest_name::ParsedInterestName& interest) 
{

    auto interest_type      = interest.command;
    //we are responsible for the name
    auto our_snds_node_id   = interest.name;
    auto maybe_name                    = interest.get_interest_parameter(0);
    auto maybe_snds_node_id_subscriber = interest.get_interest_parameter(1);
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);


    m_logger->info("Received our_snds_node_id: {} in func: {}", our_snds_node_id, __func__);

    if (!maybe_name.has_value() || !maybe_snds_node_id_subscriber.has_value())
    {
        m_logger->error("In function {} got Name or SNDS Node ID with no value.", __func__);
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        return;
    }

    auto name = *maybe_name;
    auto snds_node_id_subscriber = *maybe_snds_node_id_subscriber;

    m_logger->info("Got interest indicating new subscription for name: {0}, and snds node: {1}", name, snds_node_id_subscriber);

    const bool by_id = (interest_type == parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_ID_SUBSCRIPTION);
    const char* kind = by_id ? "BY_ID" : "BY_TYPE";

    // Try to insert the ID into the set
    bool inserted = by_id
        ? subscribe_id(subscribed_nodes_for_id,   events_, name, snds_node_id_subscriber)
        : subscribe_type(subscribed_nodes_for_type, events_, name, snds_node_id_subscriber);

    if (inserted) 
    {
        m_logger->info("[{}] New subscription: {} subscribed to {}", kind, snds_node_id_subscriber, name);
        m_logger->info("[{}] Responding to interest: {}", kind, interest_response);
        auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_SUBSCRIPTION_NEW);
        sendData(interest_response, ack_message);
        return;
    }

    m_logger->info("Subscription: {} already exists for {}", snds_node_id_subscriber, name);
    m_logger->info("Responding to interest: {}", interest_response);
    auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_SUBSCRIPTION_EXISTS);
    sendData(interest_response, ack_message);
}

void SNDSProducer::process_get_by_type_interest(const parsed_interest_name::ParsedInterestName& interest) 
{
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    auto maybe_type = interest.get_interest_parameter(0);

    if(!maybe_type.has_value())
    {
        m_logger->warn("Received TYPE with no value in {}", __func__);
        return;
    }

    auto type = maybe_type.value();

  	// Look up the registry for the base type (the keys in type_registry are stored with a leading slash).
  	auto type_registry_iterator = type_registry.find(type);
  	if (type_registry_iterator == type_registry.end()) 
    {
  	  	m_logger->warn("Unknown registry for type {}.", type);
  	  	return;
  	}

  	std::vector<std::byte> byte_vector;
    std::vector<std::byte> body = serialize_registry_file_payload(type_registry_iterator->second);
    byte_vector.reserve(2 + body.size());
  	byte_vector.push_back(std::byte{'R'});  // registry response marker
  	byte_vector.push_back(std::byte{0x00}); // reserved/future use
  	byte_vector.insert(byte_vector.end(), body.begin(), body.end());

  	std::string payload(reinterpret_cast<const char*>(byte_vector.data()), byte_vector.size());

  	m_logger->info("Sending registry file payload for type {}.", type);
  	sendData(interest_response, payload);
}


void SNDSProducer::process_scope_notification_interest(const parsed_interest_name::ParsedInterestName& interest)
{

    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    // this name an ID like car1
    auto maybe_id = interest.get_interest_parameter(0);
    auto maybe_type = interest.get_interest_parameter(1);

    if (!maybe_id.has_value())
    {
        m_logger->error("Received interest in {} with incorrect value for parameter.", __func__);
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        return;
    }

    if (!maybe_type.has_value())
    {
        m_logger->warn("Received interest in {} with incorrect value for parameter.", __func__);
    }

    auto id = maybe_id.value();
    auto type = maybe_type.value_or("");

    auto type_of_subscription = type == "" ? "BY_ID" : "BY_TYPE";
    auto name = type_of_subscription == "BY_ID" ? id : type;
    const auto& container = type_of_subscription == "BY_ID" ? subscribed_nodes_for_id : subscribed_nodes_for_type;

    m_logger->info("[{}] Subscription notification for ID name {} and/or TYPE: {}.", type_of_subscription, id, type);

    auto iterator = container.find(name);
    if(iterator == container.end())
    {
        m_logger->info("No subscribers found for ID: {} and/or TYPE: {}.", id, type);
        m_logger->info("Responding to interest: {}", interest_response);
        //TODO proper ack messages
        auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_SUBSCRIPTION_NO_SUBSCRIBERS);
        sendData(interest_response, ack_message);
        return;
    }

    //TODO need to check
    const auto& subscribers = iterator->second;
    for (const auto& snds_node : subscribers) 
    {
        auto interest_name = parsed_interest_name::to_custom_ndn_name
        (
            parsed_interest_name::make_parsed_interest_name
            (
                snds_prefix, 
                snds_node,
                parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION,
                {id, type, snds_id_scope_name}
            )
        );

        m_logger->info("Notifying SNDS node: {}", interest_name);

        auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::move(interest_name),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION,
            snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER,
            "",
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );
        v_to_consumer->writeMessage(std::move(message));
    }
    //TODO proper ack meesages
    auto ack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::ACK_SUBSCRIPTION_NOTIFICATION);
    sendData(interest_response, ack_message);
}

void SNDSProducer::process_get_by_id_interest(const parsed_interest_name::ParsedInterestName& interest) 
{
    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    m_logger->debug("Created interest response: {} in {}", interest_response, __func__);
    auto maybe_id = interest.get_interest_parameter(0);

    if(!maybe_id.has_value())
    {
        m_logger->warn("Received ID with no value in {}", __func__);
        return;
    }

    auto id = maybe_id.value();

    m_logger->info("ID request received: id = {}", id);
    // In local ID requests, we assume the name field itself is the requested ID.
    httplib::Client cli(broker_ip.c_str(), broker_port);
    std::string endpoint = fmt::format
    (
      	"/get?id={}",
        url_encode(id)
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
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
        //sendData(interest_response, "dummy_payload");
    }

}



uint8_t SNDSProducer::_publish_to_broker(const nlohmann::json& doc)
{
    m_logger->info("Publishing to broker...");
    if (!doc.contains("id") || !doc["id"].is_string() || doc["id"].empty()) 
    {
        m_logger->error("{}: 'id' (string) is required", __func__);
        return -1;
    }

    if(!doc.contains("type") || !doc["type"].is_string() || doc["type"].empty())
    {
        m_logger->warn("{}: 'type' (string) not provided...", __func__);
    }

    httplib::Client cli(broker_ip.c_str(), broker_port);
    cli.set_read_timeout(5, 0);
    cli.set_connection_timeout(5, 0);

    const auto body = doc.dump();

    m_logger->info("POST /add -> {}", body);

    auto res = cli.Post("/add", body, "application/json");

    if (res && res->status == 200) 
    {
        m_logger->info("Published to broker OK: {}", res->body);
        //special case for updating state 
        m_logger->debug("Appending body to broker journal: {}", body);
        append_broker_journal_(doc);
        if (doc.contains("type") && doc["type"].is_string()) update_store_with_broker_state_(doc["type"].get_ref<const std::string&>());
        return 0;
    } 
    else 
    {
        m_logger->error("Failed to publish to broker: {}", res ? res->body : "No response");
        return -1;
    }
}

std::optional<nlohmann::json> SNDSProducer::_http_get_json
(
    std::string_view path,
    const std::vector<std::pair<std::string, std::string>>& q
)
{
    httplib::Client cli(broker_ip.c_str(), broker_port);
    cli.set_read_timeout(5, 0);
    cli.set_connection_timeout(5, 0);

    // Build query string with your url_encode()
    std::string endpoint(path);
    if (!q.empty()) 
    {
        endpoint += '?';
        bool first = true;
        for (auto& [k,v] : q) 
        {
            if (!first) endpoint += '&';
            endpoint += url_encode(k) + "=" + url_encode(v);
            first = false;
        }
    }

    m_logger->debug("HTTP GET {}", endpoint);
    auto res = cli.Get(endpoint.c_str());
    if (res && res->status == 200) 
    {
        try 
        {
            return nlohmann::json::parse(res->body);
        } 
        catch (const std::exception& e) 
        {
            m_logger->error("Failed to parse JSON from {}: {}", endpoint, e.what());
        }
    } 
    else 
    {
        m_logger->error("GET {} failed: {}", endpoint, res ? res->body : "No response");
    }

    return std::nullopt;
}


void SNDSProducer::onRegisterSuccess(const ndn::Name& prefix)
{
    m_logger->info("Successfully registered prefix: {}", prefix.toUri());
    announce(announcements, events_, prefix.toUri());
}

static std::string iso8601_now_utc() 
{
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::optional<nlohmann::json> SNDSProducer::fetch_broker_state_json_(const std::string& type) 
{
  // Reuse your existing HTTP helper; it already logs & parses
  return _http_get_json("/state", {{"type", type}});
}


void SNDSProducer::upsert_type_id_(const std::string& type, const std::string& id) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& types = ensure_obj(s, "types");
        auto& t     = types[type];               // auto-creates null
        if (!t.is_object()) t = nlohmann::json::object();
        auto& ids   = ensure_obj(t, "ids");
        ids[id] = true;                          // add/overwrite
    });
}

void SNDSProducer::erase_type_id_(const std::string& type, const std::string& id) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& types = ensure_obj(s, "types");
        auto itT = types.find(type);
        if (itT == types.end() || !itT->is_object()) return;
        auto& ids = ensure_obj(*itT, "ids");
        ids.erase(id);
        // optional cleanup if empty
        if (ids.empty()) itT->erase("ids");
        if (itT->empty()) types.erase(type);
    });
}

void SNDSProducer::upsert_sub_by_id_(const std::string& id, const std::string& node) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& subs = ensure_obj(s, "subscribers");
        auto& byId = ensure_obj(subs, "byId");
        auto& ent  = byId[id]; if (!ent.is_object()) ent = nlohmann::json::object();
        auto& nodes= ensure_obj(ent, "nodes");
        nodes[node] = true;
    });
}

void SNDSProducer::erase_sub_by_id_(const std::string& id, const std::string& node) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& subs = ensure_obj(s, "subscribers");
        auto& byId = ensure_obj(subs, "byId");
        auto it = byId.find(id);
        if (it == byId.end() || !it->is_object()) return;
        auto& nodes = ensure_obj(*it, "nodes");
        nodes.erase(node);
        if (nodes.empty()) it->erase("nodes");
        if (it->empty()) byId.erase(id);
    });
}

void SNDSProducer::upsert_sub_by_type_(const std::string& type, const std::string& node) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& subs = ensure_obj(s, "subscribers");
        auto& byT  = ensure_obj(subs, "byType");
        auto& ent  = byT[type]; if (!ent.is_object()) ent = nlohmann::json::object();
        auto& nodes= ensure_obj(ent, "nodes");
        nodes[node] = true;
    });
}

void SNDSProducer::erase_sub_by_type_(const std::string& type, const std::string& node) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& subs = ensure_obj(s, "subscribers");
        auto& byT  = ensure_obj(subs, "byType");
        auto it = byT.find(type);
        if (it == byT.end() || !it->is_object()) return;
        auto& nodes = ensure_obj(*it, "nodes");
        nodes.erase(node);
        if (nodes.empty()) it->erase("nodes");
        if (it->empty()) byT.erase(type);
    });
}

void SNDSProducer::store_announce_(const std::string& name) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& ann = ensure_obj(s, "announcements");
        ann[name] = true;
    });
}

void SNDSProducer::store_unannounce_(const std::string& name) 
{
    store_.mutate([&](nlohmann::json& s)
    {
        auto& ann = ensure_obj(s, "announcements");
        ann.erase(name);
    });
}

void SNDSProducer::update_store_with_broker_state_(const std::string& type) 
{
    const std::string effective_type = (type.empty() ? "GENERIC" : type);
    auto j = fetch_broker_state_json_(effective_type);
    if (!j) 
    {
        m_logger->warn("[broker] /state for type '{}' returned no JSON", type);
        return;
    }
    // Expect: collection, algo, hash, count
    store_.mutate([&](nlohmann::json& s)
    {
        auto& broker = ensure_obj(s, "broker");
        auto& cols   = ensure_obj(broker, "collections");
        cols[type] = {
            {"algo",  j->value("algo",  "")},
            {"hash",  j->value("hash",  "")},
            {"count", j->value("count", 0)},
            {"lastSeen", iso8601_now_utc()}
        };
        // keep endpoint around for observability
        if (!broker.contains("endpoint")) broker["endpoint"] = fmt::format("{}:{}", broker_ip, broker_port);
    });
}

void SNDSProducer::append_broker_journal_(const nlohmann::json& body)
{
    // Body must include at least id + type for deterministic replay.
    const std::string id   = j_get_string_or(body, "id", "");
    std::string       type = j_get_string_or(body, "type", "GENERIC");
    if (type.empty()) type = "GENERIC";

    if (id.empty()) 
    {
        m_logger->warn("[journal] skipping entry without 'id'");
        return;
    }

    store_.mutate([&](nlohmann::json& s)
    {
        auto& broker  = ensure_obj(s, "broker");
        auto& journal = ensure_arr(broker, "journal");

        // Can dedupe identical consecutive payloads cheaply:
        if (!journal.empty()) 
        {
            const auto& last = journal.back();
            if (last.contains("payload") && last["payload"] == body) 
            {
                m_logger->debug("[journal] duplicate payload -> skip");
                return;
            }
        }

        nlohmann::json entry = 
        {
            {"ts",      iso8601_now_utc()},
            {"type",    type},
            {"id",      id},
            {"payload", body}
        };

        journal.push_back(std::move(entry));

        if (journal.size() > broker_journal_max)
        {
            // drop oldest to cap growth
            const auto overflow = journal.size() - broker_journal_max;
            journal.erase(journal.begin(), journal.begin() + static_cast<nlohmann::json::difference_type>(overflow));
        }

        try 
        {
            m_logger->debug("[journal] current entries ({}):\n{}", journal.size(), journal.dump(2));
        } 
        catch (const std::exception& e) 
        {
            m_logger->error("[journal] failed to dump JSON: {}", e.what());
        }
    });
}


void SNDSProducer::rehydrate_broker_from_journal_()
{
    m_logger->warn("[rehydrate] starting Broker rehydration from journal…");

    // Snapshot journal to avoid mutating it while we POST
    const auto snap = store_.get_snapshot().to_json();
    const auto* broker = (snap.contains("state") && snap["state"].contains("broker")) ? &snap["state"]["broker"] : nullptr;
    if (!broker || !broker->is_object())
    {
        m_logger->warn("[rehydrate] no 'broker' section in state; nothing to replay");
        return;
    }

    const auto* journal = broker->contains("journal") ? &(*broker)["journal"] : nullptr;
    if (!journal || !journal->is_array() || journal->empty())
    {
        m_logger->warn("[rehydrate] journal empty; nothing to replay");
        return;
    }

    // POST each saved payload back to /add
    httplib::Client cli(broker_ip.c_str(), broker_port);
    cli.set_read_timeout(5, 0);
    cli.set_connection_timeout(5, 0);

    std::unordered_set<std::string> types_touched;

    size_t ok = 0, fail = 0;
    for (const auto& entry : *journal)
    {
        if (!entry.contains("payload")) continue;
        const auto& payload = entry["payload"];
        const auto  type    = payload.value("type", "GENERIC");

        auto res = cli.Post("/add", payload.dump(), "application/json");
        if (res && res->status == 200)
        {
            ok++;
            types_touched.insert(type);
        }
        else
        {
            fail++;
            m_logger->warn("[rehydrate] POST /add failed for id='{}' type='{}' : {}", payload.value("id",""), type, res ? std::to_string(res->status) : "no response");
        }
    }

    // Refresh fingerprints for all types we touched
    for (const auto& t : types_touched) update_store_with_broker_state_(t);
    m_logger->warn("[rehydrate] completed: ok={} fail={}", ok, fail);
}