#pragma once
/**
 * @file snds_worker.hpp
 * @brief Orchestrates request/response workflows inside SNDS: subscriptions, lookups, and temporal queries.
 *
 * @details
 * The **Worker** is the central coordinator that:
 *  - Receives intra-messages from multiple producers (HTTP server, Consumer, Producer).
 *  - Maintains light state for in-flight operations (deduplication, correlation, aggregation).
 *  - Emits follow-up Interests (via the Consumer) or terminal responses (to the HTTP service).
 *
 * ### High-level dataflow
 * - **HTTP → Worker**  
 *   - GET_BY_ID_REQUEST → enqueue an NDN `getByID` Interest (to Consumer) and track pending ID.  
 *   - GET_BY_TYPE_REQUEST → enqueue an NDN `getByType` Interest (to Consumer) and track pending type.  
 *   - TEMPORAL_QUERY_REQUEST → hash + store the query; instruct Consumer to fan out (direct/metafile).
 *   - Subscriptions...
 *
 * - **Consumer → Worker**  
 *   - Replies for BY_ID / BY_ID_FOR_TYPE / BY_ID_FROM_NODE / REGISTRY_PAYLOAD / VERSION_RESPONSE  
 *     are correlated back to pending state and forwarded to the HTTP service, possibly after
 *     aggregation/filtering (temporal queries).
 *
 * - **Producer → Worker**  
 *   - Control/ACK messages are passed through to HTTP (for observability), no state changes.
 * 
 * - **Consumer→ Worker**  
 *   - Control/ACK message again
 *
 * ### Temporal queries
 *  - A hash (see @ref temporal_query::TemporalQueryHasher) is used as a correlation key.
 *  - Two modes are supported:
 *    1. **Direct**: the response already contains the wanted document → immediately return to HTTP.
 *    2. **Manifest/Aggregated**: multiple metafile hits arrive; buffer JSON slices until `lastInterest`,
 *       apply optional field filters, merge into one bundle, then return.
 *
 * @note The Worker is single-threaded with respect to its `init()` loop; it relies on queue isolation
 *       for concurrency. If you parallelize handling, make sure to protect the pending maps/sets.
 */

#include <algorithm>  
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <span>

#include <nlohmann/json.hpp>  

#include "snds_service_base.hpp"
#include "parsed_interest_name.hpp"
#include "temporal_query.hpp"
#include "snds_utils.h"

/**
 * @class Worker
 * @brief Central coordinator and state machine for SNDS intra-messages.
 *
 * @ingroup services
 *
 * @details
 * Responsibilities:
 *  - **Subscription management**: first-subscriber triggers interest to subscribe a remote scope,
 *    deduplicating subsequent subscriptions for the same name.
 *  - **Lookup lifecycle**:
 *      * GET_BY_ID: deduplicate concurrent requests for the same ID; clear state on first reply.
 *      * GET_BY_TYPE: fetch registry file → fan out `getByIDForType` for each ID listed → aggregate results.
 *  - **Temporal query lifecycle**:
 *      * Track pending queries keyed by a stable hash, buffer partial results, detect the final chunk,
 *        apply optional field filters, and emit a single normalized JSON bundle.
 */
class Worker: public ServiceBase {
public:
    /**
     * @brief Construct a Worker service.
     *
     * @param logLevel              Initial log level (e.g., `spdlog::level::info`).
     * @param logFile               Log filename (default: `"Worker.log"`).
     * @param number_of_snds_services Number of SNDS nodes (used by scope utilities).
     * @param snds_prefix           Root NDN prefix (e.g., `"SNDS"`).
     * @param snds_state_prefix     State sub-prefix.
     * @param snds_is_alive_prefix  Liveness sub-prefix.
     * @param snds_id               This node's SNDS identifier.
     * @param id_scope_prefix       Prefix to derive per-name scope (e.g., `"SNDS_ID_scope_"`).
     * @param registryFilePostfix   Suffix for registry files (e.g., `"_registry"`).
     * @param serviceName           Human-friendly service label.
     */
	Worker
	(
	    spdlog::level::level_enum logLevel = spdlog::level::info,
	    std::string logFile = "Worker.log",
	    size_t      number_of_snds_services = 1,
	    std::string snds_prefix     = "SNDS",
	    std::string snds_state_prefix = "state",
        std::string snds_is_alive_prefix = "is_alive",
	    std::string snds_id         = "0",
	    std::string id_scope_prefix = "SNDS_ID_scope_",
	    std::string registryFilePostfix = "_registry",
	    std::string serviceName = "Worker"
	): 
	ServiceBase
	(
	    logLevel,
	    std::move(logFile),
	    number_of_snds_services,
	    std::move(snds_prefix),
		std::move(snds_is_alive_prefix),
	    std::move(snds_state_prefix),
	    std::move(snds_id),
	    std::move(id_scope_prefix),
	    std::move(registryFilePostfix),
	    std::move(serviceName)
	)
	{
	    m_logger->info("Worker initialized.");
	}

    /**
     * @brief Main service loop.
     *
     * @details
     * Reads intra-messages from @ref ServiceBase::v_to_worker and dispatches to
     * sender-specific handlers:
     *  - HTTP_SRV → @_handle_http_server_intra_message
     *  - CONSUMER → @_handle_consumer_intra_message
     *  - PRODUCER → @_handle_producer_intra_message
     *
     * The loop exits when @ref ServiceBase::running becomes `false` or the queue is shut down.
     */
  	void init() override;

private:
	// ---------------------------------------------------------------------
	// Subscription state
	// ---------------------------------------------------------------------

	/**
	 * @brief Subscriptions keyed by ID: URL set of subscribers per object ID.
	 *
	 * @details First subscriber triggers a remote subscription (via Consumer).
	 * Later duplicates are acknowledged but do not fan out a new subscription.
	 */
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribed_by_id_users;

	/**
	 * @brief Subscriptions keyed by type: URL set of subscribers per type name.
	 *
	 * @details Same semantics as @ref subscribed_by_id_users, but grouping by type.
	 */
	std::unordered_map<std::string, std::unordered_set<std::string>> subscribed_by_type_users;

	// ---------------------------------------------------------------------
	// getByType aggregation state
	// ---------------------------------------------------------------------

	/**
	 * @brief Pending *per-type* fan-out: type → set of IDs expected from registry.
	 *
	 * @details After receiving a registry file, we fan out `getByIDForType` Interests
	 * for each ID and record them here; the final response per type is emitted once
	 * this set becomes empty.
	 */
	std::unordered_map<std::string, std::unordered_set<std::string>> pending_ids_to_type_map;

	/**
	 * @brief Accumulator for `getByIDForType` responses per type.
	 */
	struct TypePayload 
	{
		std::string id;      ///< ID whose payload this is.
		std::string payload; ///< Raw payload returned for that ID.
	};

	/**
	 * @brief Map: type → list of {id, payload} items collected so far.
	 */
	std::unordered_map<std::string, std::vector<TypePayload>> payloads_for_get_by_type;

	// ---------------------------------------------------------------------
	// Deduplication for GET_BY_* requests
	// ---------------------------------------------------------------------

	/**
	 * @brief Set of IDs currently being fetched via `getByID` to avoid duplicates.
	 */
	std::unordered_set<std::string> pending_get_by_id_requests;

	/**
	 * @brief Set of types currently being fetched via `getByType` to avoid duplicates.
	 */
	std::unordered_set<std::string> pending_get_by_type_requests;

	// ---------------------------------------------------------------------
	// Temporal query aggregation state
	// ---------------------------------------------------------------------

	/**
	 * @brief Pending temporal queries, keyed by hash (see @ref temporal_query::temporal_query_hash_string).
	 */
	std::unordered_map<std::string, temporal_query::TemporalQuery> pending_temporal_queries;

	/**
	 * @brief Buffer of partial JSON documents per temporal query hash.
	 *
	 * @details Used in manifest/aggregated mode. On `lastInterest=true`, the buffered
	 * items are merged into a single bundle and returned to HTTP.
	 */
	std::unordered_map<std::string, std::vector<nlohmann::json>> temporal_buffers;

	// ---------------------------------------------------------------------
	// Request lifecycle helpers
	// ---------------------------------------------------------------------

	/**
	 * @brief Start a new `getByID` flow (deduplicated).
	 * @param id Object identifier.
	 *
	 * @details Inserts @p id into @ref pending_get_by_id_requests and issues an
	 * intra-message instructing the Consumer to express the Interest.
	 */
	void process_new_get_by_id(const std::string& id);

	/**
	 * @brief Handle a completed `getByID` with payload; clears pending state and returns to HTTP.
	 * @param id      Identifier whose response arrived.
	 * @param payload Data payload.
	 */
	void process_processed_get_by_id(const std::string& id, const std::string& payload);

	/**
	 * @brief Register a (by-id or by-type) subscription request; the first subscriber triggers a remote scope subscription.
	 * @param name     ID or type being subscribed to.
	 * @param url      Subscriber callback URL.
	 * @param msg_type BY_ID_SUBSCRIPTION or BY_TYPE_SUBSCRIPTION.
	 */
	void process_subscription_request(const std::string& name, const std::string& url, const snds_intra_message::SNDS_INTRA_MESSAGE_TYPE msg_type);

	/**
	 * @brief Start a new `getByType` flow (deduplicated).
	 * @param type Type name.
	 *
	 * @details Issues a request for the type’s registry file; IDs are extracted later
	 * (see @ref process_registry_file), then fanned out as `getByIDForType`.
	 */
	void process_new_get_by_type(const std::string& type);

	/**
	 * @brief Handle a `getByIDForType` item; aggregate until all IDs for this type are received.
	 * @param id      ID whose item arrived.
	 * @param type    Type that is being aggregated.
	 * @param payload Raw payload for this {type, id}.
	 *
	 * @details When the last pending ID for @p type is removed, emit a consolidated JSON payload
	 * to the HTTP service and clear state for that type.
	 */
	void process_processed_get_by_type(const std::string& id, const std::string& type, std::string& payload);

	/**
	 * @brief Parse a registry file payload and fan out `getByIDForType` for each listed ID.
	 * @param payload Raw registry content.
	 * @param name    Type name used for correlation.
	 */
	void process_registry_file(const std::string& payload, const std::string& name);

	/**
	 * @brief Forward a control/ACK message to the HTTP service for visibility.
	 * @param name    Correlation/name field.
	 * @param payload ACK payload.
	 */
	void process_ack_message(std::string&& name, std::string&& payload);

	/**
	 * @brief Forward a `getByIDFromNode` response to the HTTP service.
	 * @param id      Correlation ID (moved).
	 * @param payload Content (moved).
	 */
	void process_processed_get_by_id_from_node(std::string&& id, std::string&& payload);

	/**
	 * @brief Begin a temporal query flow: record state and instruct Consumer to fan out.
	 * @param temporal_query Parsed query (id/type/date/filters/...).
	 *
	 * @details Uses a stable hash of the query as the correlation key; duplicate queries
	 * (same hash) are ignored while one is in flight.
	 */
	void process_temporal_query_request(const temporal_query::TemporalQuery& temporal_query);

	/**
	 * @brief Handle temporal query responses (direct or aggregated).
	 * @param parent_query_hash Hash used to locate pending state (ignored in direct mode).
	 * @param content           Raw JSON (may contain leading/trailing noise; utils extract slices).
	 * @param last_interest     `true` if this is the final chunk for the parent query.
	 * @param is_direct_mode    `true` if the payload is already the target document.
	 *
	 * @details
	 *  - **Direct mode**: extract one item, optionally clear GET_BY_* pending sets for that item,
	 *    wrap as `{"mode":"direct","items":[...]} -> HTTP`.
	 *  - **Aggregated**: split into slices, sanitize/parse, apply field filters (if present),
	 *    buffer under @p parent_query_hash, and on `last_interest` build a manifest bundle
	 *    then send to HTTP. State is erased afterwards.
	 */
	void process_temporal_query_response(const std::string& parent_query_hash, const std::string& content, bool last_interest, bool is_direct_mode);

	// ---------------------------------------------------------------------
	// JSON utilities
	// ---------------------------------------------------------------------

	/**
	 * @brief Apply "keep only these fields" filtering to a document/array.
	 * @param doc  Input JSON document or array.
	 * @param keep Set of field names to retain (empty → no filtering).
	 * @return Filtered JSON (shape preserved where possible).
	 */
	nlohmann::json apply_filters_to_doc(const nlohmann::json& doc, const std::unordered_set<std::string>& keep);

	// ---------------------------------------------------------------------
	// Sender-specific dispatchers
	// ---------------------------------------------------------------------

	/**
	 * @brief Handle a message coming from the Consumer.
	 *
	 * @details Detects ACKs early (pass-through), otherwise routes by
	 * @ref snds_intra_message::SNDS_INTRA_MESSAGE_TYPE to the relevant processor:
	 *  - GET_BY_ID[_FROM_NODE]_RESPONSE
	 *  - GET_BY_ID_FOR_TYPE_RESPONSE
	 *  - REGISTRY_PAYLOAD
	 *  - VERSION_RESPONSE (temporal query fan-in)
	 */
	void _handle_consumer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message);

	/**
	 * @brief Handle a message coming from the HTTP server.
	 *
	 * @details Expects classified requests (GET_BY_ID/GET_BY_TYPE/TEMPORAL_QUERY/Subscriptions).
	 * Extracts parameters (unpacked from message) and invokes the appropriate *process_* function.
	 */
	void _handle_http_server_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message);

	/**
	 * @brief Handle a message coming from a Producer (e.g., control/ACK).
	 * @details If payload looks like an ACK, forward it to HTTP via @ref process_ack_message.
	 */
	void _handle_producer_intra_message(snds_intra_message::SNDS_INTRA_MESSAGE& message);

	// ---------------------------------------------------------------------
	// Parsing helpers
	// ---------------------------------------------------------------------

	/**
	 * @brief Extract a single item from a "direct" temporal payload.
	 *
	 * @param content Raw JSON string. Supported shapes:
	 *  - `{"mode":"direct","items":[{...}]}`
	 *  - `[ { "<key or empty>": { ...doc... } } ]` (legacy)
	 *  - `{ ... }` (fallback: treat entire object as the item)
	 * @param out     On success, filled with the extracted object.
	 * @return true if an item was extracted, false otherwise.
	 */
	bool _extract_direct_temporal_item(const std::string& content, nlohmann::json& out);

	std::string _extract_id_fallback(const nlohmann::json& j);
};
