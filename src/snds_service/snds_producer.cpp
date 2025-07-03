#include "snds_producer.h"

void SNDSProducer::init(){
  m_logger->info("Starting service: {}", this->serviceName);
  if (serviceName.empty()) {
      m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
  }
  m_logger->info("Thread started.");

  // Set up your initial interest filters
  std::string tmp_name = m_mySndsIdName;
  //[2025-03-04 08:35:24.437] [info] Announcing ID name: /SNDS_0
  m_logger->info("Announcing ID name: {}", tmp_name);
  m_face.setInterestFilter(tmp_name,
          std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
          std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
          std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

  tmp_name = m_mySndsIdScopeName;
  // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/SNDS_ID_scope_0
  m_logger->info("Announcing ID Scope name: {}", tmp_name);
  m_face.setInterestFilter(tmp_name,
          std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
          std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
          std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

  // Advertise for each type in your registry map
  // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR
  // [2025-03-04 08:29:36.980] [info] Announcing name: /SNDS/CAR_registry
  for (const auto &myPair : myTypesRegistryMap) {
      tmp_name = m_sndsPrefix + myPair.first;
      m_logger->info("Announcing name: {}", tmp_name);
      m_face.setInterestFilter(tmp_name,
          std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
          std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
          std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

      // For the registry file
      tmp_name += m_registryFilePostfix;
      m_logger->info("Announcing name: {}", tmp_name);
      m_face.setInterestFilter(tmp_name,
          std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
          std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
          std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));
  }

  // Optionally register a certificate filter (if needed)
  ndn::security::Certificate cert;
  try {
    cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
    m_certServeHandle = m_face.setInterestFilter(
      ndn::security::extractIdentityFromCertName(cert.getName()),
      [this, cert] (auto&&...) {
        m_face.put(cert);
      },
    std::bind(&SNDSProducer::onRegisterFailed, this, _1, _2));
  } catch (const ndn::security::pib::Pib::Error &e) {
      m_logger->error("No default identity found: {}", e.what());
      ndn::Name identityName("Producer");
      // create in PIB, which also installs a key and certificate
      auto identity = m_keyChain.createIdentity(identityName);
      // mark it as the default
      m_keyChain.setDefaultIdentity(identity);
      m_logger->info("Created and set default identity: {}", identityName.toUri());
  }

  // Process events in a blocking mode once (if needed)
  m_face.processEvents(ndn::time::milliseconds(-1));

  // Run the loop until 'running' is false.
  while (running.load()) {
      // m_logger->debug("Reading packet...");
      // Process events non-blockingly (or with a timeout)
      m_face.processEvents(ndn::time::seconds(2)); // Process for 1 second

      //// Check if there are messages to process for advertisement
      //std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_producer->readMessage();

      //if (message == nullptr) {
      //    m_logger->debug("Queue shutdown detected, exiting loop.");
      //    break;
      //}

      // Check if there are messages to process for advertisement
      std::unique_ptr<SNDS_INTRA_MESSAGE> message = v_to_producer->tryReadMessage();

      if (message == nullptr) {
        continue;
      }

      auto content = message.get()->getMessage();
      auto length = message.get()->getMessageLength();
      if(message.get()->getMessageType() == SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST){
          m_logger->info("Got new Temporal Query request...\n{}", message.get()->printDetails());
          auto query = deserialize_temporal_query(content);
          query.date = md5_hex(&query.date, query.date.size());
          auto name = to_path(query, false, false);
          m_logger->info("Annoucing name {}", name);
          m_face.setInterestFilter(name,
            std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
            std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
            std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));
          continue;
      }

      m_logger->info("Got new advertisement request...\n{}", message.get()->printDetails());
      tmp_name = m_sndsPrefix + content.substr(0, length);
      m_logger->info("Announcing name: {}", tmp_name);
      locallyAnnouncedIDsMap[content] = "SNDS_dummy_data";

      m_face.setInterestFilter(tmp_name,
          std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
          std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
          std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));
  }

  m_logger->info("Producer thread exiting.");
}

void SNDSProducer::onInterest(const ndn::Interest& interest) {
  ndn::Name name = interest.getName();
  m_logger->info("Received interest in function: {0}, name: {1}", __FUNCTION__, name.toUri());

  //if (name.size() < 2) {
  //  m_logger->warn("Interest name {} in {} is too short. Ignoring...", name.toUri(), __FUNCTION__);
  //  return;
  //}

  ParsedInterest pi = parseInterestName(name);

  m_logger->info("Parsed interest: {}", pi.toString());
  // Give priority to type advertisements
  if (myTypesRegistryMap.find("/" + pi.command) != myTypesRegistryMap.end()) {
    processTypeAdvertisement(pi);
    return;
  }
  //TODO needs to check with @yannisT
  if ((m_idScopePrefix + m_mySndsId).compare("/" + pi.command) == 0) {
    //TODO this needs to be done differently
    if(name.size() == 5){
      processSubscription(pi);
      return;
    }

    //TODO this needs to be done differently
    if(name.size() == 4){
      processNotification(pi);
      return;
    }

    m_logger->warn("Unrecognized subscription command: {}. Ignoring.", pi.command);
    return;
  }

  // If the command contains the registry postfix, handle registry requests.
  if (pi.command.find(m_registryFilePostfix) != std::string::npos) {
    processRegistryRequest(pi);
    return;
  }

  //TODO I don't think this is correct
  //TODO figure out the new if statement here
  processLocalIdRequest(pi);

  //m_logger->warn("Interest left unprocessed. No matching handler found.");
}

void SNDSProducer::processTypeAdvertisement(ParsedInterest& interest) {

  m_logger->info("Processing Type Advertisement");
  // Attempt to get the advertised ID from the parsed parameters.
  // TODO hopefully one ID received here
  std::string id_for_type = interest.params[0];
  std::string type = interest.command;
  ensureLeadingSlash(type);
  if (id_for_type.empty()) {
    m_logger->warn("No ID provided in type advertisement. Ignoring.");
    return;
  }

  m_logger->info("Type advertisement: command (registry) = {0}, ID = {1}", type, id_for_type);

  // Look up the content type (with a preceding slash) in the registry map.
  auto iter = myTypesRegistryMap.find(type);

  if (iter == myTypesRegistryMap.end()){
    m_logger->warn("Type {} not found in registry. Ignoring advertisement.", type);
  }

  // Access the vector of IDs stored by value.
  auto& ids = iter->second;

  // Try to insert the ID; check if it was already present
  auto [_, inserted] = ids.insert(id_for_type);

  if (!inserted) {
    m_logger->warn("ID already registered for type {}. Skipping...", type);
    std::string warn_message = "WARNING_ALREADY_REGISTERED::ID already registered for type {" + type+ "}";
    sendData(toName(interest), warn_message);
    return;
  }

  m_logger->info("Registered new ID {0} for type {1}. Sending ACK.", id_for_type, type);
  sendData(toName(interest), m_ackTag);
}

void SNDSProducer::processSubscription(ParsedInterest& interest) {


  // TODO this needs to be modified
  std::string prepared_name    = interest.getParam(0).value_or("Missing parameter");
  std::string prepared_snds_id = interest.getParam(1).value_or("Missing parameter");
  m_logger->info("Got interest indicating new SUBSCRIPTION getByid for {0} from SNDS_NODE {1}", prepared_name, prepared_snds_id);
  ensureLeadingSlash(prepared_name);
  ensureLeadingSlash(prepared_snds_id);

  auto& subscribers = subscribedBYID_SNDSnodes[prepared_name];

  // Try to insert the ID into the set
  auto [_, inserted] = subscribers.insert(prepared_snds_id);

  if (inserted) {
      m_logger->info("New subscription: {} subscribed to {}", prepared_snds_id, prepared_name);
      sendData(toName(interest), m_ackTag + " to_new_subscription");
  }
}

void SNDSProducer::processRegistryRequest(ParsedInterest& interest) {

  std::string name = interest.command;
  // Parse the incoming Interest name.
  m_logger->info("Registry request received: name = {}", name);

  // Remove the registry postfix from the command to get the base type.
  std::string base_type = name.substr(0, name.find(m_registryFilePostfix));

  // Look up the registry for the base type (the keys in myTypesRegistryMap are stored with a leading slash).
  auto iter = myTypesRegistryMap.find("/" + base_type);
  if (iter == myTypesRegistryMap.end()) {
    m_logger->warn("Unknown registry for type {}.", base_type);
    return;
  }

  std::vector<std::byte> byte_vector;
  byte_vector.push_back(std::byte{'R'});  // registry response marker
  byte_vector.push_back(std::byte{0x00}); // reserved/future use
  byte_vector = serializeRegistryFilePayload(iter->second);

  std::string payload(reinterpret_cast<const char*>(byte_vector.data()), byte_vector.size());

  m_logger->info("Sending registry file payload for type {}.", base_type);
  sendData(toName(interest), payload);
}

void SNDSProducer::processNotification(ParsedInterest& interest){

  std::string prepared_name = interest.getParam(0).value_or("Missing parameter");
  ensureLeadingSlash(prepared_name);
  m_logger->info("Subscription notification for name {}.", prepared_name);

  auto iterator = subscribedBYID_SNDSnodes.find(prepared_name);

  if(iterator == subscribedBYID_SNDSnodes.end()){
    m_logger->info("No subscribers found for ID {}.", prepared_name);
    sendData(toName(interest), m_ackTag + " to_notification_for_new_subscription");
    return;
  }

  const auto& subscribers = iterator->second;
  for (const auto& snds_node : subscribers) {

    m_logger->info("Notifying SNDS node: {}", snds_node);

    std::string itemName = snds_node + prepared_name;

    auto message = std::make_unique<SNDS_INTRA_MESSAGE>(
      std::move(itemName),
      SNDS_INTRA_MESSAGE_TYPE::GENERIC,
      SNDS_CLIENT_TYPES::ON_INTEREST,
      ""
    );

    v_to_consumer->writeMessage(std::move(message));
  }

  sendData(toName(interest), m_ackTag + " notification_for_new_subscription");
}

void SNDSProducer::processLocalIdRequest(ParsedInterest& interest) {
  std::string local_id = interest.command;
  // Parse the incoming Interest name.
  m_logger->info("Local ID request received: id = {}", local_id);

  // In local ID requests, we assume the command field itself is the requested ID.
  // Ensure it has a leading slash.
  ensureLeadingSlash(local_id);

  // Look up the local ID in the locally announced IDs map.
  auto it = locallyAnnouncedIDsMap.find(local_id);
  //TODO here we handle temporal queries.
  //TODO the metafile is received onData inside the consumer 
  //TODO the registry file is processed inside snds_http_worker
  if (it != locallyAnnouncedIDsMap.end()) {
    m_logger->info("Local ID request for {} found. Sending associated data.", local_id);
    sendData(toName(interest), "id="+it->second);
    return;
  }
}


