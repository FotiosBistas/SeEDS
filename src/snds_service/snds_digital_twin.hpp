#pragma once

#include "parsed_interest_name.hpp"
#include "snds_service_base.hpp"
#include "temporal_query.hpp"

class DigitalTwin: public ServiceBase{

public:

    DigitalTwin
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "DigitalTwin.log",
        size_t      number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_id = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "DigitalTwin"
    ):ServiceBase
    (
        logLevel,
        logFile,
        number_of_snds_services,
        snds_prefix,
        snds_id,
        id_scope_prefix,
        registryFilePostfix,
        "THIS_IS_AN_SNDS_ACK",
        serviceName
    )
    {}


    void init();
    void process_by_id_advertisement(const std::string& id);
    void process_by_id_and_type_advertisement(const std::string& id, const std::string& type);
    void process_temporal_query_advertisement(const temporal_query::TemporalQuery& query);
    void erase_pending_id(const std::string& id);

private:
	std::unordered_set<std::string> announced_ids; //stores local annnounced ids
};