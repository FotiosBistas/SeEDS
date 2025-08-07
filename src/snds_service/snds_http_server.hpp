#pragma once

#include <thread>
#include <memory>
#include <string>

#include "httplib.h"
#include "snds_utils.h"
#include "snds_service_base.hpp"
#include "query_params.hpp"

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
