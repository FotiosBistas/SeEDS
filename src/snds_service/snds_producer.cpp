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
        debug_dump_state_(fmt::format("event:type_add {}->{}", type, id));
        debug_dump_overlay_(fmt::format("event:type_add {}->{}", type, id));
    };

    events_.on_type_remove = [this](const std::string& type, const std::string& id) 
    {
        m_logger->info("[event] type_remove: {} -> {}", type, id);
        erase_type_id_(type, id);
        debug_dump_state_(fmt::format("event:type_remove {}->{}", type, id));
        debug_dump_overlay_(fmt::format("event:type_remove {}->{}", type, id));
    };

    events_.on_subscribe_id = [this](const std::string& id, const std::string& node) 
    {
        m_logger->info("[event] subscribe_id: node={} -> id={}", node, id);
        upsert_sub_by_id_(id, node);
        debug_dump_state_(fmt::format("event:on_subscribe_id {}->{}", node, id));
        debug_dump_overlay_(fmt::format("event:on_subscribe_id {}->{}", node, id));
    };

    events_.on_unsubscribe_id = [this](const std::string& id, const std::string& node) 
    {
        m_logger->info("[event] unsubscribe_id: node={} from id={}", node, id);
        erase_sub_by_id_(id, node);
        debug_dump_state_(fmt::format("event:on_unsubscribe_id {}->{}", node, id));
        debug_dump_overlay_(fmt::format("event:on_unsubscribe_id {}->{}", node, id));
    };

    events_.on_subscribe_type = [this](const std::string& type, const std::string& node) 
    {
        m_logger->info("[event] subscribe_type: node={} -> type={}", node, type);
        upsert_sub_by_type_(type, node);
        debug_dump_state_(fmt::format("event:on_subscribe_type {}->{}", node, type));
        debug_dump_overlay_(fmt::format("event:on_subscribe_type {}->{}", node, type));
    };

    events_.on_unsubscribe_type = [this](const std::string& type, const std::string& node) 
    {
        m_logger->info("[event] unsubscribe_type: node={} from type={}", node, type);
        erase_sub_by_type_(type, node);
        debug_dump_state_(fmt::format("event:on_unsubscribe_type {}->{}", node, type));
        debug_dump_overlay_(fmt::format("event:on_unsubscribe_type {}->{}", node, type));
    };

    events_.on_announce_add = [this](const std::string& name) 
    {
        m_logger->info("[event] announce_add: {}", name);
        store_announce_(name);
        debug_dump_state_(fmt::format("event:on_announce_add {}", name));
        debug_dump_overlay_(fmt::format("event:on_announce_add {}", name));
    };

    events_.on_announce_remove = [this](const std::string& name) 
    {
        m_logger->info("[event] announce_remove: {}", name);
        store_unannounce_(name);
        debug_dump_state_(fmt::format("event:on_announce_remove {}", name));
        debug_dump_overlay_(fmt::format("event:on_announce_remove {}", name));
    };

    if (enable_resilience_ipc_ && !am_primary_) 
    {
        start_resilience_ipc_();
    }
    else
    {
        m_logger->info("Not starting resilience IPC...");
    }

    maybe_rehydrate_broker_(/*type unused*/"");

    if(!am_primary_)
    {
        // One-time bootstrap: mirror current Broker data into secondary (or copy 1:1)
        // Useful when primary & secondary are the same Broker during debugging.
        seed_secondary_from_broker_dump_();
    }
    else
    {
        m_logger->info("Not copying the data from primary to secondary.");
    }


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

    // Run the loop until 'running' is false.
    while (running.load())
    {
        // m_logger->debug("Reading packet...");

        if (promotion_tick_.exchange(false)) 
        {
            m_logger->warn("[resilience] applying promotion side-effects on main loop");

            // Make the effective state canonical (single bump) and clear overlay
            squash_overlay_into_base_and_clear_();

            // Stop IPC if still running
            if (resilience_ipc_running_.load()) 
            {
                m_logger->warn("[resilience-ipc] stopping IPC listener (promotion)");
                stop_resilience_ipc_();
            }

            // Rebuild RAM & (re)announce using the canonical base
            inherit_from_store_after_promotion_();
        }


        if (am_primary_.load() && resilience_ipc_running_.load()) 
        {
            m_logger->warn("[resilience-ipc] stopping IPC listener (became PRIMARY)");
            stop_resilience_ipc_(); // safe here; runs on a different thread than the IPC server
        }

        // Continuous warm-up: when secondary receives a patch/snapshot, replay new journal items
        if (!am_primary_.load() && rehydrate_tick_.exchange(false)) 
        {
            replay_broker_journal_incremental_();
        }
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
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                "", 
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_ADVERTISEMENT,
                snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER,
                std::move(warn_message),
                snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
            );

            v_to_worker->writeMessage(std::move(message));
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


    nlohmann::json doc = 
    {
        {"id",      parsed_interest.name},
    };
    _publish_to_broker(doc);

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
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_SNAPSHOT:
        process_resilience_snapshot_interest(pi);
        break;
    case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_PATCH:
        process_resilience_patch_interest(pi);
        break;
    default:
        break;
    }

}

void SNDSProducer::process_resilience_snapshot_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    const auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, /*as_data_name*/ true);

    // param: to_version
    const auto maybe_to = interest.get_interest_parameter(0);

    // latest version from the store (authoritative head)
    const uint64_t latest_version = store_.get_meta().version;

    if (!maybe_to.has_value())
    {
        nlohmann::json body = {{"error","missing_parameters"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    uint64_t to_version = 0;
    try
    {
        to_version = std::stoull(*maybe_to);
    }
    catch (...)
    {
        nlohmann::json body = {{"error","invalid_parameters"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    // We serve snapshots for our current head only.
    if (to_version != latest_version)
    {
        nlohmann::json body = {{"error","unknown_target_version"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    // Build snapshot of the current state
    const auto snap = store_.get_snapshot();

    // Race safety: ensure the store did not advance after we checked 'latest_version'
    if (snap.version != to_version)
    {
        const uint64_t refreshed_latest = store_.get_meta().version;
        nlohmann::json body = {{"error","store_advanced"}, {"advice","fetch_snapshot"}, {"latest",refreshed_latest}};
        sendData(interest_response, body.dump());
        return;
    }

    // Success: send snapshot as JSON (kind=snapshot, version/hash/state)
    const std::string payload = snap.to_json().dump();
    sendData(interest_response, payload);
}

void SNDSProducer::process_resilience_is_alive_interest(const parsed_interest_name::ParsedInterestName &interest)
{

    auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);

    const auto maybe_nonce = interest.get_interest_parameter(0);
    if (!maybe_nonce) 
    {
        m_logger->warn("[isAlive] missing nonce");
        nlohmann::json j = 
        { 
            {"ack", false}, 
            {"reason","missing-nonce"} 
        };
        sendData(interest_response, j.dump());
        return;
    }

    nlohmann::json j = 
    {
        {"ack",   true},
        {"nonce", *maybe_nonce},
        {"ts",    iso8601_now_utc()}
    };

    sendData(interest_response, j.dump());
}

void SNDSProducer::process_resilience_patch_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    const auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, /*as_data_name*/ true);

    // parameters: from_version, to_version
    const auto maybe_from = interest.get_interest_parameter(0);
    const auto maybe_to   = interest.get_interest_parameter(1);

    // latest version from the authoritative store
    const uint64_t latest_version = store_.get_meta().version;

    if (!maybe_from.has_value() || !maybe_to.has_value()) 
    {
        nlohmann::json body = {{"error","missing_parameters"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    uint64_t from_version = 0;
    uint64_t to_version   = 0;
    try 
    {
        from_version = std::stoull(*maybe_from);
        to_version   = std::stoull(*maybe_to);
    } 
    catch (...) 
    {
        nlohmann::json body = {{"error","invalid_parameters"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    if (to_version <= from_version) 
    {
        nlohmann::json body = {{"error","non_increasing_version"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    // We serve patches only to our current head (simple, deterministic contract).
    // If the requested target differs, tell the consumer what to aim for.
    if (to_version != latest_version) 
    {
        nlohmann::json body = {{"error","unknown_target_version"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    // Build patch from 'from_version' → CURRENT (latest_version)
    auto patch_opt = store_.make_patch_from(from_version);
    if (!patch_opt.has_value()) 
    {
        // likely too old; our history window dropped that base
        nlohmann::json body = {{"error","patch_unavailable"}, {"advice","fetch_snapshot"}, {"latest",latest_version}};
        sendData(interest_response, body.dump());
        return;
    }

    // Safety: store may have advanced between checks; re-validate target
    if (patch_opt->target_version != to_version) 
    {
        const uint64_t refreshed_latest = store_.get_meta().version;
        nlohmann::json body = {{"error","store_advanced"}, {"advice","fetch_snapshot"}, {"latest",refreshed_latest}};
        sendData(interest_response, body.dump());
        return;
    }

    // Success: send the patch as JSON (kind=patch, with ops/hash/versions)
    const std::string payload = patch_opt->to_json().dump();
    sendData(interest_response, payload);
}

void SNDSProducer::process_resilience_meta_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    using namespace ndn::time_literals; // enables 0_ms, 50_ms, etc.

    try 
    {
        // Build the response name exactly as your consumer expects
        auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);

        // Gather current snapshot (version/hash/state)
        const auto snap = store_.get_snapshot();

        // Build the meta JSON
        nlohmann::json meta = 
        {
            {"version", snap.version},
            {"hash",    snap.hash},
            {"ts",      iso8601_now_utc()}
        };

        meta["state_name"] = snds_state_with_id; // e.g. "state_0"

        const std::string payload = meta.dump(); // serialize JSON once

        // Prepare Data
        auto data = std::make_shared<ndn::Data>();
        data->setName(interest_response);
        // Set to 0 ms so CS does not serve cached copies unless consumer allows stale
        //TODO tweak this so we don't overwhelm the network
        data->setFreshnessPeriod(30_ms);
        data->setContent(std::move(payload));
        // Sign & send
        m_keyChain.sign(*data);
        m_face.put(*data);
    }
    catch (const std::exception& e) 
    {
        spdlog::warn("[producer] meta interest handling failed: {}", e.what());
    }
}

void SNDSProducer::process_version_metafile_interest(const parsed_interest_name::ParsedInterestName& interest)
{
    const auto interest_response = parsed_interest_name::to_custom_ndn_name(interest, true);
    m_logger->debug("Created interest response: {} in {}", interest_response, __func__);

    // Parse all k=v params into a map
    const auto params = parsed_interest_name::parse_params_map(interest);

    // id (required)
    auto id_it = params.find("id");
    if (id_it == params.end() || id_it->second.empty()) 
    {
        m_logger->error("Received metafile Interest without a valid 'id' parameter");
        return;
    }
    const std::string& id = id_it->second;

    // type (optional)
    std::string type_param;
    if (auto t = params.find("type"); t != params.end() && !t->second.empty()) 
    {
        type_param = t->second;
    }

    m_logger->info("Metafile request received: id={}, type='{}'", id, type_param);

    // Build /get endpoint
    std::string endpoint = "/get?id=" + url_encode(id);
    if (!type_param.empty()) 
    {
        endpoint += "&type=" + url_encode(type_param);
    }

    httplib::Client cli(broker_ip.c_str(), broker_port);
    auto res = cli.Get(endpoint.c_str());

    if (res && res->status == 200)
     {
        m_logger->info("Successfully queried {}", endpoint);
        // TODO: sign
        sendData(interest_response, res->body);
    } 
    else 
    {
        m_logger->error("Failed to query {}: {}", endpoint, res ? res->body : "No response");
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
        sendData(interest_response, nack_message);
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
        //sendData(interest_response, R"({
        //    "mode": "direct",
        //    "items": [
        //        {
        //            "id": "dummy_id",
        //            "type": "DUMMY_TYPE",
        //            "count": 0,
        //            "date": "1970-01-01T00:00:00Z",
        //            "timeframe": "before after"
        //        }
        //    ]
        //    })"
        //);
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
        url_encode(filters)
    );

	m_logger->debug("Requesting to endpoint: {}", endpoint);
    auto res = cli.Get(endpoint.c_str());
    if (res && res->status == 200) {
        m_logger->info("Successfully queried /temporal-query");
        sendData(interest_response, nlohmann::json::parse(res->body).dump(2));
    } else {
        m_logger->error("Failed to query /temporal-query: {}", res ? res->body : "No response");
        auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::NACK_ERROR);
  	  	//sendData(interest_response, nack_message);
        //sendData(interest_response, R"({
        //    "mode": "direct",
        //    "items": [
        //        {
        //            "id": "dummy_id",
        //            "type": "DUMMY_TYPE",
        //            "count": 0,
        //            "date": "1970-01-01T00:00:00Z",
        //            "timeframe": "before after"
        //        }
        //    ]
        //    })"
        //);
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
        //sendData(interest_response, R"({
        //    "mode": "direct",
        //    "items": [
        //        {
        //            "id": "dummy_id",
        //            "type": "DUMMY_TYPE",
        //            "count": 0,
        //            "date": "1970-01-01T00:00:00Z",
        //            "timeframe": "before after"
        //        }
        //    ]
        //    })"
        //);
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
    m_logger->info("Getting from IP: {} and port: {}", broker_ip, broker_port);
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

std::optional<nlohmann::json> SNDSProducer::fetch_broker_state_json_(const std::string& type) 
{
    return _http_get_json("/state", {{"type", type}});
}


void SNDSProducer::upsert_type_id_(const std::string& type, const std::string& id) 
{
    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
        {
            auto& types = ensure_obj(s, "types");
            auto& t     = types[type]; if (!t.is_object()) t = nlohmann::json::object();
            auto& ids   = ensure_obj(t, "ids");
            ids[id] = true;
        });
    } 
    else 
    {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["reg_add"][type].push_back(id);
            }
        );
    }
}

void SNDSProducer::erase_type_id_(const std::string& type, const std::string& id) 
{
    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
            {
                auto& types = ensure_obj(s, "types");
                auto itT = types.find(type);
                if (itT == types.end() || !itT->is_object()) return;
                auto& ids = ensure_obj(*itT, "ids");
                ids.erase(id);
                if (ids.empty()) itT->erase("ids");
                if (itT->empty()) types.erase(type);
            }
        );
    } 
    else 
    {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["reg_del"][type].push_back(id);
            }
        );
    }
}

void SNDSProducer::upsert_sub_by_id_(const std::string& id, const std::string& node) 
{
    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
            {
                auto& subs = ensure_obj(s, "subscribers");
                auto& byId = ensure_obj(subs, "byId");
                auto& ent  = byId[id]; if (!ent.is_object()) ent = nlohmann::json::object();
                auto& nodes= ensure_obj(ent, "nodes");
                nodes[node] = true;
            }
        );
    } 
    else 
    {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["sub_add"]["byId:" + id].push_back(node);
            }
        );
    }
}

void SNDSProducer::erase_sub_by_id_(const std::string& id, const std::string& node) 
{
    if (am_primary_.load()) 
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
            }
        );
    } else {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["sub_del"]["byId:" + id].push_back(node);
            }
        );
    }
}

void SNDSProducer::upsert_sub_by_type_(const std::string& type, const std::string& node) {
    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
            {
                auto& subs = ensure_obj(s, "subscribers");
                auto& byT  = ensure_obj(subs, "byType");
                auto& ent  = byT[type]; if (!ent.is_object()) ent = nlohmann::json::object();
                auto& nodes= ensure_obj(ent, "nodes");
                nodes[node] = true;
            }
        );
    } else {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["sub_add"]["byType:" + type].push_back(node);
            }
        );
    }
}

void SNDSProducer::erase_sub_by_type_(const std::string& type, const std::string& node) {
    if (am_primary_.load()) 
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
            }
        );
    } 
    else 
    {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["sub_del"]["byType:" + type].push_back(node);
            }
        );
    }
}

void SNDSProducer::store_announce_(const std::string& name) {
    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
            {
                auto& ann = ensure_obj(s, "announcements");
                ann[name] = true;
            }
        );
    } else {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["ann_add"].push_back(name);
            }
        );
    }
}

void SNDSProducer::store_unannounce_(const std::string& name) {
    if (am_primary_.load()) {
        store_.mutate([&](nlohmann::json& s)
            {
                auto& ann = ensure_obj(s, "announcements");
                ann.erase(name);
            }
        );
    } 
    else 
    {
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["ann_del"].push_back(name);
            }
        );
    }
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

    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
            {
                auto& broker = ensure_obj(s, "broker");
                auto& cols   = ensure_obj(broker, "collections");
                cols[effective_type] = 
                {
                    {"algo",     j->value("algo",  "")},
                    {"hash",     j->value("hash",  "")},
                    {"count",    j->value("count", 0)},
                    {"lastSeen", iso8601_now_utc()}
                };
                if (!broker.contains("endpoint")) broker["endpoint"] = fmt::format("{}:{}", broker_ip, broker_port);
            }
        );
    } 
    else 
    {
        // keep these in overlay until promotion
        overlay_mutate_([&](nlohmann::json& o)
            {
                o["broker_collections_upd"][effective_type] = 
                {
                    {"algo",     j->value("algo",  "")},
                    {"hash",     j->value("hash",  "")},
                    {"count",    j->value("count", 0)},
                    {"lastSeen", iso8601_now_utc()}
                };
                o["broker_endpoint"] = fmt::format("{}:{}", broker_ip, broker_port);
            }
        );
    }
}

void SNDSProducer::append_broker_journal_(const nlohmann::json& body)
{
    const std::string id   = j_get_string_or(body, "id", "");
    std::string       type = j_get_string_or(body, "type", "GENERIC");
    if (id.empty()) { m_logger->warn("[journal] skipping entry without 'id'"); return; }
    if (type.empty()) type = "GENERIC";

    if (am_primary_.load()) 
    {
        store_.mutate([&](nlohmann::json& s)
        {
            auto& broker  = ensure_obj(s, "broker");
            auto& journal = ensure_arr(broker, "journal");

            if (!journal.empty()) 
            {
                const auto& last = journal.back();
                if (last.contains("payload") && last["payload"] == body) 
                {
                    m_logger->debug("[journal] duplicate payload -> skip");
                    return;
                }
            }

            journal.push_back(
                {
                    {"ts",      iso8601_now_utc()},
                    {"type",    type},
                    {"id",      id},
                    {"payload", body}
                }
            );

            if (journal.size() > broker_journal_max) 
            {
                const auto overflow = journal.size() - broker_journal_max;
                journal.erase(journal.begin(), journal.begin() + static_cast<nlohmann::json::difference_type>(overflow));
            }
        });
    } 
    else 
    {
        // keep appended entries in overlay until promotion
        overlay_mutate_([&](nlohmann::json& o)
            {
                auto& add = o["broker_journal_add"];
                if (!add.is_array()) add = nlohmann::json::array();
                // optional dedupe against last overlay entry
                if (!add.empty() && add.back().contains("payload") && add.back()["payload"] == body) return;

                add.push_back(
                    {
                        {"ts",      iso8601_now_utc()},
                        {"type",    type},
                        {"id",      id},
                        {"payload", body}
                    }
                );
                if (add.size() > broker_journal_max) 
                {
                    const auto overflow = add.size() - broker_journal_max;
                    add.erase(add.begin(), add.begin() + static_cast<nlohmann::json::difference_type>(overflow));
                }
            }
        );
    }
}


void SNDSProducer::start_resilience_ipc_() 
{
    if (resilience_ipc_running_.exchange(true)) return;

    resilience_ipc_thr_ = std::thread([this]{
        try 
        {
            boost::asio::io_context ioc;
            boost::asio::ip::tcp::acceptor acc(ioc, {boost::asio::ip::make_address(resilience_ipc_host_), resilience_ipc_port_});

            m_logger->info("[resilience-ipc] listening on {}:{}", resilience_ipc_host_, resilience_ipc_port_);

            while (resilience_ipc_running_.load()) 
            {
                boost::asio::ip::tcp::socket sock{ioc};
                acc.accept(sock);  // blocks until a client connects

                m_logger->info("[resilience-ipc] Client connected...");

                // 4-byte big-endian length (capped at 32 MiB)
                char hdr[4];
                boost::asio::read(sock, boost::asio::buffer(hdr,4));
                const uint32_t n = snds_ipc_tcp::read_be32(hdr);
                if (n == 0 || n > (32u << 20)) { sock.close(); continue; }

                std::string body(n, '\0');
                boost::asio::read(sock, boost::asio::buffer(body.data(), body.size()));
                sock.close();

                // envelope: { "kind": "...", "payload": {...} }
                const auto env     = nlohmann::json::parse(body);
                const auto kind    = env.value("kind", std::string{});
                const auto payload = env.at("payload");

                m_logger->info("[resilience-ipc] Found message type: {}", kind);
                if (kind == "snapshot") 
                {
                    store_.assign(payload.at("state"));
                    if (!am_primary_.load()) 
                    {
                        rebase_overlay_onto_base_after_remote_apply_({});
                        rehydrate_tick_.store(true);
                    }
                } 
                else if (kind == "patch") 
                {
                    auto snap      = store_.get_snapshot();
                    auto new_state = snap.state.patch(payload.at("ops"));
                    store_.assign(std::move(new_state));
                    if (!am_primary_.load()) 
                    {
                        rebase_overlay_onto_base_after_remote_apply_({});
                        rehydrate_tick_.store(true);
                    }
                }
                else if (kind == "meta") 
                {
                    m_logger->debug("[resilience-ipc] meta v={} hash={}", payload.value("version", 0ULL), payload.value("hash", ""));
                }
                else if (kind == "role")
                {
                    const std::string role   = payload.value("role",   "");
                    const std::string reason = payload.value("reason", "");
                    const uint32_t    misses = payload.value("misses", 0U);
                
                    if (role == "primary") 
                    {
                        m_logger->warn("[resilience-ipc] promotion to PRIMARY (reason={}, misses={})", reason, misses);

                        // Persist role for observability
                        store_.mutate([&](nlohmann::json& s)
                        {
                            auto& r = ensure_obj(s, "resilience");
                            r["role"]   = "primary";
                            r["since"]  = iso8601_now_utc();
                            r["reason"] = reason;
                            r["misses"] = misses;
                        });

                        am_primary_.store(true);
                        promotion_tick_.store(true);
                        // IMPORTANT: do NOT call stop_resilience_ipc_() here (we're on the IPC thread).
                        // Just request stop; the loop will exit after this iteration.
                        resilience_ipc_running_.store(false);
                        m_logger->warn("[resilience-ipc] stop requested; will be joined from main loop");
                    }
                        else if (role == "secondary") 
                        {
                            m_logger->warn("[resilience-ipc] demoted to SECONDARY (reason={})", reason);
                            store_.mutate([&](nlohmann::json& s)
                            {
                                auto& r = ensure_obj(s, "resilience");
                                r["role"]   = "secondary";
                                r["since"]  = iso8601_now_utc();
                                r["reason"] = reason;
                            });
                            am_primary_.store(false);
                        } 
                    else 
                    {
                        m_logger->warn("[resilience-ipc] unknown role='{}'", role);
                    }
                }
                else 
                {
                    m_logger->warn("[resilience-ipc] unknown kind='{}'", kind);
                }
            }
        } 
        catch (const std::exception& e) 
        {
            m_logger->warn("[resilience-ipc] server error: {}", e.what());
        }
    });
}

void SNDSProducer::stop_resilience_ipc_() {
    if (!resilience_ipc_running_.exchange(false)) return;
    // Nudge accept() to unblock so the thread can exit
    try 
    {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::socket poke{ioc};
        poke.connect({boost::asio::ip::make_address(resilience_ipc_host_), resilience_ipc_port_});
    } 
    catch (...) {} // best-effort
    if (resilience_ipc_thr_.joinable()) resilience_ipc_thr_.join();
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

            if (mirror_to_secondary_) 
            {
                nlohmann::json payload_secondary = payload;  // copy
                const std::string secondary_type = to_secondary_collection_(type, secondary_suffix_);
                payload_secondary["type"] = secondary_type;

                auto res2 = cli.Post("/add", payload_secondary.dump(), "application/json");
                if (res2 && res2->status == 200) 
                {
                    types_touched.insert(secondary_type);
                } 
                else 
                {
                    m_logger->warn("[rehydrate][mirror] POST /add to secondary collection '{}' failed: {}", secondary_type, res2 ? std::to_string(res2->status) : "no response");
                }
            }
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

    if (fail == 0) 
    {
        // Re-read the same logical state to compute size and last fp
        const auto snap2  = store_.get_snapshot().to_json();
        const auto* broker2  = (snap2.contains("state") && snap2["state"].contains("broker")) ? &snap2["state"]["broker"] : nullptr;
        const auto* journal2 = (broker2 && broker2->contains("journal")) ? &(*broker2)["journal"] : nullptr;

        if (journal2 && journal2->is_array() && !journal2->empty()) 
        {
            broker_journal_cursor_ = journal2->size();
            last_replayed_fp_      = fingerprint_entry_((*journal2)[journal2->size() - 1]);
            m_logger->info("[rehydrate] advanced cursor to {} and set last fingerprint", broker_journal_cursor_);
        }
    }
}

void SNDSProducer::replay_broker_journal_incremental_()
{
    const auto snap = store_.get_snapshot().to_json();
    const auto* broker  = (snap.contains("state") && snap["state"].contains("broker")) ? &snap["state"]["broker"] : nullptr;
    const auto* journal = (broker && broker->contains("journal")) ? &(*broker)["journal"] : nullptr;

    if (!journal || !journal->is_array()) 
    {
        m_logger->warn("[rehydrate] no broker.journal to replay");
        return;
    }

    // If the journal was truncated (cap), resync cursor by fingerprint if possible.
    size_t start = broker_journal_cursor_;
    if (start > journal->size()) start = 0;

    if (!last_replayed_fp_.empty()) 
    {
        bool found = false;
        for (size_t i = (start > 0 ? start - 1 : 0); i < journal->size(); ++i) 
        {
            const auto fp = fingerprint_entry_((*journal)[i]);
            if (fp == last_replayed_fp_) 
            {
                start = i + 1;   // pick up right after last known entry
                found = true;
                break;
            }
        }
        if (!found) 
        {
            // Could not find our anchor (likely old entries dropped) → start from 0
            start = 0;
        }
    }

    if (start >= journal->size()) 
    {
        m_logger->debug("[rehydrate] up to date (cursor={}, size={})", broker_journal_cursor_, journal->size());
        return;
    }

    httplib::Client cli(broker_ip.c_str(), broker_port);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(3, 0);

    size_t ok = 0, fail = 0;
    std::unordered_set<std::string> types_touched;

    for (size_t i = start; i < journal->size(); ++i) 
    {
        const auto& entry = (*journal)[i];
        if (!entry.contains("payload")) continue;

        const auto& payload = entry["payload"];
        const std::string type = payload.value("type", "GENERIC");

        auto res = cli.Post("/add", payload.dump(), "application/json");
        if (res && res->status == 200) 
        {
            ok++;
            types_touched.insert(type);
            if (mirror_to_secondary_) 
            {
                nlohmann::json payload_secondary = payload;               // copy
                const std::string secondary_type = to_secondary_collection_(type, secondary_suffix_);
                payload_secondary["type"] = secondary_type;

                auto res2 = cli.Post("/add", payload_secondary.dump(), "application/json");
                if (!(res2 && res2->status == 200)) 
                {
                    m_logger->warn("[rehydrate][mirror] POST /add to secondary collection '{}' failed at idx={} id='{}': {}", secondary_type, i, payload.value("id",""), res2 ? std::to_string(res2->status) : "no response");
                } 
                else 
                {
                    types_touched.insert(secondary_type); // refresh broker fingerprints for the mirrored collection too
                }
            }
            last_replayed_fp_       = fingerprint_entry_(entry);
            broker_journal_cursor_  = i + 1; // advance only on success
        } 
        else 
        {
            fail++;
            m_logger->warn("[rehydrate] POST /add failed at idx={} id='{}' type='{}' : {}", i, payload.value("id",""), type, res ? std::to_string(res->status) : "no response");
            break; // back off; we'll retry from same cursor later
        }
    }

    for (const auto& t : types_touched) 
    {
        update_store_with_broker_state_(t);
    }

    m_logger->info("[rehydrate] incremental replay done: ok={} fail={} cursor={}/{}", ok, fail, broker_journal_cursor_, journal->size());
}

void SNDSProducer::maybe_rehydrate_broker_(const std::string& type/*type*/)
{
    // If we're secondary and we already have a broker journal in the store,
    // replay it immediately so our local broker is filled at startup.
    if (am_primary_.load()) 
    {
        return; // primary doesn't need to replay
    }

    const auto snap = store_.get_snapshot().to_json();
    const auto* broker  = (snap.contains("state") && snap["state"].contains("broker")) ? &snap["state"]["broker"] : nullptr;
    const auto* journal = (broker && broker->contains("journal")) ? &(*broker)["journal"] : nullptr;

    if (!journal || !journal->is_array() || journal->empty()) 
    {
        m_logger->info("[rehydrate] no journal present yet; will re-check after snapshot/patch");
        return;
    }

    // On first boot (cursor==0 and no fingerprint), do a full fill.
    if (broker_journal_cursor_ == 0 && last_replayed_fp_.empty()) 
    {
        m_logger->warn("[rehydrate] bootstrapping local broker from existing journal ({} entries)", journal->size());
        rehydrate_broker_from_journal_();
    } else 
    {
        // If something already ran, just do an incremental pass.
        m_logger->info("[rehydrate] running incremental replay at startup (cursor={}, size={})", broker_journal_cursor_, journal->size());
        replay_broker_journal_incremental_();
    }
}

 void SNDSProducer::seed_secondary_from_broker_dump_()
 {
    m_logger->warn("[seed] bootstrap via /dump → /copy (src={}:{}, dst(local)={}:{}, mirror_to_secondary_={}, suffix='{}')", seed_from_broker_ip_, seed_from_broker_port_, broker_ip, broker_port, mirror_to_secondary_, secondary_suffix_);

    // GET /dump from the PRIMARY (seed source), not from ourselves.
    httplib::Client src(seed_from_broker_ip_.c_str(), seed_from_broker_port_);
    src.set_read_timeout(60, 0);
    src.set_connection_timeout(5, 0);
    auto dump_res = src.Get("/dump");
    if (!(dump_res && dump_res->status == 200)) 
    {
        m_logger->warn("[seed] source /dump failed from {}:{} : {}", seed_from_broker_ip_, seed_from_broker_port_, dump_res ? std::to_string(dump_res ->status) : "no response");
        return;
    }

    nlohmann::json dump_json;
    try 
    {
        dump_json = nlohmann::json::parse(dump_res->body);
    } 
    catch (const std::exception& e) 
    {
        m_logger->warn("[seed] invalid JSON from source /dump: {}", e.what());
        return;
    }

    // /dump might return either { "collections": { ... } } or just { ... }.
    nlohmann::json collections = (dump_json.contains("collections") && dump_json["collections"].is_object()) ? dump_json["collections"] : dump_json;

    nlohmann::json body = 
    {
        {"mirror_to_secondary", mirror_to_secondary_},
        {"secondary_suffix",    secondary_suffix_},
        {"collections",         collections}
    };

    // POST to our LOCAL broker /copy (destination).
    httplib::Client dst(broker_ip.c_str(), broker_port);
    dst.set_read_timeout(60, 0);
    dst.set_connection_timeout(5, 0);

    auto res = dst.Post("/copy", body.dump(), "application/json");
    if (res && res->status == 200) 
    {
        m_logger->warn("[seed] /copy OK: {}", res->body);
    } 
    else 
    {
        m_logger->error("[seed] /copy failed: {}", res ? res->body : "no response");
    }
 }


void SNDSProducer::rebuild_containers_from_store_()
{
    const auto snap = store_.get_snapshot().to_json();
    if (!snap.contains("state") || !snap["state"].is_object()) return;
    const auto& st = snap["state"];

    // ---- TYPES (ids stored as object: {id:true}) ----
    if (auto it = st.find("types"); it != st.end() && it->is_object())
    {
        for (auto& [type, obj] : it->items())
        {
            if (!obj.is_object()) continue;
            if (obj.contains("ids") && obj["ids"].is_object())
            {
                auto& ids_set = type_registry[type];
                for (auto& [id, _] : obj["ids"].items())
                    ids_set.insert(id);
            }
        }
    }

    // ---- SUBSCRIBERS ----
    if (auto it = st.find("subscribers"); it != st.end() && it->is_object())
    {
        // byId[*].nodes can be {node:true,...} (object) or ["node",...] (array)
        if (auto byId = it->find("byId"); byId != it->end() && byId->is_object())
        {
            for (auto& [id, obj] : byId->items())
            {
                if (!obj.is_object()) continue;
                auto nodesIt = obj.find("nodes");
                if (nodesIt == obj.end()) continue;

                auto& dst = subscribed_nodes_for_id[id];

                if (nodesIt->is_object()) {
                    for (auto& [node, _] : nodesIt->items())
                        dst.insert(node);
                } else if (nodesIt->is_array()) {
                    for (auto& n : *nodesIt)
                        if (n.is_string()) dst.insert(n.get<std::string>());
                }
            }
        }

        // byType[*].nodes can be {node:true,...} or ["node",...]
        if (auto byType = it->find("byType"); byType != it->end() && byType->is_object())
        {
            for (auto& [type, obj] : byType->items())
            {
                if (!obj.is_object()) continue;
                auto nodesIt = obj.find("nodes");
                if (nodesIt == obj.end()) continue;

                auto& dst = subscribed_nodes_for_type[type];

                if (nodesIt->is_object()) {
                    for (auto& [node, _] : nodesIt->items())
                        dst.insert(node);
                } else if (nodesIt->is_array()) {
                    for (auto& n : *nodesIt)
                        if (n.is_string()) dst.insert(n.get<std::string>());
                }
            }
        }
    }

    // ---- ANNOUNCEMENTS ----
    // Stored by store_announce_() as object: {name:true}, but accept array too for compat.
    if (auto it = st.find("announcements"); it != st.end())
    {
        if (it->is_object()) {
            for (auto& [name, _] : it->items())
                announcements.insert(name);
        } else if (it->is_array()) {
            for (auto& n : *it)
                if (n.is_string()) announcements.insert(n.get<std::string>());
        }
    }

    // ---- DEBUG OUTPUT (full dumps) ----
    m_logger->debug("[promotion] rebuilt containers from store:");
    for (const auto& [t, ids] : type_registry) {
        std::ostringstream oss; for (auto& id : ids) oss << id << " ";
        m_logger->debug("  type_registry[{}] = [{}]", t, oss.str());
    }
    for (const auto& [id, nodes] : subscribed_nodes_for_id) {
        std::ostringstream oss; for (auto& n : nodes) oss << n << " ";
        m_logger->debug("  subscribed_nodes_for_id[{}] = [{}]", id, oss.str());
    }
    for (const auto& [t, nodes] : subscribed_nodes_for_type) {
        std::ostringstream oss; for (auto& n : nodes) oss << n << " ";
        m_logger->debug("  subscribed_nodes_for_type[{}] = [{}]", t, oss.str());
    }
    {
        std::ostringstream oss; for (auto& a : announcements) oss << a << " ";
        m_logger->debug("  announcements = [{}]", oss.str());
    }

    // ---- DEBUG SUMMARY (quick sanity check) ----
    size_t type_count = type_registry.size();
    size_t id_total = 0; for (auto& [_, s] : type_registry) id_total += s.size();

    size_t sub_id_count = 0; for (auto& [_, s] : subscribed_nodes_for_id)   sub_id_count += s.size();
    size_t sub_ty_count = 0; for (auto& [_, s] : subscribed_nodes_for_type) sub_ty_count += s.size();

    m_logger->info("[promotion] summary: types={} ids={} subs(byId)={} subs(byType)={} announced={}", type_count, id_total, sub_id_count, sub_ty_count, announcements.size());
}

void SNDSProducer::inherit_from_store_after_promotion_()
{
    // Merge store-backed state into in-memory containers
    rebuild_containers_from_store_();

    // (Re-)announce all names we have persisted. Registration is idempotent.
    //    On successful registration, onRegisterSuccess() will call announce(...),
    //    which records the name (again idempotently) into the store.
    for (const auto& name : announcements)
    {
        try
        {
            m_logger->info("[promotion] (re)announcing {}", name);
            m_face.setInterestFilter(
                name,
                std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
                std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
                std::bind(&SNDSProducer::onRegisterFailed,  this, std::placeholders::_1, std::placeholders::_2)
            );
        }
        catch (const std::exception& e)
        {
            m_logger->warn("[promotion] failed to (re)announce {}: {}", name, e.what());
        }
    }

    // Optional: refresh per-type Broker metadata so dashboards stay accurate.
    //    We only read the set of types we've seen so far.
    for (const auto& [type, _ids] : type_registry)
    {
        update_store_with_broker_state_(type);
    }
}

void SNDSProducer::debug_dump_state_(std::string_view where)
{
    // Quick summary counts
    size_t type_count = type_registry.size();
    size_t id_total   = 0; for (auto& [_, s] : type_registry) id_total += s.size();
    size_t sub_id_cnt = 0; for (auto& [_, s] : subscribed_nodes_for_id)   sub_id_cnt += s.size();
    size_t sub_ty_cnt = 0; for (auto& [_, s] : subscribed_nodes_for_type) sub_ty_cnt += s.size();
    size_t ann_cnt    = announcements.size();

    m_logger->info("[debug][{}] summary → types={} ids={} subs(byId)={} subs(byType)={} announced={}",
                   where, type_count, id_total, sub_id_cnt, sub_ty_cnt, ann_cnt);

    // Detailed container dumps
    m_logger->debug("[debug][{}] type_registry:", where);
    for (const auto& [t, ids] : type_registry) {
        std::ostringstream oss; for (const auto& id : ids) oss << id << ' ';
        m_logger->debug("  [{}] {}", t, oss.str());
    }

    m_logger->debug("[debug][{}] subscribed_nodes_for_id:", where);
    for (const auto& [id, nodes] : subscribed_nodes_for_id) {
        std::ostringstream oss; for (const auto& n : nodes) oss << n << ' ';
        m_logger->debug("  [{}] {}", id, oss.str());
    }

    m_logger->debug("[debug][{}] subscribed_nodes_for_type:", where);
    for (const auto& [t, nodes] : subscribed_nodes_for_type) {
        std::ostringstream oss; for (const auto& n : nodes) oss << n << ' ';
        m_logger->debug("  [{}] {}", t, oss.str());
    }

    {
        std::ostringstream oss; for (auto& a : announcements) oss << a << ' ';
        m_logger->debug("[debug][{}] announcements: {}", where, oss.str());
    }

    try 
    {
        const auto snap   = store_.get_snapshot().to_json();
        const auto* state = (snap.contains("state") && snap["state"].is_object()) ? &snap["state"] : nullptr;
        const auto* broker= (state && state->contains("broker") && (*state)["broker"].is_object()) ? &(*state)["broker"] : nullptr;
        const auto* journal = (broker && broker->contains("journal") && (*broker)["journal"].is_array()) ? &(*broker)["journal"] : nullptr;

        const size_t jsize = journal ? journal->size() : 0;
        m_logger->info("[debug][{}] broker.journal: entries={} cursor={} last_fp={}", where, jsize, broker_journal_cursor_, shorten(last_replayed_fp_, 10));

        if (journal && !journal->empty()) 
        {
            // Dump the last few entries to avoid log spam
            const size_t LIMIT = 10;
            const size_t start = (jsize > LIMIT) ? (jsize - LIMIT) : 0;
            m_logger->debug("[debug][{}] broker.journal (last {} of {}):", where, jsize - start, jsize);

            for (size_t i = start; i < jsize; ++i) 
            {
                const auto& e = (*journal)[i];
                const std::string ts   = e.value("ts", "");
                const std::string type = e.value("type", e.contains("payload") && e["payload"].is_object() ? e["payload"].value("type", "GENERIC") : "GENERIC");
                const std::string id   = e.value("id",   e.contains("payload") && e["payload"].is_object() ? e["payload"].value("id", "") : "");
                size_t payload_keys = 0;
                if (e.contains("payload")) 
                {
                    const auto& p = e["payload"];
                    payload_keys = p.is_object() ? p.size() : (p.is_array() ? p.size() : 1);
                }
                m_logger->debug("  [{}] ts={} type={} id={} payload_keys={}", i, ts, type, id, payload_keys);
            }
        }
    }
    catch (const std::exception& ex) 
    {
        m_logger->warn("[debug][{}] broker.journal dump failed: {}", where, ex.what());
    }
}

void SNDSProducer::debug_dump_overlay_(std::string_view where)
{
    m_logger->debug("[overlay][{}] ver={} hash={} keys={}",
        where,
        overlay_.version,
        overlay_.hash,
        overlay_.state.is_object() ? overlay_.state.size() : 0);

    // Show pending broker journal adds that SECONDARY would replay on promotion
    if (overlay_.state.is_object()) 
    {
        const auto it = overlay_.state.find("broker_journal_add");
        if (it != overlay_.state.end() && it->is_array()) 
        {
            const auto& add = *it;
            m_logger->debug("[overlay][{}] broker_journal_add: entries={}", where, add.size());

            if (!add.empty()) 
            {
                const size_t LIMIT = 10;
                const size_t start = (add.size() > LIMIT) ? (add.size() - LIMIT) : 0;
                for (size_t i = start; i < add.size(); ++i) 
                {
                    const auto& e = add[i];
                    const std::string ts   = e.value("ts", "");
                    const std::string type = e.value("type", e.contains("payload") && e["payload"].is_object() ? e["payload"].value("type", "GENERIC") : "GENERIC");
                    const std::string id   = e.value("id",   e.contains("payload") && e["payload"].is_object() ? e["payload"].value("id", "") : "");
                    size_t payload_keys = 0;
                    if (e.contains("payload")) 
                    {
                        const auto& p = e["payload"];
                        payload_keys = p.is_object() ? p.size() : (p.is_array() ? p.size() : 1);
                    }
                    m_logger->debug("  [add:{}] ts={} type={} id={} payload_keys={}", i, ts, type, id, payload_keys);
                }
            }
        }
    }
}

// Deterministic merge of the few shapes we locally touch.
// Keep this narrowly scoped (announcements, registry maps, subscribers).
nlohmann::json SNDSProducer::merged_effective_state_() const 
{
    using json = nlohmann::json;
    json base = store_.get_snapshot().state;
    const json& ov = overlay_.state;

    if (ov.contains("json_patch_against_base") && ov["json_patch_against_base"].is_array()) 
    {
        m_logger->debug("[overlay] merge: applying json_patch_against_base (ops={})", ov["json_patch_against_base"].size());
        try 
        {
            return base.patch(ov["json_patch_against_base"]);
        } 
        catch (const std::exception& e) 
        {
            m_logger->warn("[merge] failed to apply json_patch_against_base: {}", e.what());
        }
    }

    // ---------- ANNOUNCEMENTS (object: {name:true}) ----------
    ensure_obj(base, "announcements");
    std::unordered_set<std::string> ann;
    for (auto& [k, v] : base["announcements"].items()) 
    {
        if (!v.is_boolean() || v.get<bool>()) ann.insert(k);
    }

    if (ov.contains("ann_add"))
    {
        for (const auto& a : ov["ann_add"]) if (a.is_string()) ann.insert(a.get<std::string>());
    }

    if (ov.contains("ann_del"))
    {
        for (const auto& d : ov["ann_del"]) if (d.is_string()) ann.erase(d.get<std::string>());
    }

    base["announcements"] = json::object();
    for (const auto& name : ann) base["announcements"][name] = true;

    // ---------- TYPES REGISTRY (types -> ids -> {id:true}) ----------
    ensure_obj(base, "types");
    auto add_to_type = [&](const std::string& t, const std::string& id)
    {
        auto& tnode = base["types"][t];
        if (!tnode.is_object()) tnode = json::object();
        auto& ids   = ensure_obj(tnode, "ids");
        ids[id] = true;
    };

    auto del_from_type = [&](const std::string& t, const std::string& id)
    {
        auto itT = base["types"].find(t);
        if (itT == base["types"].end() || !itT->is_object()) return;
        auto& ids = ensure_obj(*itT, "ids");
        ids.erase(id);
        if (ids.empty()) itT->erase("ids");
        if (itT->empty()) base["types"].erase(t);
    };

    if (ov.contains("reg_add") && ov["reg_add"].is_object())
    {
        for (auto& [t, arr] : ov["reg_add"].items())
        {
            for (auto& v : arr) if (v.is_string()) add_to_type(t, v.get<std::string>());
        }
    }

    if (ov.contains("reg_del") && ov["reg_del"].is_object())
    {
        for (auto& [t, arr] : ov["reg_del"].items())
        {
            for (auto& v : arr) if (v.is_string()) del_from_type(t, v.get<std::string>());
        }
    }

    // ---------- SUBSCRIBERS ----------
    // Overlay uses "sub_add"/"sub_del" with keys "byId:<ID>" or "byType:<TYPE>"
    ensure_obj(base, "subscribers");
    auto& subs = base["subscribers"];
    auto ensure_nodes_obj = [&](json& parent, const std::string& key)->json&
    {
        auto& obj = parent[key];
        if (!obj.is_object()) obj = json::object();
        auto& nodes = ensure_obj(obj, "nodes");
        return nodes;
    };

    auto apply_sub_delta = [&](const char* kind)
    {
        if (!ov.contains(kind) || !ov[kind].is_object()) return;
        for (auto& [k, arr] : ov[kind].items()) 
        {
            // k is "byId:foo" or "byType:bar"
            const std::string prefix_byId  = "byId:";
            const std::string prefix_byTyp = "byType:";
            if (k.rfind(prefix_byId, 0) == 0) 
            {
                const std::string id = k.substr(prefix_byId.size());
                //auto& nodes = ensure_nodes_obj(subs, "byId")[id]["nodes"]; // ensure chain
                // normalize ensure chain explicitly:
                auto& byId = ensure_obj(subs, "byId");
                auto& ent  = byId[id]; if (!ent.is_object()) ent = json::object();
                auto& nmap = ensure_obj(ent, "nodes");

                for (auto& v : arr) if (v.is_string()) 
                {
                    const std::string node = v.get<std::string>();
                    if (std::string(kind) == "sub_add") nmap[node] = true;
                    else nmap.erase(node);
                }

                if (nmap.empty()) 
                { 
                    ent.erase("nodes"); 
                    if (ent.empty()) byId.erase(id); 
                }
            } 
            else if (k.rfind(prefix_byTyp, 0) == 0) 
            {
                const std::string type = k.substr(prefix_byTyp.size());
                auto& byT  = ensure_obj(subs, "byType");
                auto& ent  = byT[type]; if (!ent.is_object()) ent = json::object();
                auto& nmap = ensure_obj(ent, "nodes");
                for (auto& v : arr) if (v.is_string()) 
                {
                    const std::string node = v.get<std::string>();
                    if (std::string(kind) == "sub_add") nmap[node] = true;
                    else nmap.erase(node);
                }
                if (nmap.empty()) 
                { 
                    ent.erase("nodes"); 
                    if (ent.empty()) byT.erase(type); 
                }
            } 
            else 
            {
                // unknown key; ignore
            }
        }
    };
    apply_sub_delta("sub_add");
    apply_sub_delta("sub_del");

    // ---------- BROKER OVERLAY ----------
    ensure_obj(base, "broker");
    auto& broker = base["broker"];

    // journal append
    ensure_arr(broker, "journal");
    if (ov.contains("broker_journal_add") && ov["broker_journal_add"].is_array()) 
    {
        for (const auto& e : ov["broker_journal_add"]) broker["journal"].push_back(e);
        // optional cap to mirror base capping:
        if (broker["journal"].size() > broker_journal_max) 
        {
            const auto overflow = broker["journal"].size() - broker_journal_max;
            broker["journal"].erase(broker["journal"].begin(), broker["journal"].begin() + static_cast<json::difference_type>(overflow));
        }
    }

    // collections updates + endpoint
    ensure_obj(broker, "collections");
    if (ov.contains("broker_collections_upd") && ov["broker_collections_upd"].is_object()) 
    {
        for (auto& [t, v] : ov["broker_collections_upd"].items()) broker["collections"][t] = v;
    }

    if (ov.contains("broker_endpoint") && ov["broker_endpoint"].is_string()) 
    {
        broker["endpoint"] = ov["broker_endpoint"];
    }

    return base;
}

void SNDSProducer::rebase_overlay_onto_base_after_remote_apply_(const nlohmann::json& /*base_before*/)
{
    const nlohmann::json effective_old = merged_effective_state_();
    const nlohmann::json base_after    = store_.get_snapshot().state;

    const auto patch = nlohmann::json::diff(base_after, effective_old);
    overlay_.state = patch.empty() ? nlohmann::json::object() : nlohmann::json{{"json_patch_against_base", patch}};
    overlay_.version++;
    overlay_.hash = overlay_hash_(overlay_.state);

    m_logger->debug("[overlay] REBASE → version={} hash={} patch_ops={}", overlay_.version, overlay_.hash, (overlay_.state.contains("json_patch_against_base") ? overlay_.state["json_patch_against_base"].size() : 0));
}


void SNDSProducer::squash_overlay_into_base_and_clear_()
{
    const nlohmann::json effective = merged_effective_state_();
    const nlohmann::json base_now  = store_.get_snapshot().state;

    if (effective != base_now) 
    {
        const auto before = store_.get_meta();
        store_.assign(effective);
        const auto after = store_.get_meta();
        m_logger->debug("[overlay] SQUASH → base version {}→{}  hash {}→{}  overlay ver={} hash={}", before.version, after.version, before.hash, after.hash, overlay_.version, overlay_.hash);
    } 
    else 
    {
        m_logger->debug("[overlay] SQUASH → no-op (effective==base), overlay ver={} hash={}", overlay_.version, overlay_.hash);
    }

    overlay_ = Overlay{}; // reset
    m_logger->debug("[overlay] CLEARED");
}
