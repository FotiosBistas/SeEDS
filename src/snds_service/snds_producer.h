#include <spdlog/spdlog.h>
#include "snds_service_base.h"
#include "parsed_interest.h"

class SNDSProducer : public ServiceBase {
public:
  // Constructor for SNDSProducer
  SNDSProducer(spdlog::level::level_enum logLevel = spdlog::level::info,
               const std::string logFile = "Producer.log",
               uint8_t numberOfSndsSrvs = 1,
               const std::string sndsPrefix = "/SNDS",
               const std::string mySndsId = "0",
               const std::string idScopePrefix = "/SNDS_ID_scope_",
               const std::string registryFilePostfix = "_registry")
    : ServiceBase(logLevel, logFile, numberOfSndsSrvs, sndsPrefix, mySndsId, idScopePrefix, registryFilePostfix) 
  {
    // Any additional initialization specific to SNDSProducer can go here.
  }

  void onInterest(const ndn::Interest& interest);
  void onRegisterFailed(const ndn::Name& prefix, const std::string& reason);
  void processRegistryRequest(const ndn::Name& name, const ndn::Interest& interest);
  void processTypeAdvertisement(const ndn::Name& name, const ndn::Interest& interest);
  void processSubscription(const ndn::Name& name, const ndn::Interest& interest);
  void processLocalIdRequest(const ndn::Name& name, const ndn::Interest& interest);
  bool isTypeAdvertisement(const std::string& command) const;
  bool isSubscriptionCommand(const std::string& command) const; 

  void advertise() {
    m_logger->info("Advertising SNDSProducer with ID: {}", m_mySndsIdName);
    // Use inherited members such as m_face, m_mySndsIdName, etc.
  }
};
