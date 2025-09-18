#pragma once
/**
 * @file snds_service_base.hpp
 * @brief Common base class for all SNDS services (HTTP server, Consumer, Producer, DT, Worker).
 *
 * @details
 * Provides:
 *  - A unified logging setup (console + file) using spdlog.
 *  - NDN primitives (Face, KeyChain, Validator, basic handlers).
 *  - Lifecycle controls (@ref init, @ref stop).
 *  - Shared intra-service message queues (static, process-wide).
 *  - Canonical SNDS naming/config parameters derived from constructor args.
 *
 * @note The static message queues are a simple in-process message bus for this prototype;
 *       they are shared by *all* ServiceBase instances. Consider replacing with a real
 *       broker or per-service channels if you need isolation or distribution.
*/
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
/**
 * @class ServiceBase
 * @brief Base class implementing shared infrastructure for SNDS services.
 *
 * @ingroup services
 *
 * @details
 * Responsibilities:
 *  - Configure logging to both console and file, tagged by service name.
 *  - Initialize NDN Face, KeyChain, and a schema-based Validator.
 *  - Expose overridable handlers for Interest/Data/Nack/Timeout.
 *  - Provide a uniform lifecycle (`init()` loop + `stop()`).
 *  - Offer a convenience method to sign and send Data packets.
 *
 * Threading model:
 *  - Each derived service typically runs its own `init()` loop on a thread
 *    controlled by the daemon. The shared queues supply back-pressure and
 *    handoffs between services.
*/
class ServiceBase {
public:
  /**
   * @brief Construct and configure a service with logging and SNDS/NDN parameters.
   *
   * Parameters are taken by value and moved internally, so both lvalues and rvalues are accepted.
   * The constructor also attempts to load an NDN trust schema from `./trust-schema.conf`.
   *
   * @param logLevel               spdlog log level (e.g. `spdlog::level::info`).
   * @param logFile                Path to a log file (created/append).
   * @param number_of_snds_services Number of SNDS nodes in the deployment.
   * @param snds_prefix            Root SNDS NDN prefix (e.g. `"SNDS"`).
   * @param snds_state_prefix      Sub-prefix for state publications (e.g. `"state"`).
   * @param snds_is_alive_prefix   Sub-prefix for liveness publications (e.g. `"is_alive"`).
   * @param snds_id                Local service/node identifier (e.g. `"0"`).
   * @param id_scope_prefix        Prefix used to derive per-ID scopes (e.g. `"SNDS_ID_scope_"`).
   * @param registry_file_postfix  Suffix added to type names for registry files (e.g. `"_registry"`).
   * @param serviceName            Human-readable name used in logs and as logger id.
   * @param broker_ip              Placeholder for future external broker IP (unused here).
   * @param broker_port            Placeholder for future external broker port (unused here).
   *
   * @note Computed convenience members are derived and logged:
   *  - `snds_state_with_id = snds_state_prefix + "_" + snds_id`
   *  - `snds_is_alive_with_id = snds_is_alive_prefix + "_" + snds_id`
   *  - `snds_id_name = snds_prefix + "_" + snds_id`
   *  - `snds_id_scope_name = id_scope_prefix + snds_id`
   */
   ServiceBase
   (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "ServiceBase.log",
        size_t number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_state_prefix = "state",
        std::string snds_is_alive_prefix = "is_alive",
        std::string snds_id = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registry_file_postfix = "_registry",
        std::string serviceName = "ServiceBase",
        std::string broker_ip   = "0.0.0.0",
        size_t      broker_port = 10000
    ):m_face(),
    m_keyChain(),
    m_validator(m_face), // m_validator must be initialized after m_face
    m_logger(nullptr), // Will be initialized below
    number_of_snds_services(number_of_snds_services),
    snds_prefix(std::move(snds_prefix)),
    snds_state_prefix(std::move(snds_state_prefix)),
    snds_is_alive_prefix(std::move(snds_is_alive_prefix)),
    snds_id(std::move(snds_id)),
    id_scope_prefix(std::move(id_scope_prefix)),
    registry_file_postfix(std::move(registry_file_postfix)),
    serviceName(std::move(serviceName)),
    snds_state_with_id(this->snds_state_prefix + "_" + this->snds_id),
    snds_is_alive_with_id(this->snds_is_alive_prefix + "_" + this->snds_id),
    snds_id_name(this->snds_prefix + "_" + this->snds_id),
    snds_id_scope_name(this->id_scope_prefix + this->snds_id),
    broker_ip(broker_ip),
    broker_port(broker_port)
    {
        // Logger and validator are configured at construction time (implementation in .hpp).
        // Errors while loading the trust schema are logged but non-fatal.
        // See body for full details.
    }

  /** @brief Virtual destructor for proper polymorphic destruction. */
  virtual ~ServiceBase() {}

  /**
   * @brief Interest handler (override to implement custom logic).
   * @param interest Incoming Interest.
   * @note Default implementation logs the Interest name.
   */
  virtual void onInterest(const ndn::Interest& interest) {
      ndn::Name name = interest.getName();
      m_logger->info("Received in function: {0}, name: {1}", __FUNCTION__, name.toUri());
  }

  /**
   * @brief Data handler (override to implement custom logic).
   * @param interest Corresponding Interest (context).
   * @param data     Received Data.
   * @note Default implementation logs the Data name.
   */
  virtual void onData(const ndn::Interest& interest, const ndn::Data& data) {
      m_logger->info("Received Data with name: {0} in function {1}", data.getName().toUri(), __FUNCTION__);
  }

  /**
   * @brief Nack handler (override as needed).
   * @param interest Interest that was Nacked.
   * @param nack     Nack packet (includes reason code).
   * @note Default implementation logs reason and Interest name.
   */
  virtual void onNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
      m_logger->info("Received Nack (reason: {}) for Interest: {}",
        static_cast<int>(nack.getReason()), interest.getName().toUri());
  }

  /**
   * @brief Timeout handler (override to retry/backoff if needed).
   * @param interest Interest that timed out.
   * @details
   * Default implementation logs the timeout and generates a CONTROL_MESSAGE (TIMEOUT)
   * to the Worker queue (for observability/propagation).
   */
  virtual void onTimeout(const ndn::Interest& interest) {
      m_logger->info("Timeout for Interest: {}", interest.getName().toUri());
      auto nack_message = snds_intra_message::to_string(snds_intra_message::SNDS_CONTROL_MESSAGE_TYPE::TIMEOUT);

      auto message = std::make_unique<snds_intra_message::SNDS_INTRA_MESSAGE>
      (
          std::move(nack_message),
          snds_intra_message::SNDS_INTRA_MESSAGE_TYPE::CONTROL_MESSAGE,
          snds_intra_message::SNDS_CLIENT_TYPES::CONSUMER,
          "",
          snds_intra_message::HTTP_MESSAGE_TYPE::HTTP_EMPTY
      );

      v_to_worker->writeMessage(std::move(message));
  }

  /**
   * @brief Callback for prefix registration failure.
   * @param prefix NDN prefix attempted.
   * @param reason Error message.
   * @post Shuts down the Face to prevent further operations.
   */
  virtual void onRegisterFailed(const ndn::Name& prefix, const std::string& reason){
      m_logger->error("Failed to register prefix: {0} with the local forwarder ({1})", prefix.toUri(), reason);
      m_logger->error("Failed to register prefix: {0} with the local forwarder ({1})", prefix.toUri(), reason);
      m_face.shutdown();
  }

  /**
   * @brief Callback for successful prefix registration.
   * @param prefix Registered NDN prefix.
   */
  virtual void onRegisterSuccess(const ndn::Name& prefix) {
      m_logger->info("Successfully registered prefix: {}", prefix.toUri());
  }

  /**
   * @brief Default lifecycle entry point (override in derived services).
   *
   * @details
   * The default implementation only logs a start/exit pair. Most services
   * will implement a work loop in their override.
   */
  virtual void init(){
      m_logger->info("Starting service: {}", serviceName);
      if (serviceName.empty()) {
          m_logger->warn("Warning: serviceName is EMPTY! Check constructor initialization.");
      }
      m_logger->info("Exiting service: {}", serviceName);
  }

  /**
   * @brief Stop the service gracefully.
   *
   * @details
   * Sets the `running` flag to false, shuts down the NDN Face, and flushes the logger.
   * Derived classes may override if they need additional cleanup, but should call `ServiceBase::stop()`.
   */
  virtual void stop() {
      running.store(false);
      m_face.shutdown();
      m_logger->info("Service stopping: {}", serviceName);
      m_logger->flush();
  };

  /**
   * @brief Convenience API to sign and send a Data packet.
   *
   * @param name    Full NDN name of the Data.
   * @param payload Content bytes (moved internally).
   * @return `true` if signing and sending succeeded; `false` on error.
   *
   * @warning Requires a configured KeyChain and a running Face. Exceptions from
   *          `ndn-cxx` are caught and logged; failure is reported via `false`.
   */
  virtual bool sendData(const ndn::Name& name, std::string payload);

protected:
  	// ========== NDN primitives ==========
  	ndn::Face m_face;                    ///< NDN Face for I/O.
  	ndn::KeyChain m_keyChain;            ///< KeyChain used to sign Data.
  	ndn::ValidatorConfig m_validator;    ///< Schema-based validator.
  	ndn::ScopedRegisteredPrefixHandle m_certServeHandle; ///< Optional certificate serving.

  	// ========== Logging & lifecycle ==========
  	std::shared_ptr<spdlog::logger> m_logger; ///< Service logger (name = @ref serviceName).
  	std::atomic<bool> running { true };       ///< Cooperative stop flag for service loops.

  	// ========== Process-wide message queues (prototype Inter Thread Communication) ==========
  	/**
  	 * @name Shared message queues
  	 * @brief Static queues used by all services to exchange intra-messages.
  	 *
  	 * @details The daemon wires services together by writing/reading from these queues:
  	 *  - v_to_proxy, v_to_dt, v_to_producer, v_to_consumer, v_to_worker, v_to_service
  	 *    (naming reflects the intended receiver).
  	 *
  	 * @note This approach is intentionally simple for a single-process prototype.
  	 *       If you need multi-process or distributed operation, plug in a real broker.
  	 */
  	///@{
  	static std::unique_ptr<MessageQueue> v_to_proxy;
  	static std::unique_ptr<MessageQueue> v_to_dt;
  	static std::unique_ptr<MessageQueue> v_to_producer;
  	static std::unique_ptr<MessageQueue> v_to_consumer;
  	static std::unique_ptr<MessageQueue> v_to_worker;
  	static std::unique_ptr<MessageQueue> v_to_service;
  	///@}

  	friend class SNDSDaemon; ///< Allows the daemon to manage lifecycle across services.

  	// ========== SNDS configuration ==========
  	size_t      number_of_snds_services;  ///< Size of the SNDS cluster.
  	std::string snds_prefix;              ///< Root prefix, e.g. `"SNDS"`.
    std::string snds_state_prefix;        ///< `"state"` sub-prefix.
    std::string snds_is_alive_prefix;     ///< `"is_alive"` sub-prefix.
  	std::string snds_id;                  ///< Local node id, e.g. `"0"`.

    std::string snds_state_with_id;       ///< Derived: `state_<id>`.
    std::string snds_is_alive_with_id;    ///< Derived: `is_alive_<id>`.
  	std::string snds_id_name;             ///< Derived: `<snds_prefix>_<id>`.

  	std::string id_scope_prefix;          ///< Prefix to build per-ID scopes, e.g. `"SNDS_ID_scope_"`.
  	std::string snds_id_scope_name;       ///< Derived: `<id_scope_prefix><id>`.
  	std::string registry_file_postfix;    ///< Registry suffix, e.g. `"_registry"`.
  	std::string serviceName;              ///< Human-readable service name and logger id.

  	// ========== External broker placeholders (not used yet) ==========
  	std::string broker_ip;                ///< Future: external broker host/IP.
  	size_t      broker_port;              ///< Future: external broker port.
};
