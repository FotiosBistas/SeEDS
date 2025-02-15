#include "snds_producer.h"


void SNDSProducer::onInterest(const ndn::Interest& interest) {
  ndn::Name name = interest.getName();
  m_logger->info("Received in function: {0}, name: {1}", __FUNCTION__, name.toUri());

  if (name.size() < 2) {
    m_logger->warn("Interest name {} in {} is too short. Ignoring...", name.toUri(), __FUNCTION__);
    return;
  }

  ParsedInterest pi = parseInterestName(name);

  // If the command contains the registry postfix, handle registry requests.
  if (pi.command.find(m_registryFilePostfix) != std::string::npos) {
    processRegistryRequest(name, interest);
    return;
  }

  // Dispatch based on the command.
  if (isTypeAdvertisement(pi.command)) {
    processTypeAdvertisement(name, interest);
    return;
  }
  else if (isSubscriptionCommand(pi.command)) {
    processSubscription(name, interest);
    return;
  }
  else if (!pi.params.empty()) {
    processLocalIdRequest(name, interest);
    return;
  }

  m_logger->info("Interest left unprocessed. No matching handler found.");
}

std::vector<char> SNDSProducer::getRegistryFilePayload(const std::vector<std::string>& ids) {
  // Calculate the total length: 1 byte for count + 1 byte per ID for its length + ID characters.
  size_t totalLength = 1;
  for (const auto& id : ids) {
      totalLength += 1 + id.size();
  }

  // Reserve capacity in the vector for efficiency.
  std::vector<char> payload;
  payload.reserve(totalLength);

  // Write the number of IDs as the first byte.
  payload.push_back(static_cast<char>(ids.size()));

  // For each ID, write its length and then its characters.
  for (const auto& id : ids) {
      payload.push_back(static_cast<char>(id.size()));
      payload.insert(payload.end(), id.begin(), id.end());
  }

  return payload;
}

void SNDSProducer::processTypeAdvertisement(const ndn::Name& name, const ndn::Interest& interest) {
  // Parse the incoming Interest name.
  ParsedInterest pi = parseInterestName(name);

  // Attempt to get the advertised ID from the parsed parameters.
  auto idOpt = pi.getParam(0);
  if (!idOpt.has_value()) {
    m_logger->warn("No ID provided in type advertisement. Ignoring.");
    return;
  }
  
  std::string idForType = idOpt.value();
  m_logger->info("Type advertisement: command = \"{}\", ID = \"{}\"", pi.command, idForType);
  
  // Look up the content type (with a preceding slash) in the registry map.
  std::string typeKey = "/" + pi.command;
  auto iter = myTypesRegistryMap.find(typeKey);
  if (iter != myTypesRegistryMap.end()) {
    // Access the vector of IDs stored by value.
    std::vector<std::string>& idList = iter->second;
    
    // If the ID is already registered, log a warning and send a warning Data packet.
    if (std::find(idList.begin(), idList.end(), idForType) != idList.end()) {
      m_logger->warn("ID already registered for type \"{}\". Skipping.", pi.command);
      std::string warnMsg = "WARNING:;ID already registered for type {" + pi.command + "}";
      sendData(interest.getName(), warnMsg);
      return;
    }
    
    // Register the new ID.
    idList.push_back(idForType);
    m_logger->info("Registered new ID \"{}\" for type \"{}\". Sending ACK.", idForType, pi.command);
    sendData(interest.getName(), m_ackTag);
  }
  else {
    m_logger->warn("Type \"{}\" not found in registry. Ignoring advertisement.", pi.command);
  }
}


void SNDSProducer::processLocalIdRequest(const ndn::Name& name, const ndn::Interest& interest) {
  // Parse the Interest into a structured format.
  ParsedInterest pi = parseInterestName(name);

  // In local ID requests, we assume the command field itself is the requested ID.
  // Ensure it has a leading slash.
  std::string localID = pi.command;
  ensureLeadingSlash(localID);

  // Look up the local ID in the locally announced IDs map.
  auto it = locallyAnnouncedIDsMap.find(localID);
  if (it != locallyAnnouncedIDsMap.end()) {
    m_logger->info("Local ID request for \"{}\" found. Sending associated data.", localID);
    sendData(interest.getName(), it->second);
  }
  else {
    m_logger->warn("Interest for unknown local ID \"{}\".", localID);
    
    // Build a string with available IDs for debugging.
    std::string availableIDs;
    for (const auto& pair : locallyAnnouncedIDsMap) {
      availableIDs += " " + pair.first;
    }
    m_logger->info("Available IDs: {}", availableIDs);
  }
}

void SNDSProducer::processSubscription(const ndn::Name& name, const ndn::Interest& interest) {
  // Parse the Interest name.
  ParsedInterest pi = parseInterestName(name);
  
  // Check that the command is either "SUBSCRIBE" or "NOTIFY".
  if(pi.command == "SUBSCRIBE") {
    // Expect two parameters: ID and SNDS_NODE.
    if(pi.params.size() < 2) {
      m_logger->warn("SUBSCRIBE command missing parameters. Ignoring.");
      return;
    }
    std::string preparedId = pi.params[0];
    ensureLeadingSlash(preparedId);
    std::string preparedSndsId = pi.params[1];
    ensureLeadingSlash(preparedSndsId);
    
    m_logger->info("New subscription: ID \"{}\", SNDS_NODE \"{}\".", preparedId, preparedSndsId);
    
    auto it = subscribedBYID_SNDSnodes.find(preparedId);

    if(it == subscribedBYID_SNDSnodes.end()) {
      std::vector<std::string> vec;
      vec.push_back(preparedSndsId);
      subscribedBYID_SNDSnodes[preparedId] = vec;
      sendData(interest.getName(), m_ackTag + " to_new_subscription");
      return;
    } 

    auto& vec = subscribedBYID_SNDSnodes[preparedId];
    if(std::find(vec.begin(), vec.end(), preparedSndsId) == vec.end()) {
      vec.push_back(preparedSndsId);
    }

    sendData(interest.getName(), m_ackTag + " to_new_subscription");

  }else if(pi.command == "NOTIFY") {
    // Expect one parameter: ID.
    if(pi.params.empty()) {
      m_logger->warn("NOTIFY command missing ID parameter. Ignoring.");
      return;
    }
    std::string preparedId = pi.params[0];
    ensureLeadingSlash(preparedId);
    m_logger->info("Subscription notification for ID \"{}\".", preparedId);
    
    auto it = subscribedBYID_SNDSnodes.find(preparedId);
    if(it != subscribedBYID_SNDSnodes.end()) {
      for (const auto& node : subscribedBYID_SNDSnodes[preparedId]) {
        m_logger->info("Notifying SNDS node: {}", node);

        std::string itemName = node + preparedId;

        auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
          std::move(itemName),
          SNDS_INTRA_MESSAGE_TYPE::GENERIC,
          SNDS_CLIENT_TYPES::ON_INTEREST,
          ""
        );

        v_to_consumer->writeMessage(std::move(message));
      }
    } else {
      m_logger->info("No subscribers found for ID \"{}\".", preparedId);
    }

    sendData(interest.getName(), m_ackTag + " to_notification_for_new_subscription");
  }
  else {
    m_logger->warn("Unrecognized subscription command: {}. Ignoring.", pi.command);
  }
}

void SNDSProducer::processRegistryRequest(const ndn::Name& name, const ndn::Interest& interest) {
  // Parse the incoming Interest name.
  ParsedInterest pi = parseInterestName(name);
  m_logger->info("Registry request received: command = \"{}\"", pi.command);

  // Remove the registry postfix from the command to get the base type.
  std::string baseType = pi.command.substr(0, pi.command.find(m_registryFilePostfix));

  // Look up the registry for the base type (the keys in myTypesRegistryMap are stored with a leading slash).
  auto iter = myTypesRegistryMap.find("/" + baseType);
  if (iter == myTypesRegistryMap.end()) {
    m_logger->warn("Unknown registry for type \"{}\".", baseType);
    return;
  }

  // Serialize the registry data into a payload using the modern method.
  std::vector<char> payloadVec = getRegistryFilePayload(iter->second);
  
  // Convert the vector<char> to a std::string.
  std::string payload(payloadVec.begin(), payloadVec.end());
  
  m_logger->info("Sending registry file payload for type \"{}\".", baseType);
  sendData(interest.getName(), payload);
}
