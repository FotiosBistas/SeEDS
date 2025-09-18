#pragma once
/**
 * @file snds_consumer.hpp
 * @brief Consumer-side service that translates intra-service messages into NDN Interests
 *        and forwards received Data back into the intra-service bus.
 *
 * @details
 * The SNDSConsumer listens for SNDS intra-messages (e.g., GET/POST-derived requests,
 * subscription events, temporal queries), converts them into NDN Interests using the
 * parsed-interest utilities, expresses those Interests on the NDN face, and on Data
 * reception publishes results back into the worker queue as SNDS intra-messages.
 *
 * Key responsibilities:
 *  - Drive a processing loop in init() that reads messages from the consumer queue.
 *  - Build canonical NDN names via parsed_interest_name helpers and express Interests.
 *  - Handle temporal-query flows (direct vs. manifest/metafile modes).
 *  - Forward registry payloads, GET_BY_* responses, and temporal data back to workers.
*/
#include "snds_service_base.hpp"

#include "temporal_query.hpp"
#include "parsed_interest_name.hpp"
#include "parsed_interest_to_temporal_query.hpp"

/**
 * @class SNDSConsumer
 * @brief Concrete consumer service that bridges SNDS intra-messages to NDN Interests.
 *
 * @note Inherits logging, queues, and NDN face from ServiceBase.
*/
class SNDSConsumer: public ServiceBase {
public:
    /**
     * @brief Construct a consumer service.
     *
     * @param logLevel               Initial spdlog level.
     * @param logFile                Log file name for this service.
     * @param number_of_snds_services Logical fan-out / number of peer services (used by base).
     * @param snds_prefix            Top-level SNDS namespace prefix (e.g., "SNDS").
     * @param snds_state_prefix      State sub-prefix (e.g., "state").
     * @param snds_is_alive_prefix   Liveness sub-prefix (e.g., "is_alive").
     * @param snds_id                Local node/service identifier.
     * @param id_scope_prefix        Scope-prefix for ID ownership (e.g., "SNDS_ID_scope_").
     * @param registryFilePostfix    Postfix used for registry naming (e.g., "_registry").
     * @param serviceName            Human-readable service name for logs/metrics.
    */
    SNDSConsumer
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        std::string logFile = "Consumer.log",
        size_t  number_of_snds_services = 1,
        std::string snds_prefix = "SNDS",
        std::string snds_state_prefix = "state",
        std::string snds_is_alive_prefix = "is_alive",
        std::string snds_id     = "0",
        std::string id_scope_prefix = "SNDS_ID_scope_",
        std::string registryFilePostfix = "_registry",
        std::string serviceName = "Consumer"
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
        m_logger->info("SNDSConsumer initialized with serviceName: {}", this->serviceName);
    }
    /**
     * @brief Start the consumer loop.
     *
     * @details
     * Repeatedly reads messages from the consumer queue (v_to_consumer). For each
     * message, selects the correct Interest expression path based on the
     * SNDS_INTRA_MESSAGE_TYPE. The loop exits when `running` (inherited) becomes false
     * or the queue is shut down.
    */
    void init();
    /**
     * @brief Handle an internal “temporal query” intra-message (dispatcher entry).
     * @note Convenience hook if callers push pre-normalized temporal-query messages.
    */
    void process_temporal_query_intra_message();
    /**
     * @name Interest expression helpers
     * @brief Build a specific NDN name from @p content and express the Interest.
     * @param content Encoded request/parameters; typically a path- or query-style string
     *                as produced by higher-level components.
     */
    ///@{
    /** @brief Express a `getByID` Interest. */
    void express_get_by_id_interest(const std::string& content);
    /** @brief Express a `getByIDForType` Interest. */
    void express_get_by_id_for_type_interest(const std::string& content);
    /** @brief Express a `getByType` Interest. */
    void express_get_by_type_interest(const std::string& content);
    /** @brief Express a scope-notification Interest (scope ownership/assignment). */
    void express_scope_interest(const std::string& content);
    /** @brief Express a subscription Interest (either by ID or by TYPE). */
    void express_subscription_interest(const std::string& content);
    /** @brief Express a subscription-notification Interest to a subscriber. */
    void express_subscription_notification(const std::string& content);
    /** @brief Express a temporal-query Interest (direct or manifest-based flows). */
    void express_temporal_query_interest(const std::string& content);
    /** @brief Express a by-id-and-type advertisement Interest. */
    void express_by_id_and_type_advertisement_interest(const std::string& content);
    /** @brief Express a `getByIDFromNode` Interest (target a specific SNDS node). */
    void express_get_by_id_from_node_interest(const std::string& content);
    /**
     * @brief Expand a temporal-query manifest payload into individual metafile Interests.
     *
     * @param body          JSON payload returned by the temporal-query Interest.
     * @param parent_query  The parent TemporalQuery (used to tag children).
     *
     * @details
     * De-duplicates metafile Interests by canonical name, sends them sequentially,
     * and marks the last one via the `lastInterest=true` query flag.
    */
    void send_metafile_interests(const std::string& body, const temporal_query::TemporalQuery& parent_query);
    ///@}
    /**
     * @brief Data callback for expressed Interests.
     *
     * @param interest The original Interest.
     * @param data     The received Data packet.
     *
     * @details
     * Decodes payload, parses the carried name back to a ParsedInterestName, and
     * forwards an appropriate SNDS intra-message to the worker queue. Handles:
     *  - registry payloads,
     *  - GET_BY_* responses,
     *  - temporal-query results (direct item vs. metafile manifest expansion).
    */
    void onData(const ndn::Interest& interest, const ndn::Data& data);

private:
    /**
     * @brief Determine whether a temporal-query JSON payload is in “direct item” form.
     *
     * @param j Parsed JSON payload.
     * @return true if the payload represents a single item response (no manifest traversal).
     *
     * @details
     * Supports two schemas:
     *  - A: `{"mode":"direct","items":[{...}]}` (preferred)
     *  - B: Legacy `[{ "<type>": { ...doc... } }]`
    */
    bool _is_direct_temporal_payload(const nlohmann::json& j);
    /**
     * @brief Extract the first item from a direct temporal-query payload.
     *
     * @param j   Parsed JSON payload (schemas A or B).
     * @param out On success, receives the extracted item object.
     * @return true if @p out was populated with an item object; false otherwise.
    */
    bool _extract_direct_temporal_item(const nlohmann::json& j, nlohmann::json& out);
};