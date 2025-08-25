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
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST:
            {
                express_get_by_id_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_WITH_SCOPE_ADVERTISEMENT:
            {
                express_id_with_scope_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_TYPE_REQUEST:
            {
                express_get_by_type_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_REQUEST:
            {
                express_get_by_id_for_type_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT:
            {
                express_by_id_and_type_advertisement_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION:
            {
                express_id_subscription_interest(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION:
            {
                express_id_subscription_notification(content);
                break;
            }
            case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FROM_NODE_REQUEST:
            {
                express_get_by_id_from_node_interest(content);
                break;
            }
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
    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE,
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

void SNDSConsumer::express_by_id_and_type_advertisement_interest(const std::string& content)
{
    auto tmp_interest  = parsed_interest_name::parse_from_string(content);

    auto name = parsed_interest_name::to_custom_ndn_name
    (

        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            tmp_interest.prefix, //TODO ask John if this is okay
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

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            query.id,
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

void SNDSConsumer::express_id_subscription_notification(const std::string& content)
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

    // we know the format must be /snds_prefix/node/id
    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SUBSCRIPTION_NOTIFICATION,
            // these are the ID and the scope
            {id, snds_node}
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

void SNDSConsumer::express_id_subscription_interest(const std::string& content)
{
    auto tmp_interest = parsed_interest_name::parse_from_string(content);

    // we know the format must be /snds_prefix/snds_id_scope_name/id
    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            snds_id_scope_name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_SUBSCRIPTION,
            // these are the ID and the scope
            {*tmp_interest.get_interest_parameter(0), *tmp_interest.get_interest_parameter(1)}
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

void SNDSConsumer::express_id_with_scope_interest(const std::string& content)
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

    // we know the format must be /snds_prefix/snds_id_scope_name/id
    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix, 
            snds_id_scope_name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SCOPE_NOTIFICATION,
            // build a temporary vector<string> from the unwrapped optional
            {id, snds_node}
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

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            tmp_interest.prefix, 
            tmp_interest.name,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID,
            // build a temporary vector<string> from the unwrapped optional
            {*tmp_interest.get_interest_parameter(0)}
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

//TODO possible disclose spot only for ID and timestamp
//TODO possible verify spot
void SNDSConsumer::send_metafile_interests(const std::string& body, const temporal_query::TemporalQuery& parent_query){
    nlohmann::json root = nlohmann::json::parse(body);

	std::unordered_set<std::string> seen_interests;

    // included in the interest to know which parent 
    // query the child query references
    auto parent_query_hash = temporal_query::temporal_query_hash_string(parent_query);

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
				//std::string timestamp = format_timestamp_ms(raw_timestamp);
                //std::string timestamp = "geia";
                m_logger->debug("Before encoding timestamp: {}", timestamp);
				timestamp = url_encode(timestamp);

                //TODO what in the name of god
                // use my own structures maybe? 
                auto name = parsed_interest_name::make_interest_name_from_metafile
                (
                    "SNDS", 
                    id,
                    "version:",
                    timestamp
                );

                name.params.push_back(url_encode("metafile=true"));
                name.params.push_back(url_encode("parentQueryHash=" + parent_query_hash));
                auto name_to_string = parsed_interest_name::to_custom_ndn_name(name);

                // try to insert; if it's new, send it
                // for example you might have two dates that are the same
                if (!seen_interests.emplace(name_to_string).second) {
                    m_logger->debug("Skipping duplicate interest: {}", name_to_string);
                    continue;
                }

                m_logger->info("Built interest: {} sending actual interest: {}", name.to_string(), name_to_string);
                ndn::Name interestName(name_to_string);
                interestName.appendVersion();
                m_logger->info("Created interest name {}", interestName.toUri().c_str());
                ndn::Interest interest(interestName);
                //TODO we don't want all the cases to be fresh
                //for example the latest case
                //interest.setMustBeFresh(true);
                interest.setCanBePrefix(true);
                interest.setInterestLifetime(ndn::time::milliseconds(6000)); 
                m_logger->info("Sending actual Interest {}", interest.toUri().c_str());

                m_face.expressInterest(interest,
                                              std::bind(&SNDSConsumer::onData, this,  _1, _2),
                                              std::bind(&SNDSConsumer::onNack, this, _1, _2),
                                              std::bind(&SNDSConsumer::onTimeout, this, _1));
            }
        }
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

    const parsed_interest_name::ParsedInterestName pi = parsed_interest_name::parse_from_string(url_decode(interest.toUri()));
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
        case parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::METAFILE:
        {
            //this is a response from a metafile 
            //send the data for the temporal query back to the worker
            //for it to send back to the consumer HTTP request
            //Possible spot for disclosing ID and Date
            //should verify here.
            m_logger->info("Temporal Query Data Response from Metafile...");
            // I'm too tired to use my own data classes here
            //TODO check this out later
            std::string message_name = data.getName().at(1).toUri();
            std::string date  = url_decode(data.getName().at(2).toUri());
            std::string metafile  = url_decode(data.getName().at(3).toUri());
            std::string parent_query_hash  = url_decode(data.getName().at(4).toUri());
            message_name = "id=" + message_name;
            auto name = "/?" + message_name + "&" + date + "&" + metafile + "&" + parent_query_hash;
            m_logger->info("Sending Temporal Query Data with name: {0} and date: {1} ", message_name, date);
            //TODO possible spot for disclose
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                std::move(payload),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_RESPONSE, //TODO not sure this is correct
                snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
                std::move(name),
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
                payload = payload.substr(pos);
                m_logger->debug("Filtered payload to:\n{}", payload);
            }

            // data.getName() equivalent with interest.toUri()
            auto parent_temporal_query = temporal_query_from_interest(pi);
            //send_metafile_interests(payload, parent_temporal_query);
            break;
        }
    }

}
