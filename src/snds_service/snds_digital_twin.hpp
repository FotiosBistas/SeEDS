#pragma once
/**
 * @file snds_digital_twin.hpp
 * @brief Digital Twin service for advertising IDs/types and temporal queries on the SNDS bus.
 *
 * @details
 * The **DigitalTwin** is a @ref ServiceBase component that:
 *  - Listens for intra-service messages (e.g., BY_ID_ADVERTISEMENT, TEMPORAL_QUERY_ADVERTISEMENT).
 *  - Emits corresponding NDN Interests (via other services) using the parsed_interest_name helpers.
 *  - Tracks locally announced IDs to avoid duplicate announcements.
 *
 * Typical lifecycle:
 * @code{.cpp}
 * auto twin = std::make_unique<DigitalTwin>(spdlog::level::debug);
 * twin->init();   // runs the read/dispatch loop until stop() is called from outside
 * @endcode
*/
#include "parsed_interest_name.hpp"
#include "snds_service_base.hpp"
#include "temporal_query.hpp"
/**
 * @class DigitalTwin
 * @brief Service that translates intra-messages into NDN advertisements and scope notifications.
 *
 * @ingroup services
 *
 * @note Inherits transport queues, logger, and lifecycle controls from @ref ServiceBase.
*/
class DigitalTwin: public ServiceBase{

public:
    /**
     * @brief Construct a DigitalTwin service.
     *
     * @param logLevel              Initial log level.
     * @param logFile               Output log file name.
     * @param number_of_snds_services Number of SNDS nodes for scope hashing/partitioning.
     * @param snds_prefix           Root NDN prefix for SNDS names (e.g., `"SNDS"`).
     * @param snds_state_prefix     State sub-prefix.
     * @param snds_is_alive_prefix  Liveness sub-prefix.
     * @param snds_id               Local SNDS node identifier (string form).
     * @param id_scope_prefix       Prefix used to compute ID scope names.
     * @param registryFilePostfix   Suffix used for registry naming.
     * @param serviceName           Human-readable service name for logging.
    */
    DigitalTwin
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "DigitalTwin.log",
        size_t      number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_state_prefix = "state",
        std::string snds_is_alive_prefix = "is_alive",
        std::string snds_id = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "DigitalTwin"
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
    {}
    /**
     * @brief Run the service loop.
     *
     * @details
     * Continuously reads intra-messages from @ref ServiceBase::v_to_dt and dispatches
     * to the appropriate handler based on @ref snds_intra_message::SNDS_INTRA_MESSAGE_TYPE.
     *
     * @post Returns when the queue is shut down or @ref ServiceBase::stop() has been called.
    */
    void init();
    /**
     * @brief Handle a BY_ID_ADVERTISEMENT request.
     *
     * @param id The object identifier to advertise and scope.
     *
     * @details
     * - De-duplicates repeated IDs using an internal set.
     * - Emits a `POST_ID` advertisement to the Producer.
     * - After a short delay, emits a `SCOPE` notification to the Consumer for the computed ID scope.
    */
    void process_by_id_advertisement(const std::string& id);
    /**
     * @brief Handle a BY_ID_AND_TYPE_ADVERTISEMENT request.
     *
     * @param id   Object identifier.
     * @param type Object type/category.
     *
     * @details
     * - Emits a `BY_ID_AND_TYPE_ADVERTISEMENT` to the Consumer.
     * - After a short delay, emits a `SCOPE` notification for the type scope.
    */
    void process_by_id_and_type_advertisement(const std::string& id, const std::string& type);
    /**
     * @brief Handle a TEMPORAL_QUERY_ADVERTISEMENT request.
     *
     * @param query Parsed temporal query (date, timeframe, filters, extras).
     *
     * @details
     * Serializes @p query and forwards it to the Producer as an intra-message,
     * for eventual NDN temporal query expression.
    */
    void process_temporal_query_advertisement(const temporal_query::TemporalQuery& query);
    /**
     * @brief Remove an ID from the pending/announced set.
     *
     * @param id The identifier to erase.
     *
     * @details
     * Used when an ID should no longer be considered “announced,” allowing a
     * future advertisement to pass de-duplication and be re-announced.
    */
    void erase_pending_id(const std::string& id);

private:
    /**
     * @brief Set of IDs already announced by this instance.
     *
     * @details
     * Prevents repeated emissions for the same ID. Cleared individually via
     * @ref erase_pending_id().
    */
	std::unordered_set<std::string> announced_ids; //stores local annnounced ids
};