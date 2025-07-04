#include <spdlog/spdlog.h>
#include "snds_service_base.h"
#include "temporal_query.h"
#include "parsed_interest.h"

class SNDSConsumer: public ServiceBase {
public:
    // Constructor for SNDSProducer
    SNDSConsumer(spdlog::level::level_enum logLevel = spdlog::level::info,
                std::string logFile = "Consumer.log",
                uint8_t numberOfSndsSrvs = 1,
                std::string sndsPrefix = "/SNDS",
                std::string mySndsId = "0",
                std::string idScopePrefix = "/SNDS_ID_scope_",
                std::string registryFilePostfix = "_registry",
                std::string serviceName = "Consumer"
            )
    : ServiceBase(logLevel,
                  logFile,
                  numberOfSndsSrvs,
                  sndsPrefix,
                  mySndsId,
                  idScopePrefix,
                  registryFilePostfix,
                  "THIS_IS_AN_SNDS_ACK",  
                  serviceName)            
    {
        m_logger->info("SNDSConsumer initialized with serviceName: {}", this->serviceName);
    }

    void init();

    /**
     * @brief Callback invoked when a Data packet is received.
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
     * @param interest The Interest that triggered this Data response (unused).
     * @param data The received Data packet.
    */
    void onData(const ndn::Interest& interest, const ndn::Data& data);

    std::string handleTemporalQuery(const TemporalQuery& query);
    void parseTemporalQuery(const std::string& body);

};