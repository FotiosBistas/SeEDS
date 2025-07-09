#pragma once

#include <algorithm>  
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <span>

#include <nlohmann/json.hpp>  
#include "snds_service_base.h"
#include "temporal_query.h"
#include "snds_utils.h"

class Worker: public ServiceBase {
public:
  Worker(spdlog::level::level_enum logLevel = spdlog::level::info,
             std::string logFile = "Worker.log",
             uint8_t numberOfSndsSrvs = 1,
             std::string sndsPrefix = "/SNDS",
             std::string mySndsId = "0",
             std::string idScopePrefix = "/SNDS_ID_scope_",
             std::string registryFilePostfix = "_registry",
             std::string serviceName = "Worker")
    : ServiceBase(logLevel,
                  logFile,
                  numberOfSndsSrvs,
                  sndsPrefix,
                  mySndsId,
                  idScopePrefix,
                  registryFilePostfix,
                  "THIS_IS_AN_SNDS_ACK",
                  serviceName)
  {
    m_logger->info("Worker initialized.");
  }

  void init() override;

private:
  // Map for storing subscriptions: id -> list of subscriber URLs
  std::unordered_map<std::string, std::unordered_set<std::string>> subscribedBYID_users;
  // <type, vector<urls>> maps subscribed types to User URLs
  // TODO this is not used anywhere
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribedBYTYPE_users;

  // Used for serving subscription requests
  std::unordered_map<std::string, std::unordered_set<std::string>> pendingTypesForIDMap;
  std::unordered_map<std::string, std::unordered_set<std::string>> pendingIDsForTypeMap;

  // Unordered sets for deduplicating pending requests.
  std::unordered_set<std::string> pendingGetByIDreqs;
  std::unordered_set<std::string> pendingGetByTYPEreqs;
  // keep track of the temporal queries that are not answered yet
  // the key is the hash of the query
  // also used to return specific fields from the JSON return for an ID
  // an ID and its specified fields we want to keep
  std::unordered_map<std::string, TemporalQuery> pending_temporal_queries;

  void processSubscriptionRequest(const std::string& id, const std::string& url);
  void processByIDRequest(const std::string& id, const bool subscription = false);
  void processByTYPERequest(const std::string& type, const bool subscription = false);
  void processByIDResponse(const std::string& name);
  void processByTYPEResponse(const std::string& name);
  void processRegistryFileRequest(const std::string& payload, const std::string& name);
  void processTemporalQueryRequest(const TemporalQuery& temporal_query);
  void processTemporalQueryResponse(const std::string& parent_query_hash, const std::string& content);

  static constexpr const char* GET_PARAMETER_ID_TAG="id";
  static constexpr const char* GET_PARAMETER_TYPE_TAG="type";
  static constexpr const char* GET_PARAMETER_FIELDS_TAG = "filters";
  // all of the below are temporal query related
  static constexpr const char* GET_PARAMETER_DATE_TAG="date";
  static constexpr const char* GET_PARAMETER_TIMEFRAME_TAG="timeframe";
  static constexpr const char* GET_PARAMETER_COUNT_TAG="count";
  static constexpr const char* GET_PARAMETER_PARENT_QUERY_HASH_TAG="parentQueryHash";
};
