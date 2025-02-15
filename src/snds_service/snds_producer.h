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
  /**
   * @brief Processes subscription-type Interests.
   *
   * Expects a parsed Interest name with the following structure:
   *   - prefix: m_sndsPrefix (e.g., "/SNDS")
   *   - command: something like "SNDS_ID_scope_X"
   *   - params[0]: the ID (e.g., "car1")
   *   - params[1] (optional): the SNDS_NODE identifier
   *
   * If two parameters are present, the Interest is treated as a new subscription request.
   * If only one parameter is present, it is treated as a notification, and the SNDS nodes
   * subscribed to that ID are notified.
   *
   * @param name The Interest name.
   * @param interest The Interest packet.
   */
  void processSubscription(const ndn::Name& name, const ndn::Interest& interest);
  void processLocalIdRequest(const ndn::Name& name, const ndn::Interest& interest);

  /**
   * @brief Determines whether the specified command corresponds to a type advertisement.
   *
   * This function checks whether the given command (e.g., "CAR") is present in the
   * registry map (which stores known content types using keys such as "/CAR"). If the
   * registry contains an entry for "/" + command, it indicates that the Interest is
   * advertising a content type.
   *
   * @param command The command extracted from the Interest name.
   * @return true if the command is a type advertisement; false otherwise.
  */
  inline bool isTypeAdvertisement(const std::string& command) const {
      // The registry map keys are stored with a leading slash (e.g., "/CAR").
      return myTypesRegistryMap.find("/" + command) != myTypesRegistryMap.end();
  }

  bool isSubscriptionCommand(const std::string& command) const; 

  /**
   * @brief Ensures that the given string begins with a forward slash ('/').
   *
   * If the provided string is non-empty and does not start with '/',
   * this function prepends a '/' to the string.
   *
   * @param nm The name string to modify.
  */
  inline void ensureLeadingSlash(std::string& nm) {
      if (nm.empty() || nm.front() != '/') {
          nm.insert(nm.begin(), '/');
      }
  }

  void advertise() {
    m_logger->info("Advertising SNDSProducer with ID: {}", m_mySndsIdName);
    // Use inherited members such as m_face, m_mySndsIdName, etc.
  }

private: 
	std::unordered_map<std::string, std::vector<std::string>> myTypesRegistryMap; //maps Type names to vectors of IDs 
	std::unordered_map<std::string, std::vector<std::string>> subscribedBYID_SNDSnodes; //maps Type names to vectors of IDs 
};
