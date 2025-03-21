// snds_http_worker.h
#ifndef SNDS_HTTP_WORKER_H
#define SNDS_HTTP_WORKER_H

#include "snds_service_base.h"
#include <algorithm>  // For std::find
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>

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

  // Unordered sets for deduplicating pending requests.
  std::unordered_set<std::string> pendingGetByIDreqs;
  std::unordered_set<std::string> pendingGetByTYPEreqs;

  void processSubscriptionRequest(const std::string& id, const std::string& url);
  void processByIDRequest(const std::string& id);
  void processByTYPERequest(const std::string& type);

  /**
  @brief TODO I have no clue how is this used originally I assume its something
  like (@yannisT): 

    - Receive a sample payload:

    - Header: "##" (2 bytes, ignored)

    - Next byte: number of IDs (2)

    - Then, for each ID: one byte for the length and then the ID string.

    - First ID: length 3, content "car"

    - Second ID: length 3, content "bus"
  */ 
  std::vector<std::string> parseIDsFromRegistryFilePayload(std::span<const uint8_t> payload);

};

#endif // SNDS_HTTP_WORKER_H
