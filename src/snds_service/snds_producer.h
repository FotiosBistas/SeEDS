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
    myTypesRegistryMap["/CAR"] = std::vector<std::string>{};
  }

  void onInterest(const ndn::Interest& interest);
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

  /**
   * @brief Determines whether the given command corresponds to a subscription request.
   *
   * This function returns true if the command string is either "SUBSCRIBE" or "NOTIFY",
   * which are the two recognized commands for subscription requests.
   *
   * @param command The command extracted from the Interest name.
   * @return true if the command is a subscription command; false otherwise.
  */
  inline bool isSubscriptionCommand(const std::string& command) const {
      return (command == "SUBSCRIBE" || command == "NOTIFY");
  }

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

  /**
   * @brief Serializes a vector of strings into a binary payload.
   *
   * The payload format is:
   *   [ number_of_IDs (1 byte) |
   *     id1_length (1 byte) | id1 characters |
   *     id2_length (1 byte) | id2 characters | ... ]
   *
   * For example, if the vector contains {"car1", "bus2"},
   * the payload will consist of:
   *   - 1 byte: the number 2 (indicating 2 IDs),
   *   - 1 byte: the length of "car1" (which is 4),
   *   - 4 bytes: the characters of "car1",
   *   - 1 byte: the length of "bus2" (which is 4),
   *   - 4 bytes: the characters of "bus2".
   *
   * @param ids A vector of ID strings.
   * @return A vector of char containing the serialized payload.
  */
  std::vector<char> getRegistryFilePayload(const std::vector<std::string>& ids);

  void advertise() {
    m_logger->info("Advertising SNDSProducer with ID: {}", m_mySndsIdName);
    // Use inherited members such as m_face, m_mySndsIdName, etc.
  }

  // Getters and setters for private members

  inline const std::unordered_map<std::string, std::vector<std::string>>& getMyTypesRegistryMap() const {
    return myTypesRegistryMap;
  }

  // Used in testing to inject sample data
  inline std::unordered_map<std::string, std::vector<std::string>>& getMutableMyTypesRegistryMap() {
    return myTypesRegistryMap;
  }

  inline void setMyTypesRegistryMap(const std::unordered_map<std::string, std::vector<std::string>>& map) {
    myTypesRegistryMap = map;
  }

  inline const std::unordered_map<std::string, std::vector<std::string>>& getSubscribedById() const {
    return subscribedBYID_SNDSnodes;
  }

  // Used in testing to inject sample data
  inline std::unordered_map<std::string, std::vector<std::string>>& getMutableSubscribedById() {
    return subscribedBYID_SNDSnodes;
  }

  inline void setSubscribedById(const std::unordered_map<std::string, std::vector<std::string>>& map) {
    subscribedBYID_SNDSnodes = map;
  }

  inline const std::unordered_map<std::string, std::string>& getLocallyAnnouncedIds() const {
    return locallyAnnouncedIDsMap;
  }

  // Used in testing to inject sample data
  inline std::unordered_map<std::string, std::string>& getMutableLocallyAnnouncedIds() {
    return locallyAnnouncedIDsMap;
  }

  inline void setLocallyAnnouncedIds(const std::unordered_map<std::string, std::string>& map) {
    locallyAnnouncedIDsMap = map;
  }

private:
	std::unordered_map<std::string, std::vector<std::string>> myTypesRegistryMap; //maps Type names to vectors of IDs
	std::unordered_map<std::string, std::vector<std::string>> subscribedBYID_SNDSnodes; //maps Type names to vectors of IDs
	std::unordered_map<std::string, std::string> locallyAnnouncedIDsMap; //stores the IDs of locally advertised items and theit data
};
