#include <ndn-cxx/face.hpp>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include "httplib.h"

#include "snds_service_base.hpp"
#include "parsed_interest_name.hpp"
#include "temporal_query.hpp"
#include "snds_resilience.hpp"
#include "snds_utils.h"
#include "snds_ipc_tcp.hpp" 

/**
 * @brief Event hooks for registry and subscription changes.
 *
 * This struct aggregates optional callbacks invoked by the producer when the
 * in-memory registry/subscriber state or announcement set changes. Install any
 * subset of these to react to updates (e.g., mirror state elsewhere, update UI,
 * trigger logging/metrics).
*/
struct RegistryEvents
{
	/**
     * @brief Fired when an (type, id) pair is added to the type registry.
     * @param type Logical collection/type name (e.g., "CAR").
     * @param id   The object identifier added under @p type.
    */
	std::function<void(const std::string &type, const std::string &id)> on_type_add;
	/**
     * @brief Fired when an (type, id) pair is removed from the type registry.
	 * @todo There is no NDN logic implemented for removing types.
     * @param type Logical collection/type name.
     * @param id   The object identifier removed from @p type.
    */
	std::function<void(const std::string &type, const std::string &id)> on_type_remove;

	/**
     * @brief Fired when a node subscribes to updates for a specific ID.
     * @param id   Target object identifier being subscribed to.
     * @param node Subscriber's SNDS node identifier.
    */
	std::function<void(const std::string &id, const std::string &node)> on_subscribe_id;
	/**
     * @brief Fired when a node unsubscribes from a specific ID.
	 * @todo There is no NDN logic implemented for removing subscriptions.
     * @param id   Target object identifier being unsubscribed from.
     * @param node Subscriber's SNDS node identifier.
    */
	std::function<void(const std::string &id, const std::string &node)> on_unsubscribe_id;

	/**
     * @brief Fired when a node subscribes to a content TYPE (all IDs under it).
     * @param type Target content TYPE (collection) key.
     * @param node Subscriber's SNDS node identifier.
    */
	std::function<void(const std::string &type, const std::string &node)> on_subscribe_type;

	/**
     * @brief Fired when a node unsubscribes from a content TYPE.
	 * @todo There is no NDN logic implemented for removing type subscriptions.
     * @param type Target content TYPE (collection) key.
     * @param node Subscriber's SNDS node identifier.
    */
	std::function<void(const std::string &type, const std::string &node)> on_unsubscribe_type;

	/**
     * @brief Fired when a new NDN name is announced (made available) by this producer.
     * @param name Fully-qualified NDN name (URI) that was announced.
    */
	std::function<void(const std::string &name)> on_announce_add;
	/**
     * @brief Fired when a previously announced NDN name is unannounced/withdrawn.
	 * @todo There is no NDN logic implemented for removing announcements
     * @param name Fully-qualified NDN name (URI) that was removed.
    */
	std::function<void(const std::string &name)> on_announce_remove;
};

/**
 * @class SNDSProducer
 * @brief NDN producer that advertises names, serves Interests, proxies to an HTTP broker,
 *        and maintains a resilient, versioned in-memory state.
 *
 * @details
 * Responsibilities:
 *  - Registers Interest filters for IDs, scopes, liveness, state, per-type names, and registry files.
 *  - Parses incoming Interests and dispatches to handlers (by-ID/by-TYPE fetch, temporal query,
 *    version metafile, and resilience endpoints: is_alive/meta/snapshot/patch).
 *  - Maintains a type→IDs registry, subscription maps (by ID and by TYPE), and an announcement set,
 *    emitting user-provided callbacks (@ref RegistryEvents) on changes.
 *  - Proxies content retrieval to an HTTP Broker (e.g., /get, /temporal-query, /state, /dump, /copy, /add).
 *  - Keeps a bounded broker “journal” for rehydration/mirroring and can replay it on failover or restart.
 *  - Optionally runs a resilience IPC server to receive snapshots/patches/role changes and tracks
 *    PRIMARY/SECONDARY role in an internal @ref snds_resilience::Store.
 *
 * Lifecycle:
 *  - Construction initializes the store with a canonical JSON layout (types/subscribers/announcements/broker),
 *    sets an initial resilience role (PRIMARY if IPC is disabled; SECONDARY otherwise), and seeds the
 *    in-memory type registry with @p registryType. It does **not** start threads or register NDN filters.
 *  - Call @ref init() to register prefixes, install event hooks, optionally start the IPC server,
 *    seed from the Broker dump, and enter the event loop (processing NDN events and intra-queue messages).
 *
 * Threading/Concurrency:
 *  - The main NDN work runs in the @ref init() loop via ndn-cxx Face event processing.
 *  - The resilience IPC server (if enabled) runs on a separate thread and modifies the store via
 *    well-scoped methods; atomic flags coordinate role and replay ticks.
 *  - The registry/subscription/announcement containers are not inherently thread-safe; they are intended
 *    to be mutated on the producer’s event thread. Synchronize externally if accessed from other threads.
 *
 * Ownership:
 *  - Inherits logging, Face, KeyChain, and configuration from @ref ServiceBase.
 *  - Does not take ownership of external services beyond what @ref ServiceBase manages.
*/
class SNDSProducer : public ServiceBase
{
public:
	/**
	 * @brief Construct a producer with logging, naming, broker, and resilience configuration.
	 *
	 * @param logLevel               spdlog verbosity for this service.
	 * @param logFile                Log file name (passed to @ref ServiceBase).
	 * @param number_of_snds_services Number of worker services/threads (passed to base).
	 * @param snds_prefix            Root SNDS prefix (e.g., "SNDS") used to build NDN names.
	 * @param snds_state_prefix      State name component prefix (e.g., "state").
	 * @param snds_is_alive_prefix   Liveness name component prefix (e.g., "is_alive").
	 * @param snds_id                Logical node/instance identifier (e.g., "0").
	 * @param id_scope_prefix        Scope prefix for node-scoped names (e.g., "SNDS_ID_scope_").
	 * @param registry_file_postfix  Postfix for per-type registry files (e.g., "_registry").
	 * @param serviceName            Human-readable service name for logs/metrics.
	 * @param broker_ip              HTTP Broker host/IP for proxy operations.
	 * @param broker_port            HTTP Broker port.
	 * @param registryType           Initial content TYPE key to seed in the registry (e.g., "CAR").
	 * @param enable_resilience_ipc_ If true, the producer is initialized as SECONDARY and
	 *                               may start a resilience IPC listener in @ref init();
	 *                               if false, initializes as PRIMARY and does not start IPC.
	 * @param resilience_ipc_host_   Host interface on which to listen for resilience IPC.
	 * @param resilience_ipc_port_   TCP port on which to listen for resilience IPC.
	 *
	 * @details
	 *  - Initializes the versioned @ref snds_resilience::Store with a canonical JSON document containing:
	 *    {types, subscribers{byId/byType}, announcements, broker{endpoint,collections}, resilience{role,since,reason}}.
	 *    This is recorded as a new head in the Store (version is incremented there).
	 *  - Sets the initial resilience role to "secondary" when IPC is enabled, otherwise "primary".
	 *  - Seeds @ref type_registry with @p registryType (empty ID set).
	 *  - Logs the effective Broker and IPC endpoints for observability.
	 *
	 * @note This constructor does not register NDN prefixes or start any threads. Call @ref init()
	 *       to begin processing, register Interest filters, and (optionally) start the IPC server.
	*/
	SNDSProducer
	(
		spdlog::level::level_enum logLevel = spdlog::level::info,
		std::string logFile = "Producer.log",
		size_t number_of_snds_services = 1,
		std::string snds_prefix = "SNDS",
		std::string snds_state_prefix = "state",
		std::string snds_is_alive_prefix = "is_alive",
		std::string snds_id = "0",
		std::string id_scope_prefix = "SNDS_ID_scope_",
		std::string registry_file_postfix = "_registry",
		std::string serviceName = "Producer",
		std::string broker_ip = "0.0.0.0",
		size_t broker_port = 10000,
		std::string registryType = "CAR",
		bool enable_resilience_ipc_ = true,
		std::string resilience_ipc_host_ = "127.0.0.1",
		size_t      resilience_ipc_port_ =  47832,
		bool am_primary_ = true
	):
	ServiceBase
	(
		logLevel,
		logFile,
		number_of_snds_services,
		snds_prefix,
		snds_state_prefix,
		snds_is_alive_prefix,
		snds_id,
		id_scope_prefix,
		registry_file_postfix,
		serviceName,
		broker_ip,
		broker_port
	)
  		,store_(m_logger)
		,enable_resilience_ipc_(enable_resilience_ipc_)
		,resilience_ipc_host_(resilience_ipc_host_)
		,resilience_ipc_port_(resilience_ipc_port_)
		,am_primary_(am_primary_)
	{

		nlohmann::json init = 
		{
			{"types", nlohmann::json::object()},
			{"subscribers", {{"byId", nlohmann::json::object()}, {"byType", nlohmann::json::object()}}},
			{"announcements", nlohmann::json::object()},
			{"broker", 
				{
					{"endpoint", fmt::format("{}:{}", this->broker_ip, this->broker_port)},
					{"collections", nlohmann::json::object()}
				}
			}
		};

		if (enable_resilience_ipc_) 
		{
			init["resilience"] = 
			{
				{"role",   "secondary"},
				{"since",  iso8601_now_utc()},
				{"reason", "startup"}
			};
		} 
		else
		{
			init["resilience"] = 
			{
				{"role", "primary"},
				{"since", iso8601_now_utc()},
				{"reason", "startup"}
			};
		}
		store_.assign(std::move(init));

		m_logger->info("SNDSProducer initialized with serviceName: {}", this->serviceName);
		m_logger->info("SNDSProducer will connect to Broker with IP: {} and port: {}", this->broker_ip, this->broker_port);
		m_logger->info("SNDSProducer will connect to IPC with IP: {} and port: {}", this->resilience_ipc_host_, this->resilience_ipc_port_);
		type_registry[registryType] = std::unordered_set<std::string>{};
	}

	/**
	 * @brief Initialize and run the SNDSProducer main loop.
	 *
	 * Sets up NDN interest filters (ID, scope, liveness, state, per-type and registry files),
	 * installs event hooks, optionally starts the resilience IPC server, seeds the local broker
	 * from a dump, and then enters the event loop. While running, the producer processes NDN
	 * events and intra-queue messages (e.g., temporal query ads/requests, by-ID ads).
	 *
	 * Behavior details:
	 * - If promoted to PRIMARY at runtime, the IPC listener is stopped.
	 * - When SECONDARY and a rehydrate tick is set, broker journal entries are replayed.
	 * - Successful prefix registrations are recorded via onRegisterSuccess().
	 *
	 * @note Exits when @c running becomes false. May create a default identity in the PIB if none exists.
	*/
	void init();
	/** @brief Callback fired after a prefix is registered with the NDN forwarder.
	 *  @param prefix The prefix that was successfully registered.
	 *  @details Logs the registration and announces it on the internal event bus.
	*/
	void onRegisterSuccess(const ndn::Name &prefix);
	/** @brief Entry point for all incoming Interests on announced prefixes.
	 *  @param interest The received Interest.
	 *  @details Parses the name, dispatches to the appropriate handler (GET_BY_ID,
	 *  GET_BY_TYPE, TEMPORAL_QUERY, resilience endpoints, etc.), and replies via sendData().
	*/
	void onInterest(const ndn::Interest &interest);
	/**
	 * @brief Handle a newly announced subscription (by ID or by TYPE) from a subscriber node.
	 *
	 * Updates internal subscription maps, emits events via @c events_, and responds with an ACK/NACK
	 * over NDN (using the interest name as the data name).
	 *
	 * @param interest Parsed interest carrying: our node scope as @c name, and params: [target, subscriberNode].
	 *                 Command is either NEW_ID_SUBSCRIPTION or NEW_TYPE_SUBSCRIPTION.
	 *
	 * @details
	 * - Param[0] is the subscribed target (ID or TYPE, depending on command).
	 * - Param[1] is the subscriber’s SNDS node id.
	 * - On success, the function stores the subscription and replies @c ACK_SUBSCRIPTION_NEW;
	 *   on duplicate, replies @c ACK_SUBSCRIPTION_EXISTS; on malformed, replies @c NACK_ERROR.
	 *
	 * @note Triggers @c events_.on_subscribe_id / @c events_.on_subscribe_type.
	*/
	void process_new_subscription_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Notify all current subscribers that the scope (ID/TYPE) has changed.
	 *
	 * Reads subscribers from the in-memory store and emits NDN SUBSCRIPTION_NOTIFICATION interests
	 * toward each subscriber node; replies to the original interest with an ACK.
	 *
	 * @param interest Parsed interest with params: [id, type?]. If type is empty, treat as BY_ID.
	 *
	 * @note Sends @c ACK_SUBSCRIPTION_NO_SUBSCRIBERS if no listeners exist; otherwise
	 *       issues SUBSCRIPTION_NOTIFICATION to each subscriber and responds with
	 *       @c ACK_SUBSCRIPTION_NOTIFICATION.
	*/
	void process_scope_notification_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a "get-by-id" data request.
	 *
	 * Extracts the ID parameter and proxy-fetches the latest record from the HTTP broker (/get?id=...),
	 * then returns the broker payload as NDN Data (signed by our keychain).
	 *
	 * @param interest Parsed interest with param[0] = ID to fetch.
	 *
	 * @note On broker error, returns @c NACK_ERROR payload.
	*/
	void process_get_by_id_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Handle an "ID+TYPE advertisement" interest (producer announces an ID belongs to a TYPE).
	 *
	 * Registers the (type,id) association in the registry set; returns an ACK or a WARNING if already registered.
	 *
	 * @param interest Parsed interest with params: [id, type].
	*/
	void process_by_type_advertisement_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a "get-by-type" registry file request.
	 *
	 * Serializes the type registry entry as a compact binary payload (prefixed by 'R', 0x00),
	 * then publishes it as NDN Data to the caller.
	 *
	 * @param interest Parsed interest with param[0] = type key.
	 *
	 * @warning Returns nothing (logs a warning) if the requested type is unknown.
	*/
	void process_get_by_type_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a "get-by-id-for-type" request (ID resolved in the context of a TYPE).
	 *
	 * Proxies to broker @c /get?id=... and returns the latest record as NDN Data.
	 *
	 * @param interest Parsed interest with params: [id, type].
	 *
	 * @note Type is currently used for request semantics/validation but the broker fetch is by ID.
	*/
	void process_get_by_id_for_type_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Forward a subscription notification to interested parties (reverse direction).
	 *
	 * Builds a GET_BY_ID or GET_BY_TYPE interest (depending on params) and drops a message on the
	 * intra-queue to the consumer, then ACKs the notifier.
	 *
	 * @param interest Parsed interest with params: [id, type?, notifyingScope?].
	*/
	void process_subscription_notification_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a temporal-query Interest.
	 *
	 * Parses key=value params (id, type, date, timeframe, count, filters),
	 * proxies an HTTP GET to the Broker `/temporal-query`, and replies with the
	 * Broker JSON body as NDN Data (pretty-printed) on success.
	 *
	 * @param interest Parsed interest carrying the temporal-query parameters.
	 * @note On broker error, only logs a failure (no payload sent) in the current implementation.
	*/
	void process_temporal_query_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a "version metafile" request for an ID (latest object metadata).
	 *
	 * Calls the broker @c /get?id=... and returns the body as-is (JSON) over NDN.
	 *
	 * @param interest Parsed interest with param[0] = key=value pair where key is an ID key and value the ID.
	 *
	 * @note On broker failure, returns @c NACK_ERROR.
	*/
	void process_version_metafile_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Reply to a resilience liveness probe.
	 *
	 * Expects a single parameter (nonce) and returns JSON: { "ack": true, "nonce": <echo>, "ts": <ISO-8601> }.
	 * If the nonce is missing, returns { "ack": false, "reason": "missing-nonce" }.
	 *
	 * Typical use: fast health checks / round-trip verification between PRIMARY and SECONDARY.
	 *
	 * @param interest Parsed interest with param[0] = caller-supplied nonce.
	*/
	void process_resilience_is_alive_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a resilience patch request (from_version → to_version) targeting the current head.
	 *
	 * Validates parameters (from_version, to_version), ensures to_version equals the authoritative
	 * store head, and emits a JSON patch (kind=patch) containing ops/hash/versions.
	 *
	 * Error responses (JSON) include one of:
	 * - {"error":"missing_parameters" | "invalid_parameters" | "non_increasing_version"}
	 * - {"error":"unknown_target_version" | "patch_unavailable" | "store_advanced"}
	 * Each error also includes {"advice":"fetch_snapshot","latest":<head>} to guide the client.
	 *
	 * @param interest Parsed interest with params: [from_version, to_version].
	*/
	void process_resilience_patch_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Return lightweight metadata about the current store head.
	 *
	 * Produces JSON: {"version": <uint64>, "hash": "<md5>", "ts": "<ISO-8601>", "state_name": "<name>"}.
	 * Intended for quick consistency checks prior to patch/snapshot flows.
	 *
	 * @param interest Parsed interest (no parameters required).
	*/
	void process_resilience_meta_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Serve a resilience snapshot for a requested target version (must equal current head).
	 *
	 * Validates an optional @c to_version parameter (param[0]); if it matches the store head and the
	 * store has not advanced mid-flight, returns the full snapshot as JSON (kind=snapshot).
	 * Otherwise, returns a structured error advising the client to fetch a fresh snapshot.
	 *
	 * Possible errors: "missing_parameters", "invalid_parameters", "unknown_target_version", "store_advanced".
	 *
	 * @param interest Parsed interest with optional param[0] = to_version.
	*/
	void process_resilience_snapshot_interest(const parsed_interest_name::ParsedInterestName &interest);
	/**
	 * @brief Advertise a temporal-query name and publish its descriptor to the broker.
	 *
	 * Deserializes the temporal query, optionally registers (type,id) in registry if @c type is set,
	 * announces the derived NDN name, publishes a JSON doc to @c /add, and replies with ACK.
	 *
	 * @param content Serialized temporal-query JSON.
	 *
	 * @note The announced name is built with @c temporal_query::to_path and @c to_custom_ndn_name.
	*/
	void process_temporal_query_advertisement(const std::string &content);
	/**
	 * @brief Request-time temporal query (pull) and advertise its name for data follow-ups.
	 * @deprecated Use process_temporal_query_advertisement() instead.
	 * Builds/announces the query name, POSTs to the broker @c /add for bookkeeping,
	 * and logs success/failure. Intended for on-demand queries from workers.
	 *
	 * @param content Serialized temporal-query JSON.
	*/
	void process_temporal_query_request(const std::string &content);
	/**
	 * @brief Advertise an object by ID (NDN announcement) from a string-encoded interest path.
	 *
	 * Parses the input path to a @c ParsedInterestName, rebuilds a canonical NDN name and registers
	 * an interest filter; replies with @c ACK_ID_ADVERTISED over intra.
	 *
	 * @param content String path that encodes prefix + id (e.g., "/prefix/<id>").
	*/
	void process_by_id_advertisement(const std::string &content);


private:

  	RegistryEvents events_;

	using TypeRegistry = std::unordered_map<std::string, std::unordered_set<std::string>>;
	using SubscribersByID = std::unordered_map<std::string, std::unordered_set<std::string>>;
	using SubscribersByType = std::unordered_map<std::string, std::unordered_set<std::string>>;
	using Announcements = std::unordered_set<std::string>;

	TypeRegistry type_registry;					 // maps Type names to vectors of IDs
	SubscribersByID subscribed_nodes_for_id;	 // maps ID to subscribed nodes
	SubscribersByType subscribed_nodes_for_type; // maps TYPE to subscribed nodes
	Announcements announcements;

	snds_resilience::Store  store_;

	// role/flag (set from ctor, CLI, or env)
  	bool enable_resilience_ipc_ = false;

  	// IPC config
  	std::string resilience_ipc_host_ = "127.0.0.1";
  	uint16_t    resilience_ipc_port_ = 47832;

  	// thread control
  	std::thread              resilience_ipc_thr_;
  	std::atomic<bool>        resilience_ipc_running_{false};
	std::atomic<bool>        am_primary_{false};

	// --- incremental broker replay state ---
	std::atomic<bool> rehydrate_tick_{false};   // set on each patch/snapshot while secondary
	size_t           broker_journal_cursor_{0}; // next index to replay
	std::string      last_replayed_fp_;         // fingerprint of last replayed entry

	bool        mirror_to_secondary_ = true;            // toggle for your debug mirroring
	std::string secondary_suffix_    = "_SECONDARY";    // collection suffix

	/**
	 * @brief Register an (type, id) pair in the type registry (idempotent insert).
	 *
	 * Inserts @p id under @p type in @p reg. If this is a new association, fires
	 * RegistryEvents::on_type_add(type, id).
	 *
	 * @param reg  Type-to-ID registry container.
	 * @param ev   Event hooks; may be partially empty.
	 * @param type Logical collection/type key.
	 * @param id   Object identifier to register under @p type.
	 * @return true if the (type,id) association was newly inserted; false if it already existed.
	 * @see RegistryEvents::on_type_add
	 * @note Not thread-safe; synchronize if @p reg is shared across threads.
	*/
	inline bool register_id_for_type(TypeRegistry &reg, RegistryEvents &ev, const std::string &type, const std::string &id)
	{
		auto &ids = reg[type];
		auto [it, inserted] = ids.insert(id);
		if (inserted && ev.on_type_add)
			ev.on_type_add(type, id);
		return inserted;
	}

	/**
	 * @brief Unregister an (type, id) pair from the type registry.
	 *
	 * If the association exists, removes it and fires RegistryEvents::on_type_remove(type, id).
	 *
	 * @param reg  Type-to-ID registry container.
	 * @param ev   Event hooks; may be partially empty.
	 * @param type Logical collection/type key.
	 * @param id   Object identifier to remove from @p type.
	 * @return true if an entry was removed; false if (type) was unknown or @p id not present.
	 * @see RegistryEvents::on_type_remove
	 * @note Not thread-safe; synchronize if @p reg is shared across threads.
	*/
	inline bool unregister_id_for_type(TypeRegistry &reg, RegistryEvents &ev, const std::string &type, const std::string &id)
	{
		auto it = reg.find(type);
		if (it == reg.end())
			return false;
		size_t erased = it->second.erase(id);
		if (erased && ev.on_type_remove)
			ev.on_type_remove(type, id);
		return erased != 0;
	}

	/**
	 * @brief Subscribe a node to updates for a specific ID (idempotent insert).
	 *
	 * Adds @p node to the subscriber set for @p id. On first insertion, fires
	 * RegistryEvents::on_subscribe_id(id, node).
	 *
	 * @param m     Map from ID → set of subscriber nodes.
	 * @param ev    Event hooks; may be partially empty.
	 * @param id    Object identifier to subscribe to.
	 * @param node  Subscriber SNDS node identifier.
	 * @return true if the node was newly added; false if it was already subscribed.
	 * @see RegistryEvents::on_subscribe_id
	 * @note Not thread-safe; synchronize if @p m is shared across threads.
	*/
	inline bool subscribe_id(SubscribersByID &m, RegistryEvents &ev, const std::string &id, const std::string &node)
	{
		auto &s = m[id];
		auto [_, inserted] = s.insert(node);
		if (inserted && ev.on_subscribe_id)
			ev.on_subscribe_id(id, node);
		return inserted;
	}

	/**
	 * @brief Unsubscribe a node from a specific ID.
	 *
	 * Removes @p node from the subscriber set for @p id. On successful removal,
	 * fires RegistryEvents::on_unsubscribe_id(id, node).
	 *
	 * @param m     Map from ID → set of subscriber nodes.
	 * @param ev    Event hooks; may be partially empty.
	 * @param id    Object identifier to unsubscribe from.
	 * @param node  Subscriber SNDS node identifier.
	 * @return true if the node was removed; false if the ID or node was not present.
	 * @see RegistryEvents::on_unsubscribe_id
	 * @note Not thread-safe; synchronize if @p m is shared across threads.
	*/
	inline bool unsubscribe_id(SubscribersByID &m, RegistryEvents &ev, const std::string &id, const std::string &node)
	{
		auto it = m.find(id);
		if (it == m.end())
			return false;
		size_t erased = it->second.erase(node);
		if (erased && ev.on_unsubscribe_id)
			ev.on_unsubscribe_id(id, node);
		return erased != 0;
	}

	/**
	 * @brief Subscribe a node to a TYPE (all IDs under it) (idempotent insert).
	 *
	 * Adds @p node to the subscriber set for @p type. On first insertion, fires
	 * RegistryEvents::on_subscribe_type(type, node).
	 *
	 * @param m     Map from TYPE → set of subscriber nodes.
	 * @param ev    Event hooks; may be partially empty.
	 * @param type  Target TYPE/collection key.
	 * @param node  Subscriber SNDS node identifier.
	 * @return true if the node was newly added; false if it was already subscribed.
	 * @see RegistryEvents::on_subscribe_type
	 * @note Not thread-safe; synchronize if @p m is shared across threads.
	*/
	inline bool subscribe_type(SubscribersByType &m, RegistryEvents &ev, const std::string &type, const std::string &node)
	{
		auto &s = m[type];
		auto [_, inserted] = s.insert(node);
		if (inserted && ev.on_subscribe_type)
			ev.on_subscribe_type(type, node);
		return inserted;
	}

	/**
	 * @brief Unsubscribe a node from a TYPE.
	 *
	 * Removes @p node from the subscriber set for @p type. On successful removal,
	 * fires RegistryEvents::on_unsubscribe_type(type, node).
	 *
	 * @param m     Map from TYPE → set of subscriber nodes.
	 * @param ev    Event hooks; may be partially empty.
	 * @param type  Target TYPE/collection key.
	 * @param node  Subscriber SNDS node identifier.
	 * @return true if the node was removed; false if the TYPE or node was not present.
	 * @see RegistryEvents::on_unsubscribe_type
	 * @note Not thread-safe; synchronize if @p m is shared across threads.
	*/
	inline bool unsubscribe_type(SubscribersByType &m, RegistryEvents &ev, const std::string &type, const std::string &node)
	{
		auto it = m.find(type);
		if (it == m.end())
			return false;
		size_t erased = it->second.erase(node);
		if (erased && ev.on_unsubscribe_type)
			ev.on_unsubscribe_type(type, node);
		return erased != 0;
	}

	/**
	 * @brief Mark an NDN name as announced (idempotent insert).
	 *
	 * Inserts @p name into the announcement set and, on first insertion, fires
	 * RegistryEvents::on_announce_add(name).
	 *
	 * @param a     Announcement set.
	 * @param ev    Event hooks; may be partially empty.
	 * @param name  Fully-qualified NDN name (URI) being announced.
	 * @return true if the name was newly inserted; false if it already existed.
	 * @see RegistryEvents::on_announce_add
	 * @note Not thread-safe; synchronize if @p a is shared across threads.
	*/
	inline bool announce(Announcements &a, RegistryEvents &ev, const std::string &name)
	{
		auto [_, inserted] = a.insert(name);
		if (inserted && ev.on_announce_add)
			ev.on_announce_add(name);
		return inserted;
	}


	/**
	 * @brief Mark an NDN name as unannounced (remove if present).
	 *
	 * Erases @p name from the announcement set and, on successful removal, fires
	 * RegistryEvents::on_announce_remove(name).
	 *
	 * @param a     Announcement set.
	 * @param ev    Event hooks; may be partially empty.
	 * @param name  Fully-qualified NDN name (URI) being withdrawn.
	 * @return true if the name was removed; false if it was not present.
	 * @see RegistryEvents::on_announce_remove
	 * @note Not thread-safe; synchronize if @p a is shared across threads.
	*/
	inline bool unannounce(Announcements &a, RegistryEvents &ev, const std::string &name)
	{
		size_t erased = a.erase(name);
		if (erased && ev.on_announce_remove)
			ev.on_announce_remove(name);
		return erased != 0;
	}


	/**
	 * @brief Build the secondary collection name.
	 *
	 * Returns the primary collection name with @p suffix appended. If @p t is empty,
	 * the string "GENERIC" is used as the base.
	 *
	 * @param t      Primary collection/type name (may be empty).
	 * @param suffix Suffix to append (e.g., "_SECONDARY" or "_VERSIONED").
	 * @return Secondary collection name.
	*/
	static inline std::string to_secondary_collection_(const std::string& t, const std::string& suffix)
	{
		return (t.empty() ? "GENERIC" : t) + suffix;
	}

	/**
	 * @brief Compute a stable, human-readable fingerprint for a journal entry.
	 *
	 * Concatenates fields that are replicated 1:1 via snapshots/patches plus a
	 * serialized payload for uniqueness. Format:
	 *   ts "|" type "|" id "|" payloadDump
	 *
	 * @param e Journal entry JSON (expects keys "ts","id","type","payload"?).
	 * @return Pipe-delimited fingerprint string.
	 *
	 * @note Using payload .dump() may produce large strings; suitable for identity checks,
	 *       logs, or cursor anchoring—not cryptographic hashing.
	*/
	static inline std::string fingerprint_entry_(const nlohmann::json& e)
	{
	    // Use fields that are replicated 1:1 via snapshots/patches.
	    // Add payload dump for extra uniqueness.
	    const std::string ts   = e.value("ts",   "");
	    const std::string id   = e.value("id",   "");
	    const std::string type = e.value("type", "");
	    const std::string pl   = e.contains("payload") ? e["payload"].dump() : "";
	    return ts + "|" + type + "|" + id + "|" + pl;
	}

	/**
	 * @brief Get current UTC time as ISO-8601 string.
	 *
	 * Format: YYYY-MM-DDThh:mm:ssZ (e.g., "2025-09-17T12:34:56Z").
	 *
	 * @return Current UTC timestamp in ISO-8601 basic form with 'Z'.
	 *
	 * @note Uses POSIX gmtime_r for thread safety. On Windows, prefer gmtime_s.
	 *       Precision is seconds; no fractional seconds included.
	*/
	static inline std::string iso8601_now_utc() 
	{
	    using namespace std::chrono;
	    auto now = system_clock::now();
	    std::time_t t = system_clock::to_time_t(now);
	    std::tm tm{};
	    gmtime_r(&t, &tm);
	    char buf[32];
	    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
	    return std::string(buf);
	}


	/**
	 * @brief Extract a non-empty "type" from a document or default to "GENERIC".
	 *
	 * @param doc JSON document that may contain a string field "type".
	 * @return The value of doc["type"] if present and non-empty; otherwise "GENERIC".
	*/
	static inline std::string normalize_type_(const nlohmann::json& doc) 
	{
		if (doc.contains("type") && doc["type"].is_string()) 
		{
			const auto& t = doc["type"].get_ref<const std::string&>();
			if (!t.empty()) return t;
		}
		return "GENERIC";
	}

	/**
	 * @brief Ensure that @p j[key] exists and is an object; create it if needed.
	 *
	 * @param j   Parent JSON object to mutate.
	 * @param key Key under which an object is required.
	 * @return Reference to the ensured object at @p j[key].
	*/
	static nlohmann::json& ensure_obj(nlohmann::json& j, const char* key) 
	{
		if (!j.contains(key) || !j[key].is_object()) j[key] = nlohmann::json::object();
		return j[key];
	}

	/**
	 * @brief Ensure that @p j[key] exists and is an array; create it if needed.
	 *
	 * @param j   Parent JSON object to mutate.
	 * @param key Key under which an array is required.
	 * @return Reference to the ensured array at @p j[key].
	*/
	static inline nlohmann::json& ensure_arr(nlohmann::json& j, const char* key) 
	{
		if (!j.contains(key) || !j[key].is_array()) j[key] = nlohmann::json::array();
		return j[key];
	}

	/**
	 * @brief Get a string value by key or a default; stringifies non-string JSON values.
	 *
	 * Looks up @p key in @p j:
	 *  - If missing: returns @p def.
	 *  - If a string: returns that string.
	 *  - Otherwise: returns JSON-serialized text of the value (e.g., numbers/objects/arrays).
	 *
	 * @param j   Source JSON object.
	 * @param key Key to retrieve.
	 * @param def Default string view to return if missing.
	 * @return String value, or @p def, or serialized non-string value.
	*/
	static inline std::string j_get_string_or(const nlohmann::json& j, const char* key, std::string_view def)
	{
	    auto it = j.find(key);
	    if (it == j.end()) return std::string(def);
	    if (it->is_string()) return it->get<std::string>();
	    // Fallback: stringify non-strings to avoid silent loss
	    return it->dump();
	}
	/** @brief Publish a JSON document to the Broker (/add).
	 *  @param doc JSON payload; must contain string field "id"; "type" is recommended.
	 *  @return 0 on success; -1 on failure (validation or HTTP error).
	*/
	uint8_t _publish_to_broker(const nlohmann::json &doc);
	/** @brief Issue an HTTP GET to the Broker and parse JSON.
	 *  @param path Endpoint path (e.g. "/state").
	 *  @param q    Key/value query parameters (URL-encoded internally).
	 *  @return Parsed JSON on HTTP 200 and valid body; std::nullopt otherwise.
	*/
	std::optional<nlohmann::json> _http_get_json(std::string_view path, const std::vector<std::pair<std::string, std::string>> &q);
	// HTTP fetch to Broker /state?type=...
	/** @brief Convenience: GET /state?type=… from the Broker.
	 *  @param type Collection/type name (e.g. "CAR"); caller may pass "GENERIC".
	 *  @return JSON with broker state fields (algo/hash/count) or std::nullopt.
	*/
	std::optional<nlohmann::json> fetch_broker_state_json_(const std::string& type);
	/**
	 * @brief Ensure an ID exists under a type in the in-memory store.
	 * @details Mutates JSON at state.types[type].ids[id] = true.
	 * @param type Logical content type/collection.
	 * @param id   Unique identifier to upsert under the type.
	*/
	void upsert_type_id_(const std::string& type, const std::string& id);
	/**
	 * @brief Remove an ID from a type in the in-memory store.
	 * @details Cleans up empty containers as needed.
	 * @param type Logical content type/collection.
	 * @param id   Identifier to erase from the type.
	*/
	void erase_type_id_(const std::string& type, const std::string& id);
	/**
	 * @brief Add/overwrite a subscriber node for a specific ID in the store.
	 * @param id   The ID being subscribed to.
	 * @param node The subscriber node name.
	*/
	void upsert_sub_by_id_(const std::string& id, const std::string& node);
	/**
	 * @brief Remove a subscriber node for a specific ID in the store.
	 * @param id   The ID being unsubscribed from.
	 * @param node The subscriber node name.
	*/
	void erase_sub_by_id_(const std::string& id, const std::string& node);
	/**
	 * @brief Add/overwrite a subscriber node for a specific type in the store.
	 * @param type The type being subscribed to.
	 * @param node The subscriber node name.
	*/
	void upsert_sub_by_type_(const std::string& type, const std::string& node);
	/**
	 * @brief Remove a subscriber node for a specific type in the store.
	 * @param type The type being unsubscribed from.
	 * @param node The subscriber node name.
	*/
	void erase_sub_by_type_(const std::string& type, const std::string& node);
	/**
	 * @brief Record an announcement name in the store.
	 * @param name Name/path that was announced.
	*/
	void store_announce_(const std::string& name);
	/**
	 * @brief Remove an announcement name from the store.
	 * @param name Name/path that was unannounced.
	*/
	void store_unannounce_(const std::string& name);
	// Broker snapshot → Store
	/**
	 * @brief Refresh per-collection broker metadata in the store.
	 * @details GETs /state?type=… and updates algo/hash/count/lastSeen, plus endpoint.
	 * @param type Collection/type whose metadata to refresh (defaults internally to "GENERIC").
 	*/
	void update_store_with_broker_state_(const std::string& type);
	// Keep the last N successful /add payloads so we can replay to a fresh Broker.
	/**
	 * @brief Maximum broker journal entries to retain in memory for replay.
	 * @details Oldest entries are dropped once this cap is exceeded.
	*/
	static constexpr size_t broker_journal_max = 5000;
	// Append one entry to the broker journal (called after /add OK).
	/**
	 * @brief Append a successful /add payload to the in-memory broker journal.
	 * @details De-dupes identical consecutive payloads; trims to @ref broker_journal_max.
	 * @param body JSON document posted to the Broker (must include "id" and "type").
	*/
	void append_broker_journal_(const nlohmann::json& body);
	// Try to replay the journal into the Broker (use on failover or when Broker comes back).
	/**
	 * @brief Full broker rehydration from the stored journal.
	 * @details Re-POSTs each saved payload to /add; then refreshes per-type fingerprints.
	 *          Advances internal cursor/fingerprint on success.
	 * @warning Network operation; logs failures and continues.
	*/
	void rehydrate_broker_from_journal_();
	// Optional: convenience check that decides when to rebuild Broker
	// (e.g., /state fails or collection looks empty after restart).
	/**
	 * @brief Decide whether and how to rehydrate the Broker at startup.
	 * @details If secondary and journal exists: do full or incremental replay.
	 * @param type Unused (reserved for future heuristics).
	*/
	void maybe_rehydrate_broker_(const std::string& type);
	/**
	 * @brief Incrementally replay broker journal entries since the last known cursor.
	 * @details Uses a fingerprint anchor to resync if the journal was truncated.
	 *          Mirrors to secondary collections if enabled.
	*/
	void replay_broker_journal_incremental_();
	/**
	 * @brief Start the resilience IPC TCP listener thread.
	 * @details Consumes snapshot/patch/meta/role envelopes and updates local state/role.
	 * @note Safe to call multiple times; subsequent calls are no-ops if already running.
	*/
	void start_resilience_ipc_();
	/**
	 * @brief Stop the resilience IPC listener thread.
	 * @details Nudges accept() to unblock, then joins the thread.
	*/
  	void stop_resilience_ipc_();
	// Seed secondary collections from the Broker's current primary data dump.
	// If mirror_to_secondary_ is true, writes to {collection}{secondary_suffix_},
	// otherwise writes back to {collection}.
	/**
	 * @brief Seed (copy/mirror) collections from Broker’s /dump into primary/secondary.
	 * @details POSTs /copy with mirror_to_secondary_ and secondary_suffix_ options.
	 * @note Useful for initial bootstrap or when primary/secondary share a Broker.
	*/
	void seed_secondary_from_broker_dump_();

};
