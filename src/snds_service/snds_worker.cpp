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

    m_logger->debug("Server intra message...");

    switch(message.get_message_type())
    {
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::GET_BY_ID_REQUEST:
        {
            auto params = message.unpack_query_params_from_message();
            auto id_iterator = params.find(message.get_param_name_id_().data());
            process_new_get_by_id(id_iterator->second); 
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST:
        {
            auto params = message.unpack_query_params_from_message();
            const temporal_query::TemporalQuery query = temporal_query::from_params(params);
            process_temporal_query_request(query);
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
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_SUBSCRIPTION:
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION:
        {
            auto params = message.unpack_query_params_from_message();
            const auto msg_type = message.get_message_type();
        
            // URL is required
            const auto url_key_sv = message.get_param_name_subscription_url_();
            const std::string url_key(url_key_sv.data(), url_key_sv.size());
            auto url_it = params.find(url_key);
            if (url_it == params.end() || url_it->second.empty()) 
            {
                m_logger->error("Received {} with empty: {}", snds_intra_message::to_string(msg_type), url_key);
                return;
            }
        
            // Decide which key we require based on the message type
            const bool by_id = (msg_type == snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION);
            const auto key_sv = by_id ? message.get_param_name_id_() : message.get_param_name_type_();
            const std::string key_name(key_sv.data(), key_sv.size());
        
            auto key_it = params.find(key_name);

            if (key_it == params.end() || key_it->second.empty()) 
            {
                m_logger->error("Received {} missing required: {}", snds_intra_message::to_string(msg_type), key_name);
                return;
            }
        
            process_subscription_request(key_it->second, url_it->second, msg_type);
            break;
        }
    }

}

void Worker::_handle_consumer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message)
{

    m_logger->debug("Consumer intra message...");

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
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::VERSION_RESPONSE:
        {
            auto payload = message.move_payload();
            auto params = message.parse_uri_query_to_map_raw(message.get_message());
            process_temporal_query_response
            (
                params[GET_PARAMETER_PARENT_QUERY_HASH_TAG], 
                std::move(payload), 
                params[GET_PARAMETER_LAST_METAFILE_INTEREST_TAG] == "true", 
                params[GET_PARAMETER_RETRIEVE_MODE] == "direct"
            );
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

void Worker::process_temporal_query_request(const temporal_query::TemporalQuery& temporal_query)
{

    auto query_hash = temporal_query::temporal_query_hash_string(temporal_query);
    auto [_, inserted_pending] = pending_temporal_queries.emplace(query_hash, temporal_query);
    if (!inserted_pending) 
    {
        m_logger->info("Duplicate TemporalQuery {} for ID: {}", temporal_query::serialize_temporal_query(temporal_query), temporal_query.id);
        return;
    }

    m_logger->info("Got new temporal query with hash: {}", temporal_query::temporal_query_hash_string(temporal_query));
    auto id = temporal_query.id;

    m_logger->info("Pushing TemporalQuery {} for ID: {}", temporal_query::serialize_temporal_query(temporal_query), id);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(temporal_query::serialize_temporal_query(temporal_query)),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST,
        snds_intra_message::SNDS_CLIENT_TYPES::SERVICE,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_consumer->writeMessage(std::move(message));
}

nlohmann::json Worker::apply_filters_to_doc(const nlohmann::json& doc, const std::unordered_set<std::string>& keep)
{
    if (keep.empty()) return doc;

    if (doc.is_object()) 
    {
        nlohmann::json out = nlohmann::json::object();
        for (const auto& k : keep) 
        {
            m_logger->debug("Trying to apply filter: {}", k);
            if (doc.contains(k))
            {
                m_logger->debug("Doc: {} contains filter: {}", doc, k);
                out[k] = doc[k];
            }
        }
        return out;

    }

    if (doc.is_array()) 
    {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& el : doc) 
        {
            if (!el.is_object()) continue;
            nlohmann::json obj = nlohmann::json::object();
            for (const auto& k : keep)
            {
                m_logger->debug("Trying to apply filter: {}", k);
                if (el.contains(k)) 
                {
                    m_logger->debug("Doc: {} contains filter: {}", doc, k);
                    obj[k] = el[k];
                }
            }
            out.push_back(std::move(obj));
        }
        return out;
    }

    return doc;
}

void Worker::process_temporal_query_response
(
    const std::string& parent_query_hash,
    const std::string& content,
    bool last_interest,
    bool is_direct_mode
)
{
    if (is_direct_mode) 
    {
        m_logger->info("Direct mode...");

        // Try to fetch parent query state if a hash was provided
        const bool have_hash = !parent_query_hash.empty();
        temporal_query::TemporalQuery parent_query{};
        bool have_parent = false;
        if (have_hash) 
        {
            if (auto itpq = pending_temporal_queries.find(parent_query_hash); itpq != pending_temporal_queries.end())
            {
                parent_query = itpq->second;
                have_parent  = true;
                m_logger->debug("Direct mode: found parent query for hash {} -> {}", parent_query_hash, temporal_query::serialize_temporal_query(parent_query));
            }
            else 
            {
                m_logger->debug("Direct mode: no parent state for hash {}", parent_query_hash);
            }
        }

        // Parse the item (single object expected)
        nlohmann::json item;
        if (!_extract_direct_temporal_item(content, item)) 
        {
            m_logger->error("Direct mode payload not understood. First 64 bytes: {}", hexdump(content, 0, 64));
            return;
        }

        // Clear pending GET_BY_ID/GET_BY_TYPE
        const std::string resolved_id   = item.value("id", item.value("_id", std::string{}));
        if (!resolved_id.empty()) 
        {
            const auto erased = pending_get_by_id_requests.erase(resolved_id);
            m_logger->debug("Cleared pending_get_by_id_requests for '{}': erased={}", resolved_id, erased);
        } 
        else 
        {
            m_logger->warn("Direct mode item had no 'id'/'_id'; cannot clear pending_get_by_id_requests");
        }

        const std::string resolved_type = item.value("type", std::string{});
        if (!resolved_type.empty()) 
        {
            const auto erasedT = pending_get_by_type_requests.erase(resolved_type);
            m_logger->debug("Cleared pending_get_by_type_requests for '{}': erased={}", resolved_type, erasedT);
        }

        // Helpers to collect filters from multiple sources and merge them
        auto extract_keep_from = [&](const nlohmann::json* node) 
        {
            std::unordered_set<std::string> keep;
            if (!node || node->is_null()) return keep;

            const auto& filters = *node;
            if (filters.is_array()) 
            {
                for (const auto& f : filters) 
                {
                    if (f.is_object()) 
                    {
                        if (auto it = f.find("key"); it != f.end() && it->is_string()) keep.insert(it->get<std::string>());
                    } 
                    else if (f.is_string()) 
                    {
                        keep.insert(f.get<std::string>());
                    }
                }
            } 
            else if (filters.is_object()) 
            {
                for (auto it = filters.begin(); it != filters.end(); ++it) 
                {
                    if (it.value().is_boolean() ? it.value().get<bool>() : true) keep.insert(it.key());
                }
            }
            return keep;
        };

        auto parse_qs_filters = [&](const std::string& s) 
        {
            std::unordered_set<std::string> keep;
            // extract query string after "/?"
            const std::string marker = "/?";
            auto pos = s.find(marker);
            if (pos == std::string::npos) return keep;
            std::string_view qs = std::string_view(s).substr(pos + marker.size());

            // parse k=v&...
            size_t start = 0;
            while (start <= qs.size()) 
            {
                size_t amp = qs.find('&', start);
                std::string_view pair = (amp == std::string::npos) ? qs.substr(start) : qs.substr(start, amp - start);
                if (!pair.empty()) 
                {
                    size_t eq = pair.find('=');
                    std::string key = url_decode(eq == std::string::npos ? std::string(pair) : std::string(pair.substr(0, eq)));
                    std::string val = url_decode(eq == std::string::npos ? std::string() : std::string(pair.substr(eq + 1)));
                    if (key == "filters" && !val.empty()) 
                    {
                        // Reuse your temporal_query parser for consistency
                        auto fl = temporal_query::parse_filters(val);
                        for (const auto& f : fl) keep.insert(f.key);
                    }
                }
                if (amp == std::string::npos) break;
                start = amp + 1;
            }
            return keep;
        };

        auto merge_keep = [](std::unordered_set<std::string>& dst, const std::unordered_set<std::string>& src)
        {
            dst.insert(src.begin(), src.end());
        };

        // Build the keep set with this precedence:
        // 1) item.filters (or item.query.filters / item._meta.filters)
        // 2) querystring filters from `content`
        // 3) parent_query.filters (if state exists)
        std::unordered_set<std::string> keep;

        if (auto itf = item.find("filters"); itf != item.end()) merge_keep(keep, extract_keep_from(&(*itf)));
        if (keep.empty()) 
        {
            if (auto q = item.find("query"); q != item.end()) 
            {
                if (auto it = q->find("filters"); it != q->end()) merge_keep(keep, extract_keep_from(&(*it)));
            }
        }
        if (keep.empty()) 
        {
            if (auto m = item.find("_meta"); m != item.end()) 
            {
                if (auto it = m->find("filters"); it != m->end()) merge_keep(keep, extract_keep_from(&(*it)));
            }
        }

        // merge querystring filters
        merge_keep(keep, parse_qs_filters(content));

        // merge parent query filters last (lowest precedence if others are present)
        if (have_parent && !parent_query.filters.empty()) 
        {
            std::unordered_set<std::string> k2;
            for (const auto& f : parent_query.filters) k2.insert(f.key);
            merge_keep(keep, k2);
        }

        if (!keep.empty()) 
        {
            m_logger->debug("Applying {} filter keys in direct mode", keep.size());
            item = apply_filters_to_doc(item, keep);
        } 
        else 
        {
            m_logger->debug("Direct mode: no filters discovered; returning full item");
        }

        // Build response
        nlohmann::json bundle;
        bundle["mode"]  = "direct";
        bundle["items"] = nlohmann::json::array();
        bundle["items"].push_back(std::move(item));

        if (have_parent) 
        {
            bundle["parentQueryHash"] = parent_query_hash;
            bundle["query"]           = temporal_query::serialize_temporal_query(parent_query);
        }

        std::string out = bundle.dump(2);
        auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>(
            /*name*/"",
            snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_RESPONSE,
            snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
            std::move(out += "\n"),
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );
        v_to_service->writeMessage(std::move(message));

        // If we had parent state and this was the last chunk, clean it up
        if (have_parent && last_interest) 
        {
            temporal_buffers.erase(parent_query_hash);
            pending_temporal_queries.erase(parent_query_hash);
        }
        return;
    }

    // =======================
    // AGGREGATED/MANIFEST MODE (requires parent query state)
    // =======================
    auto it = pending_temporal_queries.find(parent_query_hash);
    if (it == pending_temporal_queries.end()) 
    {
        m_logger->error("No pending query for hash {}", parent_query_hash);
        return;
    }

    // Safe: we have state
    const auto parent_query = it->second;

    m_logger->debug("Parent Query is: {}", temporal_query::serialize_temporal_query(parent_query));

    // Precompute keep-set once for this batch
    std::unordered_set<std::string> keep;
    if (!parent_query.filters.empty()) 
    {
        for (const auto& f : parent_query.filters) keep.insert(f.key);
    }

    // Split full buffer into balanced JSON slices
    auto slices = extract_all_json_slices(content);
    if (slices.empty()) 
    {
        m_logger->error("No JSON found. Hex head: {}", hexdump(content, 0, 64));
        if (last_interest) 
        {
            temporal_buffers.erase(parent_query_hash);
            pending_temporal_queries.erase(it);
        }
        return;
    }

    // Ensure a vector exists for this query hash
    auto& bucket = temporal_buffers[parent_query_hash];

    // Process every slice -> sanitize -> parse -> (optionally) filter -> buffer
    for (size_t idx = 0; idx < slices.size(); ++idx) 
    {
        std::string sanitized = fix_mixed_utf8_cp1252(slices[idx]);

        if (!nlohmann::json::accept(sanitized)) 
        {
            m_logger->error("Slice #{} invalid after sanitization; skipping.", idx);
            continue;
        }

        nlohmann::json doc = nlohmann::json::parse(sanitized, nullptr, false);
        if (doc.is_discarded()) 
        {
            m_logger->error("Slice #{} parse discarded; skipping.", idx);
            continue;
        }

        m_logger->debug("Processing slice/doc: {} and trying to apply filters...", doc.dump(2));

        nlohmann::json filtered = apply_filters_to_doc(doc, keep);
        bucket.push_back(std::move(filtered));
    }

    if (!last_interest) 
    {
        m_logger->debug("Buffered {} items for hash {} (waiting for lastInterest).", temporal_buffers[parent_query_hash].size(), parent_query_hash);
        return;
    }

    // Bundle and send once (last interest)
    nlohmann::json bundle;
    bundle["mode"]            = "manifest";
    bundle["parentQueryHash"] = parent_query_hash;
    bundle["query"]           = temporal_query::serialize_temporal_query(parent_query);
    bundle["items"]           = nlohmann::json::array();

    if (auto tbi = temporal_buffers.find(parent_query_hash); tbi != temporal_buffers.end()) 
    {
        for (auto& el : tbi->second) bundle["items"].push_back(std::move(el));
    }

    std::string out = bundle.dump(2);
    m_logger->debug("Sending BULK temporal response ({} items) for hash {}", bundle["items"].size(), parent_query_hash);
    m_logger->debug("Response looks like this:\n{}", out);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        "",
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_RESPONSE,
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        std::move(out += "\n"),
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    v_to_service->writeMessage(std::move(message));

    // Cleanup state
    temporal_buffers.erase(parent_query_hash);
    pending_temporal_queries.erase(it);
}


void Worker::process_subscription_request
(
    const std::string& name,
    const std::string& subURL,
    const snds_intra_message::SNDS_INTRA_MESSAGE_TYPE msg_type
)
{
    // Pick container
    auto& container = (msg_type == snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION)? subscribed_by_id_users: subscribed_by_type_users;

    auto& subscriptions = container[name]; // creates empty set if missing

    const bool first_subscription = subscriptions.empty();

    const bool is_new_subscription = subscriptions.insert(subURL).second;

    if (!is_new_subscription) 
    {
        m_logger->info("Duplicate subscription for Name: {} and URL {}. Ignored...", name, subURL);
        return;
    }

    if (!first_subscription) 
    {
        m_logger->info("Not first subscription for Name: {} (URL {}). No interest sent.", name, subURL);
        return;
    }

    m_logger->info("Stored FIRST subscription for Name: {} and URL: {}", name, subURL);

    // Build scope + interest according to type
    const bool by_id = (msg_type == snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_SUBSCRIPTION);

    std::string node_scope_name = "";

    if(by_id)
    {
        node_scope_name = getSNDSNodeScopeNameForSubscription(name, id_scope_prefix, number_of_snds_services);
        m_logger->info("Created node scope name: {} for Name: {} with URL: {}", node_scope_name, name, subURL);
        m_logger->info("Hi this is me: {} I'm going to ask node: {} to subscribe to: {}", snds_id_scope_name, node_scope_name, name);
    }
    else
    {
        node_scope_name = name;
        m_logger->info("Since this is a byType subscription we don't need to create a scope. Using the Type: {} as a scope", name);
    }

    const auto interest_kind = by_id ? parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_ID_SUBSCRIPTION : parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::NEW_TYPE_SUBSCRIPTION;

    auto interest_name = parsed_interest_name::to_custom_ndn_name
    (
        parsed_interest_name::make_parsed_interest_name
        (
            snds_prefix,
            node_scope_name,
            interest_kind,
            {name, snds_id_scope_name}
        )
    );

    m_logger->info("Created interest name: {}", interest_name);

    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        std::move(interest_name),
        msg_type,  // BY_ID_SUBSCRIPTION or BY_TYPE_SUBSCRIPTION
        snds_intra_message::SNDS_CLIENT_TYPES::WORKER,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->debug("Sending message to consumer...\n{}", message->print_details());
    v_to_consumer->writeMessage(std::move(message));
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
            "",
            snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
        );
        
        m_logger->info("Pushing request to the consumer for id {}", id);
        v_to_consumer->writeMessage(std::move(message));
    }
    
    // Update pending_ids_to_type_map with the registry file name as key and the set of ids.
    pending_ids_to_type_map[type] = std::move(id_set);
}

// Optional: normalize to a friendlier {id,date,type} when present.
static nlohmann::json _normalize_temporal_entry_to_item(const nlohmann::json& entry)
{
    nlohmann::json out = nlohmann::json::object();

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
                // Keep raw; convert later if you need ISO-8601
                out["date"] = idobj["timestamp"]["$date"];
            }
        }
        if (entry.contains("type")) out["type"] = entry["type"];
    }

    return out.empty() ? entry : out;
}

// Extract the single item from a "direct" payload.
// Supports:
//  A) {"mode":"direct","items":[{...}]}
//  B) [ { "<bucket or empty>": { ...doc... } } ]          // legacy
//  C) [ { "<bucket>": [ { ...doc... } ] } ]               // NEW (your case)
bool Worker::_extract_direct_temporal_item(const std::string& content, nlohmann::json& out)
{
    if (!nlohmann::json::accept(content)) return false;
    nlohmann::json j = nlohmann::json::parse(content, nullptr, false);
    if (j.is_discarded()) return false;

    // --- Schema C: [ { "<bucket>": [ { ... } ] } ] ---
    if (j.is_array() && !j.empty())
    {
        const auto& first = j.front();
        if (first.is_object() && first.size() == 1)
        {
            const auto& val = first.begin().value();
            if (val.is_array() && val.size() == 1 && val.front().is_object())
            {
                out = _normalize_temporal_entry_to_item(val.front());
                return true;
            }
            // --- Schema B (legacy): [ { "<bucket>": { ... } } ] ---
            if (val.is_object())
            {
                out = _normalize_temporal_entry_to_item(val);
                return out.is_object();
            }
        }
    }

    // --- Schema A: {"mode":"direct","items":[{...}]} ---
    if (j.is_object() && j.value("mode","") == "direct")
    {
        const auto& items = j.value("items", nlohmann::json::array());
        if (items.is_array() && !items.empty() && items.front().is_object())
        {
            out = _normalize_temporal_entry_to_item(items.front());
            return true;
        }
    }

    // Fallback: a plain object is the item
    if (j.is_object()) { out = _normalize_temporal_entry_to_item(j); return true; }

    return false;
}