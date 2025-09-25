#include "snds_consumer.hpp"

void SNDSConsumer::init(){
    m_logger->info("Starting service: {}", this->serviceName);
    if (serviceName.empty()) 
    {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }

    // Run the loop until 'running' is false.
    while (running.load()) 
    {

        m_logger->info("Reading packet...");
        std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> message = v_to_consumer->readMessage();

        if (message == nullptr) 
        {
            m_logger->debug("Queue shutdown detected, exiting loop.");
            break;
        }

        auto content = message.get()->get_message();
        auto length = message.get()->get_message_length();
        auto type = message.get()->get_message_type();
        m_logger->debug("Extracted content from message: {}", content);
        m_logger->debug("Extracted content length from message: {}", length);
        m_logger->debug("Extracted message type from message: {}", snds_intra_message::to_string(type));


        switch (type)
        {
            //case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST:
            //{
            //    express_get_by_id_interest(content);
            //    break;
            //}
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_AND_TYPE_WITH_SCOPE_ADVERTISEMENT:
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_WITH_SCOPE_ADVERTISEMENT:
            {
                express_scope_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_TYPE_REQUEST:
            {
                express_get_by_type_interest(content);
                break;
            }
            //case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_REQUEST:
            //{
            //    express_get_by_id_for_type_interest(content);
            //    break;
            //}
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT:
            {
                express_by_id_and_type_advertisement_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_SUBSCRIPTION: 
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION:
            {
                express_subscription_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION:
            {
                express_subscription_notification(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FROM_NODE_REQUEST:
            {
                express_get_by_id_from_node_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_REQUEST:
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST:
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST:
            {
                express_temporal_query_interest(content);
                break;
            }

        }
    }
}

void SNDSConsumer::express_get_by_id_from_node_interest(const std::string& content)
{
    auto tmp_interest = parsed_interest_name::parse_from_string(content);


    auto maybe_id = tmp_interest.get_interest_parameter(0);
    auto maybe_snds_node = tmp_interest.get_interest_parameter(1);

    if(!maybe_id.has_value())
    {
        m_logger->warn("Received ID with no value in: {}", __func__);
        return;
    }

    if(!maybe_snds_node.has_value())
    {
        m_logger->warn("Received SNDS Node with no value in: {}", __func__);
        return;
    }

    auto id = maybe_id.value();
    auto snds_node = maybe_snds_node.value();

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FROM_NODE,
            {id, snds_node}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);
    ndn::Name interestName(name);
    ndn::Interest interest(interestName);
    //TODO we don't want all the cases to be fresh
    //for example the latest case
    //interest.setMustBeFresh(true);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
    m_logger->info("Sending actual Interest {}", interest.toUri().c_str());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();
}

void SNDSConsumer::express_get_by_id_for_type_interest(const std::string& content)
{
    auto tmp_interest  = parsed_interest_name::parse_from_string(content);


    auto maybe_id = tmp_interest.get_interest_parameter(0);
    auto maybe_type = tmp_interest.get_interest_parameter(1);

    if(!maybe_id.has_value())
    {
        m_logger->error("Received getByIdForType Interest without value in the ID...");
        return;
    }

    if(!maybe_type.has_value())
    {
        m_logger->error("Received getByIdForType Interest without type in the TYPE...");
        return;
    }

    auto id = maybe_id.value();
    auto type = maybe_type.value();

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE,
            {id, type}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);
    ndn::Name interestName(name);
    ndn::Interest interest(interestName);
    //TODO we don't want all the cases to be fresh
    //for example the latest case
    //interest.setMustBeFresh(true);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
    m_logger->info("Sending actual Interest {}", interest.toUri().c_str());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();
}

void SNDSConsumer::express_by_id_and_type_advertisement_interest(const std::string& content)
{
    auto tmp_interest  = parsed_interest_name::parse_from_string(content);
    
    auto maybe_id   = tmp_interest.get_interest_parameter(0);
    auto maybe_type = tmp_interest.get_interest_parameter(1);

    if(!maybe_id.has_value())
    {
        m_logger->error("Received ID with no value in: {}", __func__);
        return;
    }

    if(!maybe_type.has_value())
    {
        m_logger->error("Received TYPE with no value in: {}", __func__);
        return;
    }

    auto id = maybe_id.value();
    auto type = maybe_type.value();

    auto name = parsed_interest_name::to_custom_ndn_name
    (

        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT,
            {*tmp_interest.get_interest_parameter(0), *tmp_interest.get_interest_parameter(1)}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);
    ndn::Name interestName(name);
    ndn::Interest interest(interestName);
    //TODO we don't want all the cases to be fresh
    //for example the latest case
    //interest.setMustBeFresh(true);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
    m_logger->info("Sending actual Interest {}", interest.toUri().c_str());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();

}

void SNDSConsumer::express_temporal_query_interest(const std::string& content)
{
    temporal_query::TemporalQuery query = temporal_query::deserialize_temporal_query(content);
    //query.date = md5_hex(&query.date, query.date.size());
    auto tmp_name = temporal_query::to_path(query);

    m_logger->debug("Query.type: {}", query.type);
    m_logger->debug("Query.id: {}", query.id);
    // Use type if provided; otherwise fall back to id
    const std::string& entity = (!query.type.empty() ? query.type : query.id);

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            entity,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::TEMPORAL_QUERY,
            temporal_query::to_fields(query)
        )
    );

    m_logger->info("Created name {}", name);
    ndn::Name interestName(name);
    ndn::Interest interest(interestName);
    //TODO we don't want all the cases to be fresh
    //for example the latest case
    //interest.setMustBeFresh(true);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
    m_logger->info("Sending actual Interest {}", interest.toUri().c_str());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();

}

void SNDSConsumer::express_get_by_type_interest(const std::string& content)
{
    auto tmp_interest  = parsed_interest_name::parse_from_string(content);

    auto name = parsed_interest_name::to_custom_ndn_name
    (

        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_TYPE,
            {*tmp_interest.get_interest_parameter(0)}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);
    ndn::Name interestName(name);
    ndn::Interest interest(interestName);
    //TODO we don't want all the cases to be fresh
    //for example the latest case
    //interest.setMustBeFresh(true);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
    m_logger->info("Sending actual Interest {}", interest.toUri().c_str());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();
}

void SNDSConsumer::express_subscription_notification(const std::string& content)
{
    auto tmp_interest = parsed_interest_name::parse_from_string(content);

    auto maybe_id = tmp_interest.get_interest_parameter(0);
    auto maybe_type = tmp_interest.get_interest_parameter(1);
    auto maybe_notifying_snds_node = tmp_interest.get_interest_parameter(2);
    auto subscriber_snds_node = tmp_interest.name;

    if(!maybe_id.has_value())
    {
        m_logger->error("Received ID with no value in: {}", __func__);
        return;
    }

    if(!maybe_type.has_value())
    {
        m_logger->warn("Received SNDS Node with no value in: {}", __func__);
    }

    if(!maybe_notifying_snds_node.has_value())
    {
        m_logger->warn("Received notifying SNDS Node with no value in: {}", __func__);
    }

    auto id = maybe_id.value();
    auto type = maybe_type.value_or("");
    auto notifying_snds_node = maybe_notifying_snds_node.value_or("");

    // we know the format must be /snds_prefix/node/id
    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            subscriber_snds_node,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION,
            // these are the ID and the scope
            {id, type, notifying_snds_node}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);

    ndn::Name interestName(name);
    //interestName.appendVersion();
    ndn::Interest interest(interestName);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000));

    m_logger->debug("Sending actual interest to the network: {}", interest.toUri());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();

}

void SNDSConsumer::express_subscription_interest(const std::string& content)
{
    auto tmp_interest = parsed_interest_name::parse_from_string(content);


    auto interest_type        = tmp_interest.command;
    auto other_scope_name     = tmp_interest.name;
    auto maybe_name           = tmp_interest.get_interest_parameter(0);
    auto maybe_our_scope_name = tmp_interest.get_interest_parameter(1);


    m_logger->info("Received other scope name: {} in {}", other_scope_name, __func__);

    if(!maybe_name.has_value())
    {
        m_logger->error("Received subscription interest without Name value in: {}", __func__);
        return;
    }

    if(!maybe_our_scope_name.has_value())
    {
        m_logger->error("Received id subscription interest without ID SCOPE NAME in: {}", __func__);
        return;
    }

    auto name           = maybe_name.value();
    auto our_scope_name = maybe_our_scope_name.value();

    m_logger->info("Received other scope name: {}, our scope name: {} for name: {} in func: {}", other_scope_name, our_scope_name, name, __func__);

    const bool by_id = (interest_type == parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_ID_SUBSCRIPTION);
    const char* kind = by_id ? "BY_ID" : "BY_TYPE";
    const auto interest_kind = by_id ? parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_ID_SUBSCRIPTION : parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_TYPE_SUBSCRIPTION;

    // we know the format must be /snds_prefix/snds_id_scope_name/name
    auto interest_name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            other_scope_name,
            interest_kind,
            // these are the ID and the scope
            {name, our_scope_name}
        )
    );

    m_logger->debug("Created name: {} in func: {}", interest_name, __func__);

    ndn::Name interestName(interest_name);
    //interestName.appendVersion();
    ndn::Interest interest(interestName);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000));

    m_logger->debug("Sending actual interest to the network: {}", interest.toUri());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();

}

void SNDSConsumer::express_scope_interest(const std::string& content)
{
    auto tmp_interest = parsed_interest_name::parse_from_string(content);
    auto maybe_id = tmp_interest.get_interest_parameter(0);
    auto maybe_type = tmp_interest.get_interest_parameter(1);


    if(!maybe_id.has_value())
    {
        m_logger->warn("Received ID with no value in: {}", __func__);
        return;
    }

    if(!maybe_type.has_value())
    {
        m_logger->warn("Received TYPE with no value in: {}", __func__);
    }

    auto id = maybe_id.value();
    auto type = maybe_type.value_or("");
    auto node_responsible_for_name = tmp_interest.name;

    // we know the format must be /snds_prefix/snds_id_scope_name/id
    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            node_responsible_for_name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SCOPE_NOTIFICATION,
            {id, type, snds_id_scope_name}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);

    ndn::Name interestName(name);
    //interestName.appendVersion();
    ndn::Interest interest(interestName);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000));

    m_logger->debug("Sending actual interest to the network: {}", interest.toUri());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();

}

void SNDSConsumer::express_get_by_id_interest(const std::string& content)
{
    auto tmp_interest = parsed_interest_name::parse_from_string(content);

    auto maybe_id = tmp_interest.get_interest_parameter(0);

    if(!maybe_id.has_value())
    {
        m_logger->error("Received ID with no value in {}", __func__);
        return;
    }

    auto id = maybe_id.value();

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            tmp_interest.prefix, 
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID,
            {id}
        )
    );

    m_logger->debug("Created name: {} in func: {}", name, __func__);

    ndn::Name interestName(name);
    //interestName.appendVersion();
    ndn::Interest interest(interestName);
    interest.setCanBePrefix(true);
    interest.setInterestLifetime(ndn::time::milliseconds(6000));

    m_logger->debug("Sending actual interest to the network: {}", interest.toUri());
    m_face.expressInterest
    (
        interest,
        std::bind(&SNDSConsumer::onData, this,  _1, _2),
        std::bind(&SNDSConsumer::onNack, this, _1, _2),
        std::bind(&SNDSConsumer::onTimeout, this, _1)
    );
	m_face.processEvents();

}

void SNDSConsumer::send_metafile_interests(const std::string& body, const temporal_query::TemporalQuery& parent_query)
{
    nlohmann::json root = nlohmann::json::parse(body);
    const auto parent_query_hash = temporal_query::temporal_query_hash_string(parent_query);
    m_logger->debug("Created parent_query_hash: {} in {}", parent_query_hash, __func__);

    std::unordered_set<std::string> seen_canonical;
    std::optional<parsed_interest_name::ParsedInterestName> pending;

    auto send_with_flag = [&](parsed_interest_name::ParsedInterestName& pin, bool is_last)
    {
        parsed_interest_name::add_param(pin, GET_PARAMETER_LAST_METAFILE_INTEREST_TAG, is_last ? "true" : "false");

        const auto name_to_string = parsed_interest_name::to_custom_ndn_name(pin, true);
        m_logger->info("Sending Interest ({}={}): {}", GET_PARAMETER_LAST_METAFILE_INTEREST_TAG, is_last ? "true" : "false", name_to_string);

        try 
        {
            ndn::Name interestName(name_to_string);
            m_logger->debug("Created interest_name: {}", interestName.toUri());
            ndn::Interest interest(interestName);
            interest.setCanBePrefix(true);
            interest.setInterestLifetime(ndn::time::milliseconds(6000));

            m_face.expressInterest
            (
                interest,
                std::bind(&SNDSConsumer::onData,    this, _1, _2),
                std::bind(&SNDSConsumer::onNack,    this, _1, _2),
                std::bind(&SNDSConsumer::onTimeout, this, _1)
            );
        } 
        catch (const std::exception& e) 
        {
            m_logger->error("Invalid NDN name {}; dropping. Error: {}", name_to_string, e.what());
        }
    };

    for (const auto& entry : root) 
    {
        for (auto& [key, arr] : entry.items()) 
        {
            m_logger->debug("Parsing key: {}", key);
            if (!arr.is_array()) 
            { 
                m_logger->warn("Expected array at key {}", key); 
                continue; 
            }

            for (const auto& doc : arr) 
            {
                // ---- SAFE READS ----
                const auto& id_object = doc["_id"];
                if (!id_object.is_object()) { m_logger->warn("Missing _id object"); continue; }

                const auto& id_field = id_object["_id"];
                if (id_field.is_null()) { m_logger->warn("Missing _id._id"); continue; }
                const std::string id = ::json_to_string(id_field);

                const auto& date_field = id_object["timestamp"]["$date"];
                if (date_field.is_null()) { m_logger->warn("Missing _id.timestamp.$date"); continue; }
                const long long raw_timestamp = ::json_to_epoch_ms(date_field);

                const std::string type = ::json_to_string(doc["type"]);
                m_logger->debug("Doc fields -> id={}, ts(ms)={}, type={}", id, raw_timestamp, type);

                // ---- Build base interest (no last-flag yet) ----
                auto pin = parsed_interest_name::make_interest_name_from_metafile
                (
                    snds_prefix,
                    id,
                    parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::VERSION,
                    std::vector<std::string>{}
                );

                m_logger->debug("Adding params...");
                try
                {
                    parsed_interest_name::add_param(pin, GET_PARAMETER_ID_TAG,   id);
                    parsed_interest_name::add_param(pin, GET_PARAMETER_TYPE_TAG, type);
                    parsed_interest_name::add_param(pin, GET_PARAMETER_DATE_TAG, format_timestamp_ms(raw_timestamp));
                    parsed_interest_name::add_param(pin, GET_PARAMETER_IS_METAFILE_TAG, "true");
                    parsed_interest_name::add_param(pin, GET_PARAMETER_PARENT_QUERY_HASH_TAG, parent_query_hash);
                
                    m_logger->debug("Added params...");
                }
                catch (const std::exception& ex)
                {
                    m_logger->error("Exception while adding params: {}", ex.what());
                }
                catch (...)
                {
                    m_logger->error("Unknown exception while adding params");
                }
                m_logger->debug("Added params...");

                const auto canonical = parsed_interest_name::to_custom_ndn_name(pin, true);

                if (!seen_canonical.emplace(canonical).second) 
                {
                    m_logger->debug("Skipping duplicate (canonical): {}", canonical);
                    continue; 
                }

                // We found a new unique interest:
                // flush the previous pending one as not-last, then keep this as pending
                if (pending.has_value()) 
                {
                    send_with_flag(pending.value(), false);
                    pending.reset();
                }
                pending = std::move(pin);
            }
        }
    }

    // After the loop, whatever is left is the last
    if (pending.has_value()) 
    {
        send_with_flag(pending.value(), /*is_last*/ true);
    } 
    else 
    {
        m_logger->info("No metafile interests to send for this parent query.");
    }
}

void SNDSConsumer::onData(const ndn::Interest& interest, const ndn::Data& data) 
{
    // Log the receipt of the Data packet.
    m_logger->info("Received Data with name: {0} in function {1}", data.getName().toUri(), __FUNCTION__);

    auto block = data.getContent();
    const auto* value = block.value();
    auto len          = block.value_size();  


    // Convert the data content into a string.
    std::string payload(reinterpret_cast<const char*>(value), len);
    m_logger->info("Data packet payload: {0} in function {1}", payload, __FUNCTION__);

    // Ensure the Data name has at least two components.
    if (data.getName().size() < 2) 
    {
        m_logger->warn("WARNING: Got unexpectedly small content name. Skipping.");
        return;
    }

    const parsed_interest_name::ParsedInterestName pi = parsed_interest_name::parse_from_string(url_decode(data.getName().toUri()));
    m_logger->info("Parsed interest: {} in {}", pi.to_string(), __func__);


    // If the payload contains the ACK tag, then no further processing is needed.
    if(snds_intra_message::is_ack_message(payload))
    {
        m_logger->info("Got ACK {}. Skipping...", payload);
        auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::move(parsed_interest_name::to_custom_ndn_name(pi)),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_RESPONSE,
            snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
            std::move(payload),
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );

        // Dispatch the intra-service message to the service queue for further processing.
        v_to_worker->writeMessage(std::move(message));
        return;
    }


    switch(pi.command)
    {
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_TYPE:
        {
            m_logger->info("Received Registry File");

            auto maybe_type = pi.get_interest_parameter(0);

            if(!maybe_type)
            {
                m_logger->warn("Received empty TYPE in {}", __func__);
                return;
            }

            auto type = maybe_type.value();

            m_logger->info("In Function: {0} sending name {1} to worker {2}", __func__, type, payload);
            // Create a new intra-service message with the payload.
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                std::move(type),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::REGISTRY_PAYLOAD,
                snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                std::move(payload),
                snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
            );
            // Dispatch the intra-service message to the service queue for further processing.
            v_to_worker->writeMessage(std::move(message));
            break;
        }
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE:
        {
            auto maybe_id = pi.get_interest_parameter(0);
            auto maybe_type = pi.get_interest_parameter(1);

            if(!maybe_id.has_value())
            {
                m_logger->warn("Received ID with no value in: {}", __func__);
                break;
            }

            if(!maybe_type.has_value())
            {
                m_logger->warn("Received TYPE with no value in: {}", __func__);
                break;
            }

            auto id = maybe_id.value();
            auto type = maybe_type.value();


            auto name = parsed_interest_name::to_custom_ndn_name
            (
                parsed_interest_name::make_parsed_interest_name
                (
                    type,
                    id,
                    parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE,
                    {id, type}
                )
            );

            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                std::move(name),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_RESPONSE,
                snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                std::move(payload),
                snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
            );

            // Dispatch the intra-service message to the service queue for further processing.
            v_to_worker->writeMessage(std::move(message));
            break;
        }
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FROM_NODE:
        {
            auto maybe_id = pi.get_interest_parameter(0);

            if(!maybe_id.has_value())
            {
                m_logger->warn("Received ID with no value in {}", __func__);
                return;
            }

            auto id = maybe_id.value();

            m_logger->info("In Function: {0} sending name {1} to worker {2}", __FUNCTION__, id, payload);
            // Create a new intra-service message with the payload.
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                std::move(id),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FROM_NODE_RESPONSE,
                snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                std::move(payload),
                snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
            );

            // Dispatch the intra-service message to the service queue for further processing.
            v_to_worker->writeMessage(std::move(message));
            break;
        }
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID:
        {
            auto maybe_id = pi.get_interest_parameter(0);

            if(!maybe_id.has_value())
            {
                m_logger->warn("Received ID with no value in {}", __func__);
                return;
            }

            auto id = maybe_id.value();

            m_logger->info("In Function: {0} sending name {1} to worker {2}", __FUNCTION__, id, payload);
            // Create a new intra-service message with the payload.
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                std::move(id),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_RESPONSE,
                snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                std::move(payload),
                snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
            );

            // Dispatch the intra-service message to the service queue for further processing.
            v_to_worker->writeMessage(std::move(message));
            break;
        }
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::VERSION:
        {
            //this is a response from a metafile 
            //send the data for the temporal query back to the worker
            //for it to send back to the consumer HTTP request
            //Possible spot for disclosing ID and Date
            //should verify here.
            m_logger->info("Temporal Query Data Response from Metafile...");
            // I'm too tired to use my own data classes here
            //TODO check this out later
            auto maybe_id = pi.get_interest_parameter(0);

            if(!maybe_id.has_value())
            {
                m_logger->error("Received metafile response in {} without ID value", __func__);
                break;
            }

            auto maybe_type = pi.get_interest_parameter(1);

            if(!maybe_type.has_value())
            {
                m_logger->warn("Received metafile response in {} without type value", __func__);
                break;
            }

            auto maybe_timestamp = pi.get_interest_parameter(2);

            if(!maybe_timestamp.has_value())
            {
                m_logger->error("Received metafile response in {} without timestamp", __func__);
                break;
            }

            auto maybe_metafile = pi.get_interest_parameter(3);

            if(!maybe_metafile.has_value())
            {
                m_logger->warn("Received metafile response in {} without metafile=true", __func__);
                break;
            }

            auto maybe_parent_query_hash = pi.get_interest_parameter(4);

            if(!maybe_parent_query_hash.has_value())
            {
                m_logger->error("Received metafile response in {} without parent_query_hash", __func__);
                break;
            }

            auto maybe_last_interest = pi.get_interest_parameter(5);

            if(!maybe_last_interest.has_value())
            {
                m_logger->error("Received metafile response in {} without is last interest", __func__);
                break;
            }

            std::vector<std::string> kvs;
            kvs.push_back(*maybe_id);                // e.g. "id=car2"
            kvs.push_back(*maybe_type);              // e.g. "type="
            kvs.push_back(*maybe_timestamp);         // e.g. "date=2025-06-28T15:41:50Z" (raw or already encoded)
            kvs.push_back(*maybe_metafile);          // e.g. "metafile=true"
            kvs.push_back(*maybe_parent_query_hash); // e.g. "parentQueryHash=..."
            kvs.push_back(*maybe_last_interest);     // e.g. "lastInterest="

            const bool values_are_already_encoded = true;

            //message_name = "id=" + message_name;
            m_logger->info("Building query URI...");
            auto name = parsed_interest_name::build_query_uri("/", kvs, !values_are_already_encoded);
            ////TODO possible spot for disclose
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                std::move(name),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::VERSION_RESPONSE,
                snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                std::move(payload),
                snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
            );
            v_to_worker->writeMessage(std::move(message));
            break;
        }
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::TEMPORAL_QUERY:
        {
            m_logger->info("Temporal Query Reponse...");
            //weird thing where the data received have some weird characters in front
            //    �q[
            //{
            //"before": [
            //  {
            //    "_id": {
            //      "_id": "car2",
            //      "timestamp": {
            //        "$date": 1750835111321
            //      }
            //    }
            //  }
            //]
            auto pos = payload.find_first_of("[{");
            if (pos != std::string::npos) 
            {
                // get a balanced JSON slice, not just "from here to end"
                payload = extract_json_slice(payload);
                m_logger->debug("Removed extra characters from payload (first 32 bytes hex): {}", hexdump_prefix(payload));
                m_logger->debug("Candidate JSON slice:\n{}", payload);
            } 
            else 
            {
                m_logger->error("No JSON start detected. First 32 bytes: {}", hexdump_prefix(payload));
                return;
            }

            log_non_ascii(payload, m_logger);
            // sanitize UTF-8 before parsing
            payload = fix_mixed_utf8_cp1252(payload);
            log_non_ascii(payload, m_logger);


            // fast validation to avoid throwing
            if (!nlohmann::json::accept(payload)) 
            {
                m_logger->error("Payload is not valid JSON after UTF-8 sanitization. First 32 bytes: {}", hexdump_prefix(payload));
                return;
            }

            m_logger->debug("Sanitized payload to:\n{}", payload);

            // data.getName() equivalent with interest.toUri()

            //direct mode: just check whether the count is empty or 1
            nlohmann::json j = nlohmann::json::parse(payload);

            // Decide direct vs indirect by bucketed array size
            if (_is_direct_temporal_payload(j)) 
            {
                nlohmann::json item;
                if (!_extract_direct_temporal_item(j, item)) 
                {
                    m_logger->warn("Direct-mode detected but item extraction failed; falling back to manifest/indirect.");
                } 
                else 
                {
                    m_logger->debug("Direct mode (single id in bucket).");

                    parsed_interest_name::ParsedInterestName env =
                        parsed_interest_name::make_parsed_interest_name
                        (
                            /*prefix*/ "",
                            /*name*/   "",
                            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::VERSION,
                            {}
                        );

                    // Prefer normalized fields; fall back to any available source.
                    const std::string id   = item.value("id",    pi.get_interest_parameter(0).value_or(""));
                    const std::string type = item.value("type",  "");  // may be empty in your examples
                    // date can be number (epoch ms) or string; store as string for the URI if present.
                    std::string date;
                    if (item.contains("date")) 
                    {
                        if (item["date"].is_string())        date = item["date"].get<std::string>();
                        else                                  date = item["date"].dump(); // preserve numeric
                    }

                    parsed_interest_name::add_param(env, GET_PARAMETER_ID_TAG,                id);
                    parsed_interest_name::add_param(env, GET_PARAMETER_TYPE_TAG,              type);
                    parsed_interest_name::add_param(env, GET_PARAMETER_DATE_TAG,              date);
                    parsed_interest_name::add_param(env, GET_PARAMETER_IS_METAFILE_TAG,       "false");
                    parsed_interest_name::add_param(env, GET_PARAMETER_PARENT_QUERY_HASH_TAG, "");
                    parsed_interest_name::add_param(env, GET_PARAMETER_LAST_METAFILE_INTEREST_TAG, "true");
                    parsed_interest_name::add_param(env, GET_PARAMETER_RETRIEVE_MODE,         "direct");

                    const bool values_already_encoded = true;
                    std::string name = parsed_interest_name::build_query_uri("/", env.params, /*encode_values=*/!values_already_encoded);

                    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
                    (
                        std::move(name),
                        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::VERSION_RESPONSE,
                        snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                        std::move(payload),
                        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
                    );
                    v_to_worker->writeMessage(std::move(message));
                    return; // do not fall through
                }
            }

            // Indirect / manifest path (>= 2 ids or structure not recognized)
            auto parent_temporal_query = temporal_query_from_interest(pi);
            m_logger->debug("Created Parent Temporal Query: {}", temporal_query::serialize_kv(parent_temporal_query));
            send_metafile_interests(payload, parent_temporal_query);
            break;
        }
    }

};


// Small helper: try to read "[{ <bucket>: [ ... ] }]" and expose the inner array.
static bool _extract_bucket_array(const nlohmann::json& j, std::string* bucket_name, const nlohmann::json** bucket_array)
{
    if (!(j.is_array() && !j.empty())) return false;
    const auto& first = j.front();
    if (!(first.is_object() && first.size() == 1)) return false;

    // Single key object: { "<bucket>": <value> }
    const auto it = first.begin();
    if (!(it.value().is_array())) return false;

    if (bucket_name)  *bucket_name  = it.key();
    if (bucket_array) *bucket_array = &it.value();
    return true;
}

bool SNDSConsumer::_is_direct_temporal_payload(const nlohmann::json& j)
{
    std::string bucket;
    const nlohmann::json* arr = nullptr;
    if (!_extract_bucket_array(j, &bucket, &arr)) return false;

    // "Direct" ≙ exactly one item in the bucketed array
    return arr && arr->is_array() && arr->size() == 1;
}

// Normalize one entry into something usable by your VERSION interest builder.
// Falls back gracefully if fields are missing.
static nlohmann::json _normalize_temporal_entry_to_item(const nlohmann::json& entry)
{
    nlohmann::json out = nlohmann::json::object();

    // Common shape from your examples:
    // { "_id": { "_id": "car1", "timestamp": { "$date": 1758754068761 } }, "type": "GENERIC"? }
    if (entry.is_object())
    {
        if (entry.contains("_id") && entry["_id"].is_object())
        {
            const auto& idobj = entry["_id"];
            if (idobj.contains("_id") && idobj["_id"].is_string())
                out["id"] = idobj["_id"].get<std::string>();

            if (idobj.contains("timestamp") &&
                idobj["timestamp"].is_object() &&
                idobj["timestamp"].contains("$date"))
            {
                // Keep as-is (number or string). You can convert to ISO-8601 elsewhere if needed.
                out["date"] = idobj["timestamp"]["$date"];
            }
        }

        if (entry.contains("type")) out["type"] = entry["type"];
    }

    // If we failed to normalize, return the raw entry so caller can still try to read fields.
    if (out.empty()) return entry;
    return out;
}

bool SNDSConsumer::_extract_direct_temporal_item(const nlohmann::json& j, nlohmann::json& out)
{
    std::string bucket;
    const nlohmann::json* arr = nullptr;
    if (!_extract_bucket_array(j, &bucket, &arr)) return false;
    if (!(arr && arr->is_array() && !arr->empty())) return false;

    const auto& entry = (*arr)[0];
    out = _normalize_temporal_entry_to_item(entry);
    return out.is_object();
}