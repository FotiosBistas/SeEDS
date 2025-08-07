#pragma once

#include <variant>
#include <sstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>  

#include "httplib.h"
#include "snds_utils.h"
#include "temporal_query.hpp"

using InsertOrReplaceResult = std::variant<
    mongocxx::result::insert_one,
    mongocxx::result::replace_one
>;


class Broker{
public: 

    Broker(
        const mongocxx::uri& uri,
        const std::string& database,
        const spdlog::level::level_enum logLevel = spdlog::level::info,
        const std::string& logFile = "Broker.log",
        const std::string& brokerName = "Broker",
        const std::string& ipAddress = "0.0.0.0",
        const int          port = 8080
    );

    inline static void initiateShutdown(){
        shouldShutdown.store(true);
    };

    inline static void handleSigint(int){
        initiateShutdown();
    };


    ~Broker();
    void listen();
private:

    std::string logFile;
    std::string ipAddress;
    std::string database;
    int port;

    mongocxx::client client; 

    static std::atomic<bool> shouldShutdown;

    std::shared_ptr<spdlog::logger> logger;
    //TODO is this correct?
    // or should the broker be determined dynamically?
    std::unordered_map<std::string, std::string> service_locations;

    //utils
    std::vector<std::string> split(std::string_view s, const std::string& delimiter);
    std::pair<std::chrono::_V2::system_clock::time_point, bsoncxx::types::b_date> parse_timestamp(std::string_view date);
    mongocxx::pipeline create_date_pipeline(const Timeframe& frame, const size_t count, std::string_view id, const std::chrono::_V2::system_clock::time_point& base_date, const bsoncxx::types::b_date& parsed_date);
    nlohmann::json normalize_json(const nlohmann::json& j);
    std::string hash_document(bsoncxx::document::view document);
    std::string collection_fingerprint(mongocxx::collection& collection);
    std::string get_from_database(const Timeframe& frame, bsoncxx::v_noabi::document::view json_data);
    std::string compute_collection_hash(const mongocxx::collection& collection);
    std::optional<bsoncxx::document::value> set_generic_type(bsoncxx::v_noabi::document::view view);
    void filter_to_only_id(mongocxx::v_noabi::pipeline& pipeline);
    std::string format_timestamp_ms(long long ms_since_epoch);

    //api
    std::optional<InsertOrReplaceResult> add_one(const std::string& body, const bool versioned);
    nlohmann::json get_one(const std::string& request);
    nlohmann::json get(const std::string& request);
    std::string state(const std::string& request);
    void remove();
};