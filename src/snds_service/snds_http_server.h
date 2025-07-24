#ifndef SNDS_HTTP_SERVICE_H
#define SNDS_HTTP_SERVICE_H

#include "snds_service_base.h"
#include <thread>
#include <memory>
#include <string>
// Include cpp-httplib header (make sure the include directory is set in CMake)
#include "httplib.h"

/**
 * @brief HTTP Service implemented with cpp-httplib.
 *
 * This service listens on port 8080 and handles GET and POST requests.
 * On a GET, it logs the request details and writes a message to the service queue.
 * Similarly for POST, the request body is used to create an internal message.
 */
class SNDSHTTPService : public ServiceBase {
public:
    SNDSHTTPService
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "HTTPServer.log",
        size_t  number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_id = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "HTTPServer"
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

    }

    /**
     * @brief Starts the HTTP server using cpp-httplib.
     *
     * Overrides the init() method from ServiceBase.
     */
    void init() override;
};

#endif // SNDS_HTTP_SERVICE_H
