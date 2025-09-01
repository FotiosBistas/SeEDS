#pragma once

#include <algorithm>  
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <span>

#include <nlohmann/json.hpp>  

#include "snds_service_base.hpp"
#include "parsed_interest_name.hpp"
#include "temporal_query.hpp"
#include "snds_utils.h"

class Worker: public ServiceBase {
public:
  	Worker(
		spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "Worker.log",
        size_t      number_of_snds_services = 1,
        std::string snds_prefix     = "SNDS",
        std::string snds_id         = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "Worker"
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
  	{
  	  	m_logger->info("Worker initialized.");
  	}

  void init() override;

private:
  	// Map for storing subscriptions: id -> list of subscriber URLs
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribed_by_id_users;
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribed_by_type_users;

	// TODO ask john about this 
	// simplify the request for the pending type
  	// std::unordered_map<std::string, std::unordered_set<std::string>> pending_types_to_id_map;
  	std::unordered_map<std::string, std::unordered_set<std::string>> pending_ids_to_type_map;

	struct TypePayload 
	{
    	std::string id;
    	std::string payload;
	};

	std::unordered_map<std::string, std::vector<TypePayload>> payloads_for_get_by_type;

  	// Unordered sets for deduplicating pending requests.
  	std::unordered_set<std::string> pending_get_by_id_requests;
  	std::unordered_set<std::string> pending_get_by_type_requests;
  	// keep track of the temporal queries that are not answered yet
  	// the key is the hash of the query
  	// also used to return specific fields from the JSON return for an ID
  	// an ID and its specified fields we want to keep
  	std::unordered_map<std::string, temporal_query::TemporalQuery> pending_temporal_queries;
	std::unordered_map<std::string, std::vector<nlohmann::json>> temporal_buffers;

  	void process_new_get_by_id(const std::string& id);
  	void process_processed_get_by_id(const std::string& id, const std::string& payload);
  	void process_subscription_request(const std::string& name, const std::string& url, const snds_intra_message::SNDS_INTRA_MESSAGE_TYPE msg_type);
  	void process_new_get_by_type(const std::string& type);
	//TODO need to ask JOHN about this
  	void process_processed_get_by_type(const std::string& id, const std::string& type, std::string& payload);
  	void process_registry_file(const std::string& payload, const std::string& name);
	void process_ack_message(std::string&& name, std::string&& payload);
    void process_processed_get_by_id_from_node(std::string&& id, std::string&& payload);
  	void process_temporal_query_request(const temporal_query::TemporalQuery& temporal_query);
  	void process_temporal_query_response(const std::string& parent_query_hash, const std::string& content, bool last_interest);

	nlohmann::json apply_filters_to_doc(const nlohmann::json& doc, const std::unordered_set<std::string>& keep);
	void _handle_consumer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message);
	void _handle_http_server_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message);
	void _handle_producer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message);

  	static constexpr const char* GET_PARAMETER_ID_TAG      = "id";
  	static constexpr const char* GET_PARAMETER_TYPE_TAG    = "type";
  	static constexpr const char* GET_PARAMETER_FIELDS_TAG  = "filters";
    static constexpr const char* GET_PARAMETER_SUB_URL_TAG = "suburl";
    static constexpr const char* GET_PARAMETER_IS_SUB_TAG  = "isSub";
  	// all of the below are temporal query related
  	static constexpr const char* GET_PARAMETER_DATE_TAG="date";
  	static constexpr const char* GET_PARAMETER_TIMEFRAME_TAG="timeframe";
  	static constexpr const char* GET_PARAMETER_COUNT_TAG="count";
  	static constexpr const char* GET_PARAMETER_PARENT_QUERY_HASH_TAG="parentQueryHash";
  	static constexpr const char* GET_PARAMETER_LAST_METAFILE_INTEREST_TAG="lastInterest";
};
