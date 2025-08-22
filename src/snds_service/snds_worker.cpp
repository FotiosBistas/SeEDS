#include "snds_worker.hpp"

void Worker::init() {
    m_logger->info("Starting service: {}", serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Thread started.");

    while (running.load()) 
    {
        m_logger->info("Reading packet...");

        auto message = v_to_worker.get()->readMessage();
        if (!message) 
        {
            m_logger->debug("Queue shutdown detected, exiting loop.");
            break;
        }

        // Retrieve the message content and extract parameters using snds_utils.
        std::string content = message->get_message();
        std::string payload = message->get_payload();
        std::string type    = snds_intra_message::to_string(message->get_message_type());
        m_logger->info("Received content: {}", content);
        m_logger->info("Received payload: {}", payload);
        m_logger->info("Received type: {}", type);

        //TODO we can differentiate messages based on type
        //I don't know why are we doing this
        // Process according to sender type.

        //TODO THREAD POOL?
        switch(message->get_sender())
        {
            case snds_intra_message::SNDS_CLIENT_TYPES::HTTP_SRV:
                _handle_http_server_intra_message(*message);
                break;
            case snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER:
                _handle_consumer_intra_message(*message);
                break;
            case snds_intra_message::SNDS_CLIENT_TYPES::PRODUCER:
                _handle_producer_intra_message(*message);
                break;  
        }

    }

    m_logger->info("Worker thread stopping.");
}

void Worker::_handle_producer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message)
{
    if(snds_intra_message::is_ack_message(message.get_payload()))
    {
        process_ack_message(std::move(message.move_message()), std::move(message.move_payload()));
        return;
    }
}


void Worker::_handle_http_server_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message)
{


    switch(message.get_message_type())
    {
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST:
        {
            auto params = message.unpack_query_params_from_message();
            auto id_iterator = params.find(message.get_param_name_id_().data());
            process_new_get_by_id(id_iterator->second); 
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_TYPE_REQUEST:
        {
            auto params = message.unpack_query_params_from_message();
            auto type_iterator = params.find(message.get_param_name_type_().data()); 
            if(type_iterator == params.end())
            {
                m_logger->error("Received {} with empty: {}", snds_intra_message::to_string(message.get_message_type()), message.get_param_name_type_());
                return;
            }
            process_new_get_by_type(type_iterator->second);
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION:
        {
            auto params = message.unpack_query_params_from_message();
            auto id_iterator = params.find(message.get_param_name_id_().data());
            auto url_iterator = params.find(message.get_param_name_subscription_url_().data()); 

            if(id_iterator == params.end())
            {
                m_logger->error("Received {} with empty: {}", snds_intra_message::to_string(message.get_message_type()), message.get_param_name_id_());
                return;
            }

            if(url_iterator == params.end())
            {
                m_logger->error("Received {} with empty: {}", snds_intra_message::to_string(message.get_message_type()), message.get_param_name_subscription_url_());
                return;
            }

            process_subscription_request(id_iterator->second, url_iterator->second);
            break;
        }
    }

}

void Worker::_handle_consumer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message)
{

    if(snds_intra_message::is_ack_message(message.get_payload()))
    {
        process_ack_message(std::move(message.move_message()), std::move(message.move_payload()));
        return;
    }

    switch(message.get_message_type())
    {
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FROM_NODE_RESPONSE:
        {
            auto id = message.move_message();
            auto payload = message.move_payload();
            process_processed_get_by_id_from_node(std::move(id), std::move(payload));
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_RESPONSE:
        {
            auto id = message.get_message();
            auto payload = message.get_payload();
            process_processed_get_by_id(id, payload);
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_RESPONSE:
        {
            auto name = parsed_interest_name::parse_from_string(message.get_message());

            m_logger->debug("Name: {}", name.to_string());

            auto maybe_id = name.get_interest_parameter(0);
            auto maybe_type = name.get_interest_parameter(1);

            if(!maybe_id.has_value())
            {
                m_logger->warn("Received ID with no value in {}", __func__);
                break;
            }

            if(!maybe_type.has_value())
            {
                m_logger->warn("Received TYPE with no value in {}", __func__);
                break;
            }

            auto id = maybe_id.value();
            auto type = maybe_type.value();
            auto payload = message.get_payload();

            //TODO ask john about this
            process_processed_get_by_type(id, type, payload);
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::REGISTRY_PAYLOAD:
        {
            auto type = message.get_message();
            auto registry_payload = message.get_payload();
            process_registry_file(registry_payload, type);
            break;
        }
    }
}


void Worker::process_processed_get_by_id_from_node(std::string&& id, std::string&& payload)
{
    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(id),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FROM_NODE_RESPONSE,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        std::move(payload + "\n"),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->debug("Sending the data to the HTTP Server: {}", message->print_details());

    v_to_service->writeMessage(std::move(message));
}

void Worker::process_ack_message(std::string&& name, std::string&& payload)
{
    m_logger->info("Received ACK Message with name: {} payload: {}...", name, payload);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::CONTROL_MESSAGE,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        std::move(payload + "\n"),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->debug("Writing message to service queue:\n{}", message->print_details());

    v_to_service->writeMessage(std::move(message));
}

void Worker::processTemporalQueryRequest(const TemporalQuery& temporal_query)
{

    auto query_hash = temporal_query_hash_string(temporal_query);
    auto [_, inserted] = pending_temporal_queries.emplace(query_hash, temporal_query);
    if (!inserted) 
    {
        m_logger->info("Duplicate TemporalQuery {} for ID: {}", serialize_temporal_query(temporal_query), temporal_query.id);
        return;
    }

    m_logger->info("Got new temporal query with hash: {}", temporal_query_hash_string(temporal_query));
    auto id = temporal_query.id;

    ensureLeadingSlash(id);
    m_logger->info("Pushing TemporalQuery {} for ID: {}", serialize_temporal_query(temporal_query), id);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(serialize_temporal_query(temporal_query)),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST,
        snds_intra_message::SNDS_CLIENT_TYPES::SERVICE,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_consumer.get()->writeMessage(std::move(message));
}

void Worker::processTemporalQueryResponse
(
    const std::string& parent_query_hash, 
    const std::string& content,
    bool  last_interest
)
{

    auto it = pending_temporal_queries.find(parent_query_hash);
    if (it == pending_temporal_queries.end()) 
    {
        m_logger->error("No pending query for hash {}", parent_query_hash);
        return;
    }

    auto parent_query = it->second;
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
    auto pos = content.find_first_of("[{");
    std::string payload = content;
    if (pos != std::string::npos) 
    {
        payload = content.substr(pos);
        m_logger->debug("Removed extra characters from payload:\n{}", payload);
    }

    m_logger->info("Found stored temporal query: {}", serialize_temporal_query(parent_query));

    if(!parent_query.filters.empty())
    {
        m_logger->info("Applying filters to the data received");

        nlohmann::json document = nlohmann::json::parse(payload);

        std::unordered_set<std::string> keep;
        for (auto const& f : parent_query.filters) 
        {
            m_logger->debug("Adding filter: {} with value: {} to keep set", f.key, f.value);
            keep.insert(f.key);
        }

        nlohmann::json filtered = nlohmann::json::object();
        for (auto const& key : keep) 
        {
            if (document.contains(key)) 
            {
                filtered[key] = document[key];
            }
        }

        payload = filtered.dump(2);
    }

    m_logger->debug("Sending data to HTTP server: {}", payload);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        "",
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_RESPONSE,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        std::move(payload + "\n"),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_service->writeMessage(std::move(message));
}

void Worker::process_subscription_request(const std::string& id, const std::string& subURL)
{
    // Retrieve the subscription set for this ID.
    // If 'id' is new, operator[] creates an empty set.
    auto& subscriptions = subscribed_by_id_users[id];

    // Record whether this is the first subscription for the ID in order to send the interest
    bool first_subscription = subscriptions.empty();

    // Insert the new subscription URL into the set.
    bool is_new_subscription = subscriptions.insert(subURL).second;

    if (!is_new_subscription) 
    {
        m_logger->info("Duplicate subscription for ID: {0} and URL {1}. Ignored...", id, subURL);
        return;
    } 

    if(!first_subscription)
    {
        m_logger->info("This is not the first subscription for ID: {0} and URL {1}. Ignored...", id, subURL);
        return;
    }

    m_logger->info("Stored new subscription for ID: {0} and URL: {1}", id, subURL);

    m_logger->info("Pushing getByID subscription interest for ID: {}", id);
    // Build the interest name using the scope name and the prepared ID.
    // NOTE: you need to append you own SNDS ID that you want to subscribe to
    // This will return something like SNDS_ID_scope_0
    auto node_scope_name = getSNDSNodeScopeNameForSubscription(id, id_scope_prefix, number_of_snds_services);
    m_logger->info("Created new node scope name: {} for ID: {} with URL: {}", node_scope_name, id, subURL);

    auto interest_name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            node_scope_name, 
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_SUBSCRIPTION,
            {id, id_scope_prefix + snds_id}
        )
    );

    m_logger->info("Created interest name: {}", interest_name);
    // Create the intra-SNDS message (using the proper constructor)
    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(interest_name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->debug("Sending message to consumer...\n{}", message.get()->print_details());
    v_to_consumer.get()->writeMessage(std::move(message));
}

void Worker::process_new_get_by_id(const std::string& id)
{

    auto [_, new_request] = pending_get_by_id_requests.insert(id);
    if(!new_request)
    {
        m_logger->info("Duplicate get by ID request for ID: {}.", id);
        return;
    }

    m_logger->info("Inserted ID: {} into set. Pushing regular getByID request for ID: {}", id, id);

    auto interest_name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            id, 
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID,
            {id}
        )
    );

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(interest_name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_consumer.get()->writeMessage(std::move(message));
}

void Worker::process_processed_get_by_id(const std::string& id, const std::string& payload)
{
    m_logger->info("Subscription response with name {} received...", id);
    m_logger->debug("Checking if pending_get_by_id_requests contains {}...", id);

    if(pending_get_by_id_requests.erase(id) > 0)
    {
        m_logger->info("Got successful data for previous requestByID. Removing state...");

        auto message1 = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::string(id),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::ERASE_PENDING_ID,
            snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
            "",
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );

        v_to_dt->writeMessage(std::move(message1));

        auto message2 = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::string(id),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_RESPONSE,
            snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
            std::move(payload + "\n"),
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );

        v_to_service->writeMessage(std::move(message2));
    }
}

void Worker::process_new_get_by_type(const std::string& type)
{

    auto [_, new_request] = pending_get_by_type_requests.insert(type);

    if (!new_request)
    {
        m_logger->info("Duplicate getByType request for type: {} ignored.", type);
        return;
    }

    std::string full_type = type + registry_file_postfix;

    auto name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            full_type,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_TYPE,
            {type}
        )
    );

    m_logger->info("Pushing getByType request for type: {}", full_type);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_TYPE_REQUEST,
        snds_intra_message::SNDS_CLIENT_TYPES::SERVICE,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_consumer.get()->writeMessage(std::move(message));
}

void Worker::process_processed_get_by_type(const std::string& id, const std::string& type, std::string& payload)
{
    //TODO need to ask JOHN about this
    if(!pending_ids_to_type_map.contains(type))
    {
        m_logger->warn("Type {} is not in pending_ids_for_type_map", type);
        return; 
    }

    //sanity check
    if(!pending_get_by_type_requests.contains(type))
    {
        m_logger->warn("Type {} is not in pending_get_by_type_requests", type);
        return;
    }

    m_logger->info("Type {} is in pending ids for type map", type);
    // This gets us the types for an ID
    auto& ids = pending_ids_to_type_map.at(type);


    if(ids.erase(id) == 0)
    {
        m_logger->warn("ID {} was not in pending IDS for TYPE: {}", id, type);
        return;
    }

    m_logger->info("Sucessfully removed ID: {} from pending_ids_to_type for TYPE: {}", id, type);

    // we know that for a specific type the request is one at a time
    auto& items = payloads_for_get_by_type[type]; // creates if missing

    items.push_back(TypePayload{id, std::move(payload)});

    if(ids.size() == 0)
    {
        m_logger->info("Last ID received for TYPE. Erasing state...");
        // Build final JSON payload: { "type": "...", "payloads": [ ... ] }
        nlohmann::json j;
        j["type"] = type;
        j["items"] = nlohmann::json::array();
        for (const auto& it : items) \
        {
            j["items"].push_back({ {"id", it.id}, {"payload", it.payload} });
        }
        std::string serialized_payload = j.dump(2);
        pending_ids_to_type_map.erase(type);
        pending_get_by_type_requests.erase(type);
        payloads_for_get_by_type.erase(type);

        //TODO I have no clue what is this
        //auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        //(
        //    std::string(id),
        //    snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::ERASE_PENDING_ID,
        //    snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        //    "",
        //    snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        //);

        //v_to_dt->writeMessage(std::move(message));

        auto service_message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::string(id),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GENERIC,
            snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
            std::move(serialized_payload + "\n"),
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );

        v_to_service->writeMessage(std::move(service_message));
    }

}

void Worker::process_registry_file(const std::string& payload, const std::string& type) 
{

    // Parse the payload into a vector of IDs.
    std::vector<std::string> registry_ids = parse_ids_from_registry_file_payload
    (
        std::span<const uint8_t>
        (
            reinterpret_cast<const uint8_t*>(payload.data()),
            payload.size()
        ),
        m_logger
    );


    
    m_logger->info("Size of ids in registry file: {}", registry_ids.size());
    
    // Create a set to hold all the IDs from the registry file.
    std::unordered_set<std::string> id_set;
    
    // Process each extracted ID.
    for (auto& id : registry_ids) 
    {

        m_logger->debug("Processing registry id: {}", id);
        
        // Insert the id into the set.
        id_set.insert(id);
        
        // Update pendingTypesForIDMap: for this id, add the type name.
        // TODO ask john about this
        // pending_types_to_id_map[id].insert(type);


        auto name = parsed_interest_name::to_custom_ndn_name
        (
            parsed_interest_name::make_parsed_interest_name
            (
                snds_prefix, 
                type, 
                parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE,
                {id, type}
            )
        );
        
        // Create a message for this id.
        auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
        (
            std::move(name),
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_FOR_TYPE_REQUEST,
            snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
            "",   // Empty message name.
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );
        
        m_logger->info("Pushing request to the consumer for id {}", id);
        v_to_consumer.get()->writeMessage(std::move(message));
    }
    
    // Update pending_ids_to_type_map with the registry file name as key and the set of ids.
    pending_ids_to_type_map[type] = std::move(id_set);
}