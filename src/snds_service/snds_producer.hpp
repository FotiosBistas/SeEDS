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

struct RegistryEvents
{
	// type_registry
	std::function<void(const std::string &type, const std::string &id)> on_type_add;
	std::function<void(const std::string &type, const std::string &id)> on_type_remove;

	// subscribers by id
	std::function<void(const std::string &id, const std::string &node)> on_subscribe_id;
	std::function<void(const std::string &id, const std::string &node)> on_unsubscribe_id;

	// subscribers by type
	std::function<void(const std::string &type, const std::string &node)> on_subscribe_type;
	std::function<void(const std::string &type, const std::string &node)> on_unsubscribe_type;

	// announcements
	std::function<void(const std::string &name)> on_announce_add;
	std::function<void(const std::string &name)> on_announce_remove;
};

class SNDSProducer : public ServiceBase
{
public:
	SNDSProducer(
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
		size_t      resilience_ipc_port_ =  47832)
		:ServiceBase(
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
			  broker_port)
  		,store_(m_logger)
		,enable_resilience_ipc_(enable_resilience_ipc_)
		,resilience_ipc_host_(resilience_ipc_host_)
		,resilience_ipc_port_(resilience_ipc_port_)
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

	void init();

	void onRegisterSuccess(const ndn::Name &prefix);
	void onInterest(const ndn::Interest &interest);

	void processRegistryRequest(const parsed_interest_name::ParsedInterestName &interest);
	void process_new_subscription_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_scope_notification_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_get_by_id_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_by_type_advertisement_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_get_by_type_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_get_by_id_for_type_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_subscription_notification_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_temporal_query_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_version_metafile_interest(const parsed_interest_name::ParsedInterestName &interest);

	void process_resilience_is_alive_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_resilience_patch_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_resilience_meta_interest(const parsed_interest_name::ParsedInterestName &interest);
	void process_resilience_snapshot_interest(const parsed_interest_name::ParsedInterestName &interest);

	void process_temporal_query_advertisement(const std::string &content);
	void process_by_id_advertisement(const std::string &content);
	void process_temporal_query_request(const std::string &content);

	uint8_t _publish_to_broker(const nlohmann::json &doc);
	std::optional<nlohmann::json> _http_get_json(std::string_view path, const std::vector<std::pair<std::string, std::string>> &q);

	void handleTemporalQuery(parsed_interest_name::ParsedInterestName &interest);
	void parseTemporalQuery(const std::string &body);

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
	std::string secondary_suffix_    = "_secondary";    // collection suffix

	inline bool register_id_for_type(TypeRegistry &reg, RegistryEvents &ev, const std::string &type, const std::string &id)
	{
		auto &ids = reg[type];
		auto [it, inserted] = ids.insert(id);
		if (inserted && ev.on_type_add)
			ev.on_type_add(type, id);
		return inserted;
	}

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

	inline bool subscribe_id(SubscribersByID &m, RegistryEvents &ev, const std::string &id, const std::string &node)
	{
		auto &s = m[id];
		auto [_, inserted] = s.insert(node);
		if (inserted && ev.on_subscribe_id)
			ev.on_subscribe_id(id, node);
		return inserted;
	}

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

	inline bool subscribe_type(SubscribersByType &m, RegistryEvents &ev, const std::string &type, const std::string &node)
	{
		auto &s = m[type];
		auto [_, inserted] = s.insert(node);
		if (inserted && ev.on_subscribe_type)
			ev.on_subscribe_type(type, node);
		return inserted;
	}

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

	inline bool announce(Announcements &a, RegistryEvents &ev, const std::string &name)
	{
		auto [_, inserted] = a.insert(name);
		if (inserted && ev.on_announce_add)
			ev.on_announce_add(name);
		return inserted;
	}

	inline bool unannounce(Announcements &a, RegistryEvents &ev, const std::string &name)
	{
		size_t erased = a.erase(name);
		if (erased && ev.on_announce_remove)
			ev.on_announce_remove(name);
		return erased != 0;
	}


	static inline std::string to_secondary_collection_(const std::string& t, const std::string& suffix)
	{
		return (t.empty() ? "GENERIC" : t) + suffix;
	}

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

	static inline std::string normalize_type_(const nlohmann::json& doc) 
	{
		if (doc.contains("type") && doc["type"].is_string()) 
		{
			const auto& t = doc["type"].get_ref<const std::string&>();
			if (!t.empty()) return t;
		}
		return "GENERIC";
	}

	static nlohmann::json& ensure_obj(nlohmann::json& j, const char* key) 
	{
		if (!j.contains(key) || !j[key].is_object()) j[key] = nlohmann::json::object();
		return j[key];
	}

	static inline nlohmann::json& ensure_arr(nlohmann::json& j, const char* key) 
	{
		if (!j.contains(key) || !j[key].is_array()) j[key] = nlohmann::json::array();
		return j[key];
	}

	static inline std::string j_get_string_or(const nlohmann::json& j, const char* key, std::string_view def)
	{
	    auto it = j.find(key);
	    if (it == j.end()) return std::string(def);
	    if (it->is_string()) return it->get<std::string>();
	    // Fallback: stringify non-strings to avoid silent loss
	    return it->dump();
	}


	// HTTP fetch to Broker /state?type=...
	std::optional<nlohmann::json> fetch_broker_state_json_(const std::string& type);

	// Registry → Store mutations
	void upsert_type_id_(const std::string& type, const std::string& id);
	void erase_type_id_(const std::string& type, const std::string& id);

	void upsert_sub_by_id_(const std::string& id, const std::string& node);
	void erase_sub_by_id_(const std::string& id, const std::string& node);

	void upsert_sub_by_type_(const std::string& type, const std::string& node);
	void erase_sub_by_type_(const std::string& type, const std::string& node);

	// Announcements → Store
	void store_announce_(const std::string& name);
	void store_unannounce_(const std::string& name);

	// Broker snapshot → Store
	void update_store_with_broker_state_(const std::string& type);

	// Keep the last N successful /add payloads so we can replay to a fresh Broker.
	static constexpr size_t broker_journal_max = 5000;

	// Append one entry to the broker journal (called after /add OK).
	void append_broker_journal_(const nlohmann::json& body);

	// Try to replay the journal into the Broker (use on failover or when Broker comes back).
	void rehydrate_broker_from_journal_();

	// Optional: convenience check that decides when to rebuild Broker
	// (e.g., /state fails or collection looks empty after restart).
	void maybe_rehydrate_broker_(const std::string& type);
	void replay_broker_journal_incremental_();

	void start_resilience_ipc_();
  	void stop_resilience_ipc_();

};
