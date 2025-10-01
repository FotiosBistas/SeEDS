#pragma once

#include <variant>
#include <sstream>
#include <optional>

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

/// @brief Result type for /add: insert_one (versioned) or replace_one (unversioned).
using InsertOrReplaceResult = std::variant<mongocxx::result::insert_one, mongocxx::result::replace_one>;
/// @class Broker
/// @brief Thin HTTP server backed by MongoDB for SNDS data.
///
/// Responsibilities:
///  - Accept JSON documents via /add and write to TYPE + TYPE_VERSIONED
///  - Serve reads via /get (latest) and /temporal-query (time-sliced history)
///  - Produce per-collection fingerprints via /state
///  - Dump/restore helpers via /dump and /copy
class Broker{
public: 
    /**
     * @brief Construct a broker bound to a MongoDB instance and HTTP bind.
     * @param uri        MongoDB connection URI (e.g. mongodb://127.0.0.1:27017).
     * @param database   Database name (e.g. "SNDS").
     * @param logLevel   spdlog level for this broker.
     * @param logFile    Log file path.
     * @param brokerName Logger name (helps when multiple services run).
     * @param ipAddress  HTTP bind address (default 0.0.0.0).
     * @param port       HTTP port (default 8080).
    */
    Broker(
        const mongocxx::uri& uri,
        const std::string& database,
        const spdlog::level::level_enum logLevel = spdlog::level::info,
        const std::string& logFile = "Broker.log",
        const std::string& brokerName = "Broker",
        const std::string& ipAddress = "0.0.0.0",
        const int          port = 8080
    );
    /// @brief Programmatic shutdown request; polled in listen() loop.
    inline static void initiateShutdown(){
        shouldShutdown.store(true);
    };
    /// @brief SIGINT handler that triggers graceful shutdown.
    inline static void handleSigint(int){
        initiateShutdown();
    };
    /// @brief Destructor (no special resource ownership).
    ~Broker();
    /**
     * @brief Start the HTTP server and register all endpoints.
     *
     * Registers handlers for:
     *  - POST /add                → write to TYPE_VERSIONED (insert) + TYPE (upsert)
     *  - GET  /get                → latest doc by id & type
     *  - GET  /temporal-query     → time-sliced reads from TYPE_VERSIONED
     *  - GET  /state              → SHA-256 fingerprint + document count for a collection
     *  - GET  /dump               → dump all or a single collection
     *  - POST /copy               → mirror a JSON dump into target collections
     *
     * Blocks until SIGINT or initiateShutdown() is called.
     */
    void listen();
private:
    // ----- configuration/constants -----
    /// @brief Suffix used to identify mirrored secondary collections.
    static constexpr std::string_view secondary_suffix = "_SECONDARY";
    /// @brief Suffix used to identify versioned collections.
    static constexpr std::string_view versioned_suffix = "_VERSIONED";

    // ----- immutable runtime configuration (HTTP/Mongo) -----
    std::string logFile;    ///< Path to log file used by the broker.
    std::string ipAddress;  ///< HTTP bind address (e.g., "0.0.0.0").
    std::string database;   ///< MongoDB database name.
    int         port;       ///< HTTP port.
    /// @brief MongoDB client connected to the configured URI.
    mongocxx::client client; 
    /// @brief Global shutdown flag for the server loop.
    static std::atomic<bool> shouldShutdown;

    std::shared_ptr<spdlog::logger> logger;
    //TODO is this correct?
    // or should the broker be determined dynamically?
    std::unordered_map<std::string, std::string> service_locations;
    // ----------------------------------------------------------------
    // Utility helpers
    // ----------------------------------------------------------------
    /**
     * @brief Split a string_view by a multi-character delimiter.
     * @param s         Source string_view.
     * @param delimiter Delimiter to split on.
     * @return Vector of tokens (last segment included even if empty).
    */
    std::vector<std::string> split(std::string_view s, const std::string& delimiter);
    /**
     * @brief Parse a human-readable date into chrono + BSON date.
     * @param date Input string (supports YYYY-mm-dd[THH:MM:SS][Z]).
     * @return Pair of (system_clock::time_point, bsoncxx::types::b_date).
     * @note Parsing is basic; timezone/locale handling is minimal.
    */
    std::pair<std::chrono::_V2::system_clock::time_point, bsoncxx::types::b_date> parse_timestamp(std::string_view date);
    /**
     * @brief Build an aggregation pipeline for versioned date-window queries.
     * @param frame       Window semantics (NOW/BEFORE/AFTER).
     * @param count       Max results to return.
     * @param id          Logical object id (_id._id in versioned collection).
     * @param base_date   Base date (reserved for future use).
     * @param parsed_date BSON date used in comparisons.
     * @return Configured MongoDB pipeline.
    */
    mongocxx::pipeline create_date_pipeline(const temporal_query::Timeframe& frame, const size_t count, std::string_view id, const std::chrono::_V2::system_clock::time_point& base_date, const bsoncxx::types::b_date& parsed_date);
    /**
     * @brief Canonicalize JSON by sorting object keys recursively.
     * @param j Arbitrary JSON.
     * @return Canonicalized JSON (arrays preserved, object keys sorted).
    */
    nlohmann::json normalize_json(const nlohmann::json& j);
    /**
     * @brief Stable per-document hash (MD5 of canonicalized JSON).
     * @param document BSON document view.
     * @return Lowercase hex MD5.
    */
    std::string hash_document(bsoncxx::document::view document);
     /**
     * @brief Compute a collection fingerprint.
     * @details Iterates docs in stable _id order, hashes each doc (MD5 over canonical JSON),
     *          then folds into a single SHA-256 digest.
     * @param collection Mongo collection.
     * @return Lowercase hex SHA-256 string.
    */
    std::string collection_fingerprint(mongocxx::collection& collection);
    /**
     * @brief Internal read helper for temporal/latest queries.
     * @details If "date" is present → reads from TYPE_VERSIONED using a pipeline.
     *          Otherwise → reads latest from TYPE by _id.
     * @param frame     Timeframe selection for versioned reads.
     * @param json_data Parsed request as BSON view.
     * @return Extended-JSON string (single doc or array).
    */
    std::string get_from_database(const temporal_query::Timeframe& frame, bsoncxx::v_noabi::document::view json_data);
    /**
     * @brief Compute a full-collection hash (reserved; not used by /state).
     * @param collection Mongo collection.
     * @return Hex hash string.
    */
    std::string compute_collection_hash(const mongocxx::collection& collection);
    /**
     * @brief Ensure a document has a non-empty "type" (defaults to "GENERIC").
     * @param view Source BSON view.
     * @return Modified document if changed; std::nullopt if already valid.
    */
    std::optional<bsoncxx::document::value> set_generic_type(bsoncxx::v_noabi::document::view view);
    /**
     * @brief Project only the `_id` field in a pipeline result.
     * @param pipeline Pipeline to mutate in-place.
    */
    void filter_to_only_id(mongocxx::v_noabi::pipeline& pipeline);
    /**
     * @brief Format milliseconds since epoch as an ISO-like string.
     * @param ms_since_epoch Milliseconds since UNIX epoch.
     * @return String timestamp.
    */
    std::string format_timestamp_ms(long long ms_since_epoch);
    /**
     * @brief Dump all collections (or a single one) as extended JSON arrays.
     * @param collection Optional collection name; dumps all if not provided.
     * @return Object keyed by collection → array of documents (extended JSON).
    */
    nlohmann::json dump_database(const std::optional<std::string>& collection = std::nullopt);
    // ----------------------------------------------------------------
    // HTTP API backends
    // ----------------------------------------------------------------
    /**
     * @brief Insert one payload into TYPE_VERSIONED (insert) or TYPE (upsert).
     * @details
     *  - Adds/overwrites "timestamp".
     *  - For versioned=true, stores {_id:{_id,timestamp}} to enable time slicing.
     *  - Honors input type with *_SECONDARY suffix by mirroring to *_VERSIONED_SECONDARY.
     * @param body      Raw JSON body.
     * @param versioned When true, write to TYPE_VERSIONED; else to TYPE.
     * @return Mongo result variant on success; std::nullopt on error.
    */
    std::optional<InsertOrReplaceResult> add_one(const std::string& body, const bool versioned);
    /**
     * @brief GET /get handler: latest record by id & type.
     * @param request Raw JSON string: {"id":"..","type":".."}.
     * @return Parsed JSON object on success; {} on miss/error.
    */
    nlohmann::json get_one(const std::string& request);
    /**
     * @brief GET /temporal-query handler: slice TYPE_VERSIONED around a date.
     * @param request Raw JSON string with {"id","type","date","timeframe","count"}.
     * @return JSON array of entries keyed by timeframe token → results.
    */
    nlohmann::json get(const std::string& request);
    /**
     * @brief GET /state handler: fingerprint + count for a collection.
     * @details Skips *_SECONDARY collections and returns an empty fingerprint for them.
     * @param request Raw JSON string: {"type":"CollectionName"}.
     * @return JSON string containing {"collection","algo","hash","count"}.
    */
    std::string state(const std::string& request);
    /// @brief Placeholder for deletion API (not implemented).
    void remove();
};