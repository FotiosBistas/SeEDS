// snds_http_worker.h
#ifndef SNDS_HTTP_WORKER_H
#define SNDS_HTTP_WORKER_H

#include "snds_service_base.h"
#include "snds_utils.h"
#include <algorithm>  
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <span>

class HTTPWorker : public ServiceBase {
public:
  HTTPWorker(spdlog::level::level_enum logLevel = spdlog::level::info,
             std::string logFile = "HTTPWorker.log",
             uint8_t numberOfSndsSrvs = 1,
             std::string sndsPrefix = "/SNDS",
             std::string mySndsId = "0",
             std::string idScopePrefix = "/SNDS_ID_scope_",
             std::string registryFilePostfix = "_registry",
             std::string serviceName = "HTTPWorker")
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
    m_logger->info("HTTPWorker initialized.");
  }

  void init() override;

private:
  // Map for storing subscriptions: id -> list of subscriber URLs
  std::unordered_map<std::string, std::unordered_set<std::string>> subscribedBYID_users;
  // Map Type names to vectors of IDs 
  // TODO since IDs are unique for each type use a set @yannisT
	std::unordered_map<std::string, std::unordered_set<std::string>> myTypesRegistryMap;

  // Used for serving subscription requests
  std::unordered_map<std::string, std::unordered_set<std::string>> pendingTypesForIDMap;
  std::unordered_map<std::string, std::unordered_set<std::string>> pendingIDsForTypeMap;

  // Unordered sets for deduplicating pending requests.
  std::unordered_set<std::string> pendingGetByIDreqs;
  std::unordered_set<std::string> pendingGetByTYPEreqs;

  void processSubscriptionRequest(const std::string& id, const std::string& url);
  void processByIDRequest(const std::string& id, const bool subscription = false);
  void processByTYPERequest(const std::string& type, const bool subscription = false);
  void processByIDRequestSub(const std::string& id);
  void processByTYPERequestSub(const std::string& type);
  void processRegistryFileRequest(const std::string& payload, const std::string& name);

};

#endif // SNDS_HTTP_WORKER_H
