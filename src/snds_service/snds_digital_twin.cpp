#include "snds_digital_twin.hpp"
#include <array>



void DigitalTwin::init(){
    m_logger->info("Starting service: {}", this->serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Thread started.");


    while(running.load())
    {
        m_logger->debug("DigitalTwin: Reading packet...");

        std::unique_ptr<snds_intra_message::SNDS_INTRA_MESSAGE> message = v_to_dt->readMessage();
        if (message == nullptr) 
        {
            m_logger->debug("DigitalTwin: Queue shutdown detected, exiting loop.");
            break;
        }

        auto content = message->get_message();
        auto length = message->get_message_length();
        auto type = message->get_message_type();
        auto params = message->unpack_query_params_from_message();
        m_logger->debug("Extracted content from message {}", content);
        m_logger->debug("Extracted content length from message {}", length);
        m_logger->debug("Extracted type from message: {}", snds_intra_message::to_string(type));

        switch (type)
        {
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT:
        {
            const auto it = params.find(message->get_param_name_id_().data());
            if(it == params.end())
            {
                m_logger->error("Received ID advertisement with no ID.");
            }
            process_by_id_advertisement(it->second);
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT:
        {
            //process_by_id_and_type_advertisement();
            const auto id_iterator  = params.find(message->get_param_name_id_().data());
            if(id_iterator == params.end())
            {
                m_logger->error("Received TYPE advertisement with no ID.");
            }
            const auto type_iterator = params.find(message->get_param_name_type_().data());
            if(type_iterator == params.end())
            {
                m_logger->error("Received TYPE advertisement with no TYPE.");
            }
            process_by_id_and_type_advertisement(id_iterator->second, type_iterator->second);
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::ERASE_PENDING_ID:
        {
            //erase_pending_id();
            const auto id_iterator  = params.find(message->get_param_name_id_().data());
            if(id_iterator == params.end())
            {
                m_logger->error("Received TYPE advertisement with no ID.");
            }
            erase_pending_id(id_iterator->second);
            break;
        }
        case snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_ADVERTISEMENT:
        {
            const auto date_iterator = params.find(message->get_param_name_date_().data());
            if(date_iterator == params.end())
            {
                m_logger->error("Received TEMPORAL QUERY advertisement with no DATE.");
                return;
            }

            const auto id_iterator  = params.find(message->get_param_name_id_().data());
            if(id_iterator == params.end())
            {
                m_logger->error("Received TEMPORAL QUERY advertisement with no ID.");
                return;
            }

            const auto type_iterator = params.find(message->get_param_name_type_().data());
            if(type_iterator == params.end())
            {
                m_logger->warn("Received TEMPORAL QUERY advertisement with no TYPE.");
            }

            const auto get_or_empty = [&](auto it) -> std::string 
            {
                return (it == params.end()) ? std::string{} : it->second;
            };

            TemporalQuery query = 
            {
                Timeframe::NONE,
                date_iterator->second,
                id_iterator->second,
                get_or_empty(type_iterator),
                "",
                {}
            };

            std::array<std::string_view, 4> keys = 
            {
                message->get_param_name_date_(),
                message->get_param_name_id_(),
                message->get_param_name_type_(),
                message->get_param_name_count_(),
            };


            query.extras.reserve(params.size());
            for (auto const& [key, val] : params) 
            {
                const bool is_reserved = std::any_of
                (
                    keys.begin(), 
                    keys.end(),
                    [&](std::string_view r) { return key == r; }
                );

                if (!is_reserved) 
                {
                    query.extras.emplace(key, val);
                }
            }

            process_temporal_query_advertisement(query);
            break;
        }
        }
    }
}


void DigitalTwin::process_by_id_advertisement(const std::string& id)
{
    m_logger->info("Received by ID get request...");
    //ensureLeadingSlash(id);
    auto [_, new_request] = announced_ids.insert(id);
    if(!new_request)
    {
        m_logger->debug("ID {} is already in the announced_ids. Skipping...", id);
        return;
    }

    m_logger->debug("Pushed ID {} to the announced IDs vector...", id);

    std::string id_scope = getSNDSNodeScopeNameForSubscription(id, id_scope_prefix, number_of_snds_services);
    m_logger->debug("Extracted id Scope from message {}", id_scope);

    auto interest_name = parsed_interest_name::make_parsed_interest_name
    (
        snds_prefix, 
        id,
        parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::POST_ID,
        {id}
    );

    auto message1 = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        parsed_interest_name::to_custom_ndn_name(interest_name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT,
        snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->info("Pushing byID advertisement to the Producer...\n{}", message1.get()->print_details());
    v_to_producer->writeMessage(std::move(message1));

    interest_name = parsed_interest_name::make_parsed_interest_name
    (
        snds_prefix, 
        id_scope,
        parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::SCOPE,
        {id, id_scope}
    );

    auto message2 = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        parsed_interest_name::to_custom_ndn_name(interest_name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_WITH_SCOPE_ADVERTISEMENT,
        snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );

    m_logger->info("Pushing byID with Scope advertisement to the Consumer...\n{}", message2.get()->print_details());
    v_to_consumer->writeMessage(std::move(message2));

}

void DigitalTwin::process_by_id_and_type_advertisement(const std::string& id, const std::string& type)
{
    m_logger->info("Received by Type get request...");
    m_logger->debug("The Type is the following: {}", type);
    m_logger->debug("The ID is the following: {}", id);
    auto interest_name = parsed_interest_name::make_parsed_interest_name
    (
        type, 
        id,
        parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT,
        {id, type}
    );
    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        parsed_interest_name::to_custom_ndn_name(interest_name),
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_AND_TYPE_ADVERTISEMENT,
        snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );
    m_logger->info("Pushing byType advertisement to the Consumer...\n{}", message.get()->print_details());
    v_to_consumer->writeMessage(std::move(message));

}

void DigitalTwin::process_temporal_query_advertisement(const TemporalQuery& query)
{
    auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
    (
        serialize_temporal_query(query),    
        snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_ADVERTISEMENT,
        snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
        "",
        snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
    );
    m_logger->info("Pushing Temporal Query to Producer: {}", serialize_temporal_query(query));
    m_logger->info("Pushing message to producer: {}", message->print_details());
    v_to_producer->writeMessage(std::move(message));
}

void DigitalTwin::erase_pending_id(const std::string& id)
{
    m_logger->info("Received erase ID command for ID {}...", id);
    if(announced_ids.erase(id) > 0)
    {
        m_logger->debug("Successfully removed ID {}", id);
    };
}