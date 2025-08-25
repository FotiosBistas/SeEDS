#include <ndn-cxx/face.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>  

#include "httplib.h"
#include "snds_service_base.hpp"
#include "parsed_interest_name.hpp"
#include "temporal_query.hpp"

#include "snds_utils.h"

class SNDSProducer : public ServiceBase {
public:
    SNDSProducer
	(
		spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "Producer.log",
        size_t number_of_snds_services = 1,
        std::string snds_prefix        = "SNDS",
        std::string snds_id            = "0",
        std::string id_scope_prefix    = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "Producer",
        std::string broker_ip = "0.0.0.0",
        size_t      broker_port = 10000,
		std::string registryType = "CAR"
	):ServiceBase
	(
		logLevel,
        logFile,
        number_of_snds_services,
        snds_prefix,
        snds_id,
        id_scope_prefix,
        registryFilePostfix,
        "THIS_IS_AN_SNDS_ACK\n",
        serviceName,
        broker_ip,
        broker_port
	)
  	{
  	    m_logger->info("SNDSProducer initialized with serviceName: {}", this->serviceName);
		m_logger->info("SNDSProducer will connect to Broker with IP: {0} and port: {1}", broker_ip, broker_port);
  	    type_registry[registryType] = std::unordered_set<std::string>{};
  	}

	void init();


    void onInterest(const ndn::Interest &interest);
    void processRegistryRequest(const parsed_interest_name::ParsedInterestName& interest);
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
	void process_new_subscription_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_scope_notification_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_get_by_id_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_by_type_advertisement_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_get_by_type_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_get_by_id_for_type_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_subscription_notification_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_temporal_query_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_version_metafile_interest(const parsed_interest_name::ParsedInterestName& interest);
	void process_temporal_query_advertisement(const std::string& content);
	void process_by_id_advertisement(const std::string& content);
	void process_temporal_query_request(const std::string& content);

	uint8_t _publish_to_broker(const nlohmann::json& doc);

	void handleTemporalQuery(parsed_interest_name::ParsedInterestName& interest);
	void parseTemporalQuery(const std::string& body);

private:
	std::unordered_map<std::string, std::unordered_set<std::string>> type_registry; //maps Type names to vectors of IDs
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribed_nodes_for_id; //maps ID to subscribed nodes
};
