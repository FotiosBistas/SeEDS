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

void SNDSProducer::onRegisterFailed(const ndn::Name& prefix, const std::string& reason){
    m_logger->error("Failed to register prefix: {0} with the local forwarder ({1})", prefix, reason);
	m_face.shutdown();
}

//void SNDSProducer::processTypeAdvertisement(const ndn::Name& name, const ndn::Interest& interest) {
//  // In our parsed model, we expect:
//  //   prefix: m_sndsPrefix (e.g., "/SNDS")
//  //   command: type (e.g., "CAR")
//  //   params[0]: ID for that type.
//  ParsedInterest pi = parseInterestName(name);
//  if(pi.params.empty()) {
//    m_logger->warn("No ID provided in type advertisement. Ignoring.");
//    return;
//  }
//  
//  std::string idForType = pi.params[0];
//  m_logger->info("Type advertisement: command = \"{}\", ID = \"{}\"", pi.command, idForType);
//  
//  auto iter = myTypesRegistryMap.find("/" + pi.command);
//  if (iter != myTypesRegistryMap.end()) {
//    std::vector<std::string>* idList = iter->second;
//    if (std::find(idList->begin(), idList->end(), idForType) != idList->end()) {
//      m_logger->warn("ID already registered for type \"{}\". Skipping.", pi.command);
//      sendData(interest.getName(), "WARNING:;ID already registered for type.");
//      return;
//    }
//    idList->push_back(idForType);
//    m_logger->info("Registered new ID \"{}\" for type \"{}\". Sending ACK.", idForType, pi.command);
//    sendData(interest.getName(), ACK_TAG);
//  }
//}
//
//void SNDSProducer::processSubscription(const ndn::Name& name, const ndn::Interest& interest) {
//  // In our parsed model for subscription, we expect:
//  //   prefix: m_sndsPrefix
//  //   command: something like "SNDS_ID_scope_X"
//  //   params[0]: the ID; params[1] (if present): SNDS_NODE identifier.
//  ParsedInterest pi = parseInterestName(name);
//  if(pi.params.empty()) {
//    m_logger->warn("Subscription request missing parameters. Ignoring.");
//    return;
//  }
//  
//  // Determine if this is a "new subscription" or a "notification"
//  if(pi.params.size() == 2) { // New subscription request.
//    m_logger->info("New subscription: ID \"{}\", SNDS_NODE \"{}\".", pi.params[0], pi.params[1]);
//    std::string preparedId = pi.params[0];
//    prepareName(preparedId);
//    std::string preparedSndsId = pi.params[1];
//    prepareName(preparedSndsId);
//    auto it = subscribedBYID_SNDSnodes.find(preparedId);
//    if (it == subscribedBYID_SNDSnodes.end()) {
//      std::vector<std::string>* vec = new std::vector<std::string>();
//      vec->push_back(preparedSndsId);
//      subscribedBYID_SNDSnodes[preparedId] = vec;
//    } else {
//      std::vector<std::string>* vec = it->second;
//      if (std::find(vec->begin(), vec->end(), preparedSndsId) == vec->end()) {
//        vec->push_back(preparedSndsId);
//      }
//    }
//    sendData(interest.getName(), ACK_TAG + " to_new_subscription");
//  }
//  else if(pi.params.size() == 1) { // Notification.
//    m_logger->info("Subscription notification for ID \"{}\".", pi.params[0]);
//    std::string preparedId = pi.params[0];
//    prepareName(preparedId);
//    auto it = subscribedBYID_SNDSnodes.find(preparedId);
//    if (it != subscribedBYID_SNDSnodes.end()) {
//      std::vector<std::string>* vec = it->second;
//      for (const auto& node : *vec) {
//        m_logger->info("Notifying SNDS node: {}", node);
//        std::string itemName = node + preparedId;
//        SNDS_Intra_Msg* msg = new SNDS_Intra_Msg(ON_INTEREST, itemName.c_str(), itemName.length());
//        writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg);
//      }
//    } else {
//      m_logger->info("No subscribers found for ID \"{}\".", preparedId);
//    }
//    sendData(interest.getName(), ACK_TAG + " to_notification_for_new_subscription");
//  }
//}
//
//void SNDSProducer::processRegistryRequest(const ndn::Name& name, const ndn::Interest& interest) {
//  // For registry requests, the command contains our registry postfix.
//  ParsedInterest pi = parseInterestName(name);
//  m_logger->info("Registry request received: command = \"{}\"", pi.command);
//  // Remove the registry postfix to extract the base type.
//  std::string baseType = pi.command.substr(0, pi.command.find(m_registryFilePostfix));
//  auto iter = myTypesRegistryMap.find("/" + baseType);
//  if (iter == myTypesRegistryMap.end()) {
//    m_logger->warn("Unknown registry for type \"{}\".", baseType);
//    return;
//  }
//  std::string payload = getRegistryFilePayload(iter->second);
//  m_logger->info("Sending registry file payload for type \"{}\".", baseType);
//  sendData(interest.getName(), payload);
//}
//
//void SNDSProducer::processLocalIdRequest(const ndn::Name& name, const ndn::Interest& interest) {
//  // For local ID requests, we assume the command itself is the ID.
//  ParsedInterest pi = parseInterestName(name);
//  std::string idQuery = "/" + pi.command;
//  auto it = locallyAnnouncedIDsMap.find(idQuery);
//  if (it != locallyAnnouncedIDsMap.end()) {
//    m_logger->info("Local ID request for \"{}\". Sending data.", idQuery);
//    sendData(interest.getName(), it->second);
//  } else {
//    m_logger->warn("Interest for unknown local ID \"{}\".", idQuery);
//    std::string availableIDs;
//    for (const auto& pair : locallyAnnouncedIDsMap) {
//      availableIDs += " " + pair.first;
//    }
//    m_logger->info("Available IDs: {}", availableIDs);
//  }
//}