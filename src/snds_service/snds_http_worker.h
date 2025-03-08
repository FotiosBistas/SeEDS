#include "snds_service_base.h"

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
                  "THIS_IS_AN_SNDS_ACK",  // Pass the ackTag explicitly
                  serviceName)            // Then pass the serviceName
  {
  }

  void init();
};