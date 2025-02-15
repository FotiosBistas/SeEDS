#include <spdlog/spdlog.h>
#include "snds_service_base.h"
#include "parsed_interest.h"

class SNDSConsumer: public ServiceBase {
public:
    // Constructor for SNDSProducer
    SNDSConsumer(spdlog::level::level_enum logLevel = spdlog::level::info,
                const std::string logFile = "Producer.log",
                uint8_t numberOfSndsSrvs = 1,
                const std::string sndsPrefix = "/SNDS",
                const std::string mySndsId = "0",
                const std::string idScopePrefix = "/SNDS_ID_scope_",
                const std::string registryFilePostfix = "_registry")
        : ServiceBase(logLevel, logFile, numberOfSndsSrvs, sndsPrefix, mySndsId, idScopePrefix, registryFilePostfix) 
    {}

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
    void onData(const ndn::Interest&, const ndn::Data& data);
};