// snds_http_worker.h
#ifndef SNDS_HTTP_WORKER_H
#define SNDS_HTTP_WORKER_H

#include "snds_service_base.h"
#include <algorithm>  
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
  // Map Type names to vectors of IDs 
  // TODO since IDs are unique for each type use a set @yannisT
	std::unordered_map<std::string, std::unordered_set<std::string>> myTypesRegistryMap;

  // Unordered sets for deduplicating pending requests.
  std::unordered_set<std::string> pendingGetByIDreqs;
  std::unordered_set<std::string> pendingGetByTYPEreqs;

  void processSubscriptionRequest(const std::string& id, const std::string& url);
  void processByIDRequest(const std::string& id);
  void processByTYPERequest(const std::string& type);

  //TODO these functions below require their own data type
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

/**
   * @brief Serializes a set of IDs into a binary payload.
   *
   * The payload is structured as follows:
   *   [number_of_ids (1 byte)]
   *   [id1_length (1 byte)] [id1 bytes...]
   *   [id2_length (1 byte)] [id2 bytes...]
   *   ...
   *
   * - The first byte holds the number of IDs (max 255).
   * - Each ID is preceded by a 1-byte length (max length 255).
   *
   * @param ids An unordered_set of strings representing the IDs.
   * @return A vector of std::byte containing the binary payload.
   *
   * @throws std::length_error if the number of IDs exceeds 255 or any ID's length exceeds 255.
   */
  std::string createRegistryFilePayload(const std::unordered_set<std::string>& ids);
};

#endif // SNDS_HTTP_WORKER_H
