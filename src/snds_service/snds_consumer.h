#include <spdlog/spdlog.h>
#include "snds_service_base.h"
#include "temporal_query.h"
#include "parsed_interest_name.h"
#include "parsed_interest_to_temporal_query.h"

class SNDSConsumer: public ServiceBase {
public:
    // Constructor for SNDSProducer
    SNDSConsumer
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "Consumer.log",
        size_t  number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_id     = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "Consumer"
    ):ServiceBase
    (
        logLevel,
        logFile,
        number_of_snds_services,
        snds_prefix,
        snds_id,
        id_scope_prefix,
        registryFilePostfix,
        "THIS_IS_AN_SNDS_ACK",  
        serviceName
    )            
    {
        m_logger->info("SNDSConsumer initialized with serviceName: {}", this->serviceName);
    }

    void init();

    /**
    * @brief Callback invoked when a Data packet is received from the ndn network.
    *
    * This function is called by the underlying NDN Face when a Data packet arrives.
    * It performs the following operations:
    * - Logs the received Data packet's name.
    * - Converts the Data content to a std::string.
    * - Checks that the Data name has at least two components; if not, logs a warning and aborts further processing.
    * - If the payload contains the acknowledgment tag (m_ackTag), logs this and aborts further processing.
    * - Extracts the second component of the Data name (used as the intra-service message name).
    * - Creates a new SNDS_INTRA_MESSAGE encapsulating the payload and message name.
    * - Dispatches this intra-service message to the service queue for further processing.
    *
    * @param interest The Interest that triggered this Data response.
    * @param data The received Data packet.
    */
    void onData(const ndn::Interest& interest, const ndn::Data& data);

    std::string handleTemporalQuery(const TemporalQuery& query);

    void send_metafile_interests(const std::string& body, const TemporalQuery& parent_query);

};