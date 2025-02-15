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
  /**
   * @brief Processes an Interest that advertises a new content type.
   *
   * This function handles Interests that signal a new advertisement for a content type.
   * It expects the parsed Interest name to have the following format:
   * - The first component is the service prefix (e.g., "/SNDS").
   * - The second component is the command, representing the content type (e.g., "CAR").
   * - The third component (params[0]) is the identifier (ID) for that content type.
   *
   * If the Interest does not contain an ID, the function logs a warning and ignores the Interest.
   * If the ID is already registered for the type (as found in the myTypesRegistryMap), a warning is logged
   * and a warning Data packet is sent back.
   * Otherwise, the new ID is added to the registry, and an acknowledgment (ACK) is sent back.
   *
   * @param name The Interest name received.
   * @param interest The Interest packet that triggered this callback.
 */
  void processTypeAdvertisement(const ndn::Name& name, const ndn::Interest& interest);
  void processSubscription(const ndn::Name& name, const ndn::Interest& interest);
  void processLocalIdRequest(const ndn::Name& name, const ndn::Interest& interest);
  bool isTypeAdvertisement(const std::string& command) const;
  bool isSubscriptionCommand(const std::string& command) const; 

  void advertise() {
    m_logger->info("Advertising SNDSProducer with ID: {}", m_mySndsIdName);
    // Use inherited members such as m_face, m_mySndsIdName, etc.
  }

private: 
	std::unordered_map<std::string, std::vector<std::string>> myTypesRegistryMap; //maps Type names to vectors of IDs 
};
