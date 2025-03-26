#ifndef SERVICE_BASE_H
#define SERVICE_BASE_H

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/validator-config.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <thread>
#include <memory>
#include <thread>
#include <exception>
#include <vector>
#include <string>
#include "snds_message.h"
#include "message_queue.h"
#include "snds_utils.h"

class ServiceBase {
public:
  /**
   * @brief Constructor that configures logging and protocol-specific parameters.
   *
   * Parameters are taken by value so that both lvalues and rvalues can be used.
   *
   * @param logLevel The desired spdlog log level (default: info).
   * @param logFile The file to which log output should be written (default: "ServiceBase.log").
   * @param numberOfSndsSrvs The number of SNDS servers (default: 1).
   * @param sndsPrefix The SNDS prefix (default: "/SNDS").
   * @param mySndsId The local SNDS service ID (default: "0").
   * @param idScopePrefix The prefix used for the ID scope (default: "/SNDS_ID_scope_").
   * @param registryFilePostfix The postfix used for registry file names (default: "_registry").
   * @param ackTag The ack tag for acking data
   */
   ServiceBase(spdlog::level::level_enum logLevel = spdlog::level::info,
            std::string logFile = "ServiceBase.log",
            uint8_t numberOfSndsSrvs = 1,
            std::string sndsPrefix = "/SNDS",
            std::string mySndsId = "0",
            std::string idScopePrefix = "/SNDS_ID_scope_",
            std::string registryFilePostfix = "_registry",
            std::string ackTag = "THIS_IS_AN_SNDS_ACK",
            std::string serviceName = "ServiceBase")
  : m_face(),
    m_keyChain(),
    m_validator(m_face), // m_validator must be initialized after m_face
    m_logger(nullptr), // Will be initialized below
    m_numberOfSndsSrvs(numberOfSndsSrvs),
    m_sndsPrefix(std::move(sndsPrefix)),
    m_mySndsId(std::move(mySndsId)),
    m_idScopePrefix(std::move(idScopePrefix)),
    m_registryFilePostfix(std::move(registryFilePostfix)),
    m_ackTag(std::move(ackTag)),
    serviceName(std::move(serviceName)),
    m_mySndsIdName(m_sndsPrefix + "_" + m_mySndsId), // Now initialized correctly
    m_mySndsIdScopeName(m_sndsPrefix + m_idScopePrefix + m_mySndsId) // Correct order
  {

    // Logger initialization
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // Updated pattern includes [%n] to print the logger's name (i.e., serviceName)
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
    m_logger = std::make_shared<spdlog::logger>(this->serviceName, sinks.begin(), sinks.end());
    spdlog::register_logger(m_logger);
    m_logger->set_level(logLevel);

    m_logger->info("ServiceBase initialized with serviceName: {}", this->serviceName);
    // Validator setup
    try {
        m_validator.load("./trust-schema.conf");
    } catch (const std::exception& e) {
        m_logger->error("Error loading validator configuration: {}", e.what());
    }

    m_logger->info("========== {} Configuration ==========", this->serviceName);
    m_logger->info("Log Level: {}", spdlog::level::to_string_view(logLevel));
    m_logger->info("Log File: {}", logFile);
    m_logger->info("Number of SNDS Services: {}", m_numberOfSndsSrvs);
    m_logger->info("SNDS Prefix: {}", m_sndsPrefix);
    m_logger->info("My SNDS ID: {}", m_mySndsId);
    m_logger->info("ID Scope Prefix: {}", m_idScopePrefix);
    m_logger->info("Registry File Postfix: {}", m_registryFilePostfix);
    m_logger->info("ACK Tag: {}", m_ackTag);
    m_logger->info("Service Name: {}", this->serviceName);
    m_logger->info("Computed m_mySndsIdName: {}", m_mySndsIdName);
    m_logger->info("Computed m_mySndsIdScopeName: {}", m_mySndsIdScopeName);
    m_logger->info("===============================================");
  }

  virtual ~ServiceBase() {
  }

  virtual void onInterest(const ndn::Interest& interest) {
    ndn::Name name = interest.getName();
    m_logger->info("Received in function: {0}, name: {1}", __FUNCTION__, name.toUri());
  }

  virtual void onData(const ndn::Interest&, const ndn::Data& data) {
    // Log the receipt of the Data packet.
    m_logger->info("Received Data with name: {0} in function {1}", data.getName().toUri(), __FUNCTION__);

  }

  virtual void onNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
    m_logger->info("Received Nack (reason: {}) for Interest: {}",
      static_cast<int>(nack.getReason()), interest.getName().toUri());
  }

  virtual void onTimeout(const ndn::Interest& interest) {
    m_logger->info("Timeout for Interest: {}", interest.getName().toUri());
  }

  virtual void onRegisterFailed(const ndn::Name& prefix, const std::string& reason){
    m_logger->error("Failed to register prefix: {0} with the local forwarder ({1})", prefix.toUri(), reason);
    m_logger->error("Failed to register prefix: {0} with the local forwarder ({1})", prefix.toUri(), reason);
		m_face.shutdown();
  }

  virtual void onRegisterSuccess(const ndn::Name& prefix) {
    m_logger->info("Successfully registered prefix: {}", prefix.toUri());
  }


  virtual void init(){
    m_logger->info("Starting service: {}", serviceName);
    if (serviceName.empty()) {
        m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
    }
    m_logger->info("Exiting service: {}", serviceName);
  }

  // This method stops the loop by setting the flag to false.
  virtual void stop() {
    running.store(false);
    m_face.shutdown();
    m_logger->info("Service stopping: {}", serviceName);
    m_logger->flush();
  };

  /**
   * @brief Utility method to send a Data packet with the specified name and payload.
   */
  virtual bool sendData(const ndn::Name& name, std::string payload) {
    auto data = std::make_shared<ndn::Data>();
    data->setName(name);
    data->setFreshnessPeriod(ndn::time::seconds(10));
    data->setContent(std::move(payload));
    m_keyChain.sign(*data);
    m_face.put(*data);
    return true;
  }


protected:
  ndn::Face m_face;
  ndn::KeyChain m_keyChain;
  ndn::ValidatorConfig m_validator;
  ndn::ScopedRegisteredPrefixHandle m_certServeHandle;
  std::shared_ptr<spdlog::logger> m_logger;
  std::atomic<bool> running { true };

  // Declare message queues as static, so they are common to all instances.
  // Using std::unique_ptr for RAII management.
  // TODO this is terrible and should be changed
  static std::unique_ptr<MessageQueue> v_to_proxy;
  static std::unique_ptr<MessageQueue> v_to_dt;
  static std::unique_ptr<MessageQueue> v_to_producer;
  static std::unique_ptr<MessageQueue> v_to_consumer;
  static std::unique_ptr<MessageQueue> v_to_service;

  friend class SNDSDaemon;

  // Configurable protocol parameters:
  uint8_t m_numberOfSndsSrvs;           // Number of SNDS services.
  std::string m_sndsPrefix;             // e.g., "/SNDS"
  std::string m_mySndsId;               // e.g., "0"
  std::string m_mySndsIdName;           // Computed as m_sndsPrefix + "_" + m_mySndsId.
  std::string m_idScopePrefix;          // e.g., "/SNDS_ID_scope_"
  std::string m_mySndsIdScopeName;      // Computed as m_sndsPrefix + m_idScopePrefix + m_mySndsId.
  std::string m_registryFilePostfix;    // e.g., "_registry"
  std::string serviceName;
	std::string m_ackTag = "THIS_IS_AN_SNDS_ACK\n";
  std::string PARAM_ID_TAG = "id";
  std::string PARAM_TYPE_TAG = "type";

};

#endif // SERVICE_BASE_H
