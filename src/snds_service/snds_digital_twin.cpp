#include "snds_digital_twin.h"


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

        auto content = message.get()->getMessage();
        auto length = message.get()->getMessageLength();
        m_logger->debug("Extracted content from message {}", content);
        m_logger->debug("Extracted content length from message {}", length);


        std::string id      = std::string(get_from_get_request(content, "id"));
        std::string type    = std::string(get_from_get_request(content, "type"));
        std::string id_scope = getSNDSNodeScopeNameForSubscription(id, id_scope_prefix, number_of_snds_services);


        m_logger->debug("Extracted id from message {}", id);
        m_logger->debug("Extracted id Scope from message {}", id_scope);

        if(content.find(ERASE_PREFIX) != std::string::npos)
        {
            m_logger->info("Received erase ID command for ID {}...", id);
            if(announcedIDs.erase(id) > 0)
            {
                m_logger->debug("Successfully removed ID {}", id);
            };
            continue;
        }


        //TODO we can differentiate messages based on type
        //I don't know why are we doing this
        auto params = parse_query_params(content);
        auto it_date = params.find("date");


        if(it_date != params.end()){
            m_logger->info("Received temporal query request...");
            m_logger->debug("Extracted Date from message {}", it_date->second);
            TemporalQuery query{
                Timeframe::NONE,
                it_date->second,
                id,
                type,
                "",
                {}  // extras below
            };

            for (auto const& k : {"id","date","type","count"}) {
                params.erase(k);
            }

            query.extras = std::move(params);


            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                serialize_temporal_query(query),    
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST,
                snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );
            m_logger->info("Pushing Temporal Query to Producer: {}", serialize_temporal_query(query));
            v_to_producer.get()->writeMessage(std::move(message));
            continue;
        }

        if(!id.empty())
        {
            m_logger->info("Received by ID get request...");
            //ensureLeadingSlash(id);
            auto [_, new_request] = announcedIDs.insert(id);
            if(!new_request)
            {
                m_logger->debug("ID {} is already in the announcedIDsVec. Skipping...", id);
                continue;
            }

            m_logger->debug("Pushed ID {} to the announced IDs vector...", id);

            auto interest_name = parsed_interest_name::makeParsedInterestName
            (
                snds_prefix, 
                id,
                interest_message_type::INTEREST_MESSAGE_TYPE::POST_ID,
                {id}
            );

            auto message1 = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                parsed_interest_name::toCustomNDNName(interest_name),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_ADVERTISEMENT,
                snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );

            m_logger->info("Pushing byID advertisement to the Producer...\n{}", message1.get()->printDetails());
            v_to_producer->writeMessage(std::move(message1));

            interest_name = parsed_interest_name::makeParsedInterestName
            (
                snds_prefix, 
                id_scope,
                interest_message_type::INTEREST_MESSAGE_TYPE::SCOPE,
                {id}
            );

            auto message2 = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                parsed_interest_name::toCustomNDNName(interest_name),
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_ID_WITH_SCOPE_ADVERTISEMENT,
                snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );

            m_logger->info("Pushing byID with Scope advertisement to the Consumer...\n{}", message2.get()->printDetails());
            v_to_consumer->writeMessage(std::move(message2));
        }


        if(!type.empty()){
            m_logger->info("Received by Type get request...");
            ensureLeadingSlash(type);
            m_logger->debug("The Type is the following: {}", type);
            ensureLeadingSlash(id);
            m_logger->debug("The ID is the following: {}", id);
            auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
            (
                type+id,
                snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::BY_TYPE_RESPONSE,
                snds_intra_message::SNDS_CLIENT_TYPES::DIGITAL_TWIN,
                ""
            );
            m_logger->info("Pushing byType advertisement to the Consumer...\n{}", message.get()->printDetails());
            v_to_consumer->writeMessage(std::move(message));
        }
    }
}