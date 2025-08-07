#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <thread>
#include <exception>
#include <vector>
#include <string>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/validator-config.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "snds_utils.h"

#include "snds_message.hpp"
#include "message_queue.hpp"

class ServiceBase {
public:
  /**
   * @brief Constructor that configures logging and protocol-specific parameters.
   *
   * Parameters are taken by value so that both lvalues and rvalues can be used.
   *
   * @param logLevel The desired spdlog log level (default: info).
   * @param logFile The file to which log output should be written (default: "ServiceBase.log").
   * @param number_of_snds_services The number of SNDS servers (default: 1).
   * @param snds_prefix The SNDS prefix (default: "SNDS").
   * @param snds_id The local SNDS service ID (default: "0").
   * @param id_scope_prefix The prefix used for the ID scope (default: "SNDS_ID_scope_").
   * @param registryFilePostfix The postfix used for registry file names (default: "_registry").
   * @param ackTag The ack tag for acking data
   */
   ServiceBase
   (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "ServiceBase.log",
        size_t number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_id = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registry_file_postfix = "_registry",
        std::string ackTag = "THIS_IS_AN_SNDS_ACK",
        std::string serviceName = "ServiceBase",
        std::string broker_ip   = "0.0.0.0",
        size_t      broker_port = 10000,
		std::string temporalQueryPrefix = "date%3D",
        std::string metafilePrefix = "metafile%3Dtrue"
    ):m_face(),
    m_keyChain(),
    m_validator(m_face), // m_validator must be initialized after m_face
    m_logger(nullptr), // Will be initialized below
    number_of_snds_services(number_of_snds_services),
    snds_prefix(std::move(snds_prefix)),
    snds_id(std::move(snds_id)),
    id_scope_prefix(std::move(id_scope_prefix)),
    registry_file_postfix(std::move(registry_file_postfix)),
    m_ackTag(std::move(ackTag)),
    serviceName(std::move(serviceName)),
    snds_id_name(this->snds_prefix + "_" + this->snds_id),
    snds_id_scope_name(this->id_scope_prefix + this->snds_id),
    broker_ip(broker_ip),
    broker_port(broker_port),
	temporalQueryPrefix(temporalQueryPrefix),
	metafilePrefix(metafilePrefix)
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
        m_logger->info("Number of SNDS Services: {}", number_of_snds_services);
        m_logger->info("SNDS Prefix: {}", snds_prefix);
        m_logger->info("My SNDS ID: {}", snds_id);
        m_logger->info("ID Scope Prefix: {}", id_scope_prefix);
        m_logger->info("Registry File Postfix: {}", registry_file_postfix);
        m_logger->info("ACK Tag: {}", m_ackTag);
        m_logger->info("Service Name: {}", this->serviceName);
        m_logger->info("Computed SNDS ID name: {}", snds_id_name);
        m_logger->info("Computed SNDS ID Scope Name: {}", snds_id_scope_name);
        m_logger->info("Temporal Query Prefix: {}", temporalQueryPrefix);
        m_logger->info("===============================================");
    }

virtual ~ServiceBase() {}

virtual void onInterest(const ndn::Interest& interest) {
    ndn::Name name = interest.getName();
    m_logger->info("Received in function: {0}, name: {1}", __FUNCTION__, name.toUri());
}

virtual void onData(const ndn::Interest& interest, const ndn::Data& data) {
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
virtual bool sendData(const ndn::Name& name, std::string payload) 
{
    m_logger->debug("Sending data to the NDN…");
    try 
    {
        auto data = std::make_shared<ndn::Data>();
        data->setName(name);
        data->setFreshnessPeriod(ndn::time::seconds(60));
        data->setContent(std::move(payload));
        // May throw if KeyChain isn’t set up correctly
        m_keyChain.sign(*data);
        // May throw if the face is stopped or underlying I/O fails
        m_face.put(*data);
        return true;
    }
    catch (const std::exception& e) 
    {
        m_logger->error("Failed to send Data {}: {}", name.toUri(), e.what());
        return false;
    }
    catch (...) 
    {
        m_logger->error("Unknown error while sending Data {}", name.toUri());
        return false;
    }
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
  	static std::unique_ptr<MessageQueue> v_to_worker;
  	static std::unique_ptr<MessageQueue> v_to_service;

  	friend class SNDSDaemon;

  	// Configurable protocol parameters:
  	size_t number_of_snds_services;       // Number of SNDS services.
  	std::string snds_prefix;              // e.g., "SNDS"
  	std::string snds_id;                  // e.g., "0"
  	std::string snds_id_name;             // Computed as snds_prefix + "_" + snds_id.
  	std::string id_scope_prefix;          // e.g., "SNDS_ID_scope_"
  	std::string snds_id_scope_name;       // Computed as snds_prefix + id_scope_prefix + snds_id.
  	std::string registry_file_postfix;    // e.g., "_registry"
	std::string temporalQueryPrefix;      //e.g. "date%3D"
    std::string metafilePrefix;           //e.g. "metafile%3D"
  	std::string serviceName;
  	std::string broker_ip;
  	size_t      broker_port;

	std::string m_ackTag = "THIS_IS_AN_SNDS_ACK\r\n";
  	std::string WARNING_ALREADY_REGISTERED = "WARNING_ALREADY_REGISTERED";
  	static constexpr const char* ERASE_PREFIX="erase=";

};
