#include <ndn-cxx/face.hpp>
#include <spdlog/spdlog.h>
#include "snds_service_base.h"
#include "parsed_interest.h"
#include "temporal_query.h"

class SNDSProducer : public ServiceBase {
public:
  SNDSProducer(spdlog::level::level_enum logLevel = spdlog::level::info,
              std::string logFile = "Producer.log",
              uint8_t numberOfSndsSrvs = 1,
              std::string sndsPrefix = "/SNDS",
              std::string mySndsId = "0",
              std::string idScopePrefix = "/SNDS_ID_scope_",
              std::string registryFilePostfix = "_registry",
              std::string serviceName = "Producer")
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
      m_logger->info("SNDSProducer initialized with serviceName: {}", this->serviceName);
      myTypesRegistryMap["/CAR"] = std::unordered_set<std::string>{};
  }

  void init();

  void onInterest(const ndn::Interest& interest);
  void processRegistryRequest(ParsedInterest& interest);
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
   * @param interest The Parsed Interest packet that triggered this callback.
 */
  void processTypeAdvertisement(ParsedInterest& interest);

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
   * @param interest The Parsed Interest packet.
   */
  void processSubscription(ParsedInterest& interest);

  void processNotification(ParsedInterest& interest);

  /**
   * @brief Processes a getById request for a locally announced ID.
   *
   * This function assumes that for a local ID request the command itself represents
   * the requested ID (with a leading slash added, if necessary). It then checks if the
   * locally announced IDs map contains that ID. If found, the associated data is sent
   * back. Otherwise, it logs a warning and lists the available IDs.
   *
   * @param name The full Interest name.
   * @param interest The Interest packet.
   */
  void processLocalIdRequest(ParsedInterest& interest);

  void advertise() {
    m_logger->info("Advertising SNDSProducer with ID: {}", m_mySndsIdName);
    // Use inherited members such as m_face, m_mySndsIdName, etc.
  }

private:
	std::unordered_map<std::string, std::unordered_set<std::string>> myTypesRegistryMap; //maps Type names to vectors of IDs
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribedBYID_SNDSnodes; //maps Type names to vectors of IDs
	std::unordered_map<std::string, std::string> locallyAnnouncedIDsMap; //stores the IDs of locally advertised items and their data
};
