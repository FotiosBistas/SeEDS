#pragma once
/**
 * @file snds_http_server.hpp
 * @brief HTTP ingress/egress service built on cpp-httplib.
 *
 * @details
 * The **SNDSHTTPService** adapts HTTP requests into internal SNDS intra-messages
 * and returns replies back over HTTP. It is responsible for:
 *  - Parsing query/body parameters via the QUERY_PARAM_LIST macros.
 *  - Classifying requests into @ref snds_intra_message::SNDS_INTRA_MESSAGE_TYPE
 *    (e.g., GET_BY_ID_REQUEST, TEMPORAL_QUERY_ADVERTISEMENT) based on the HTTP verb
 *    and which parameters are present.
 *  - Pushing outbound messages onto the appropriate service queue.
 *  - Waiting for a single response message and using its payload as the HTTP response body.
 *
 * ### Endpoints
 * - **GET /**  
 *   Reads URL query parameters, maps them to intra-message parameters, infers a
 *   *GET* classification, and forwards to the Worker queue (@ref ServiceBase::v_to_worker).
 *
 * - **POST /**  
 *   Reads the request body (treated as query-string tokens or JSON upstream), maps
 *   parameters, infers a *POST* classification, and forwards to the Digital-Twin queue
 *   (@ref ServiceBase::v_to_dt).
 *
 * ### Message flow (high-level)
 * @verbatim
 *     HTTP client  -->  SNDSHTTPService (GET/POST)  -->  intra-message (typed)
 *                                                -->  target queue (Worker/DT)
 *                                                <--  response message (payload)
 *     HTTP client  <--  200 + payload (sanitized)
 * @endverbatim
 *
 * ### Concurrency
 * - The embedded cpp-httplib server runs in its own thread created inside @ref init().
 * - The main thread polls @ref ServiceBase::running to coordinate shutdown, then stops
 *   the server and joins the thread.
 *
 * ### Response payload handling
 * The service trims any leading non-JSON noise from the response payload by dropping
 * bytes until it sees `'{'` or `'['` (best-effort; upstream components should emit clean JSON).
 *
 * @note This service is an adapter; it does not interpret domain semantics beyond
 *       parameter extraction and message-type inference. The Worker/DT (and downstream
 *       components) produce the actual results.
*/
#include <thread>
#include <memory>
#include <string>

#include "httplib.h"
#include "snds_utils.h"
#include "snds_service_base.hpp"
#include "query_params.hpp"
/**
 * @class SNDSHTTPService
 * @brief HTTP entry point for SNDS, converting HTTP requests to intra-messages.
 *
 * @ingroup services
 *
 * @details
 * Uses cpp-httplib to listen on **0.0.0.0:8080** and expose two handlers:
 * - `GET /` : forwards classified queries to the Worker queue.
 * - `POST /`: forwards classified advertisements to the Digital-Twin queue.
 *
 * The classification logic is delegated to
 * @ref snds_intra_message::SNDS_INTRA_MESSAGE::infer_type_from_params(), which
 * uses present/absent parameters (from @ref QUERY_PARAM_LIST) plus the HTTP verb
 * to decide the concrete @ref snds_intra_message::SNDS_INTRA_MESSAGE_TYPE.
*/
class SNDSHTTPService : public ServiceBase {
public:
    /**
     * @brief Construct an HTTP service with logging and SNDS naming configuration.
     *
     * @param logLevel              Initial log level (e.g., `spdlog::level::info`).
     * @param logFile               Filename for service logs.
     * @param number_of_snds_services Number of SNDS nodes in the deployment (used by base class utilities).
     * @param snds_prefix           Root NDN prefix for SNDS names (e.g., `"SNDS"`).
     * @param snds_state_prefix     State sub-prefix.
     * @param snds_is_alive_prefix  Liveness sub-prefix.
     * @param snds_id               Local SNDS node identifier (string).
     * @param id_scope_prefix       Prefix for computing scope names (e.g., `"SNDS_ID_scope_"`).
     * @param registryFilePostfix   Suffix for registry naming (e.g., `"_registry"`).
     * @param serviceName           Human-friendly service label (default: `"HTTPServer"`).
     *
     * @note All constructor parameters are forwarded to @ref ServiceBase.
    */
    SNDSHTTPService
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "HTTPServer.log",
        size_t  number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_state_prefix = "state",
        std::string snds_is_alive_prefix = "is_alive",
        std::string snds_id = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "HTTPServer"
    ): 
    ServiceBase
    (
        logLevel,
        std::move(logFile),
        number_of_snds_services,
        std::move(snds_prefix),
        std::move(snds_state_prefix),
		std::move(snds_is_alive_prefix),
        std::move(snds_id),
        std::move(id_scope_prefix),
        std::move(registryFilePostfix),
        std::move(serviceName)
    )
    {
    }

    /**
     * @brief Start the HTTP server (blocking until shutdown).
     *
     * @details
     * - Registers **GET /** and **POST /** handlers.
     * - For each request:
     *    1. Extract parameters using the `DECLARE_/EXTRACT_` macros from @ref query_params.hpp.
     *    2. Create an @ref snds_intra_message::SNDS_INTRA_MESSAGE and set the HTTP verb.
     *    3. Call `infer_type_from_params()` to classify the message.
     *    4. Send to the appropriate queue:
     *       - GET → @ref ServiceBase::v_to_worker
     *       - POST → @ref ServiceBase::v_to_dt
     *    5. Wait for a single response from @ref ServiceBase::v_to_service and return its payload.
     * - Spawns the cpp-httplib server in a background thread; the main thread polls the
     *   @ref ServiceBase::running flag and coordinates shutdown.
     *
     * @warning This implementation processes one reply at a time per incoming request and
     *          uses a simple wait on @ref ServiceBase::v_to_service. If multiple requests
     *          are active concurrently, ensure your queues and workers are provisioned to
     *          preserve the expected correlation semantics.
    */
    void init() override;

};
