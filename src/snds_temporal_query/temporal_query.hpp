#pragma once
/**
 * @file temporal_query.hpp
 * @brief Data model + helpers for SNDS temporal queries (timeframe, filters, kv forms).
 *
 * @details
 * This header defines:
 *  - #temporal_query::Timeframe: a bitmask enum for NOW/BEFORE/AFTER.
 *  - #temporal_query::Filter and #temporal_query::TemporalQuery: core types used to
 *    represent a temporal lookup with optional filters and extra key/values.
 *  - Converters to/from strings, query strings, path-like encodings, and JSON.
 *  - Stable hashers for use in unordered containers or cache keys.
 *
 * Supported encodings:
 *  - Timeframes: "now", "before", "after", or "+"-joined (e.g. "before+after").
 *  - Filters CSV: "key1,key2,key3" (values left empty by CSV; can be filled elsewhere).
 *  - Query-string KV: "id=...&date=...&timeframe=...&count=...".
 *  - JSON: {"id":"...","date":"...","timeframe":"before+after","filters":"k1,k2", ... }
 *
 * @note URL encode/decode is delegated to @ref url_encode / @ref url_decode from "snds_utils.h".
*/
#include <stdexcept>
#include <sstream>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>

#include <nlohmann/json.hpp>

#include "snds_utils.h"


namespace temporal_query{
    /**
     * @brief Combine a hash seed with a hash value (similar to boost::hash_combine).
     * @param seed In/out seed to mix into.
     * @param h    New hash value to combine.
    */
    inline void hash_combine(std::size_t &seed, std::size_t h) 
    {
        // from boost::hash_combine
        seed ^= h + 0x9e3779b97f4a7c15ULL + (seed<<6) + (seed>>2);
    }
    /**
     * @brief Simple filter descriptor (key/value).
     *
     * @note When parsed from | via @ref parse_filters, values are empty (key-only).
    */
    struct Filter 
    {

        std::string key;   ///< Filter key (field name).
        std::string value; ///< Optional value (may be empty).

        /// @brief Structural equality: key and value must both match.
        bool operator==(Filter const &o) const noexcept 
        {
            return key == o.key
                && value == o.value;
        }
    };


    /// Timeframe is a bitmask enum so multiple values
    ///    like NOW and AFTER can be OR'd together into one variable.
    /**
     * @brief Bitmask of timeframe flags (combinable).
     *
     * @details
     * Flags can be OR'ed together (e.g., NOW|AFTER). Use @ref timeframe_to_string
     * and @ref string_to_timeframe to round-trip between bitmask and string.
    */
    enum class Timeframe : uint8_t 
    {
        NONE   = 0,        // no timeframe selected
        NOW    = 1 << 0,   // binary 0001
        BEFORE = 1 << 1,   // binary 0010
        AFTER  = 1 << 2,   // binary 0100
    };

    // Enable bitwise OR and AND operations on Timeframe values.
    //    This makes combining and testing flags easy.
    /// @brief Bitwise OR for @ref Timeframe flags.
    inline Timeframe operator|(Timeframe a, Timeframe b) 
    {
        // Combine flag bits by casting to integer, OR, then cast back.
        return static_cast<Timeframe>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    /// @brief Bitwise AND for @ref Timeframe flags.
    inline Timeframe operator&(Timeframe a, Timeframe b) 
    {
        // Test common bits by AND-ing underlying integer values.
        return static_cast<Timeframe>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }
    /**
     * @brief Canonical in-memory representation of a temporal query.
     *
     * Fields map to common transports:
     *  - Query string / path-like: id, type, date, timeframe, count
     *  - JSON: same, plus `filters` (CSV) and `extras` (arbitrary KVs)
    */
    struct TemporalQuery
    {
        Timeframe         frame{};   ///< Timeframe bitmask (defaults to NONE).
        std::string       date;      ///< ISO-8601 or free-form date/time (upstream validated).
        std::string       id;        ///< Object identifier.
        std::string       type;      ///< Collection/type (may be empty).
        std::string       count;     ///< Optional sample limit (string form to preserve exact input).
        std::unordered_map<std::string,std::string> extras;  ///< Unknown or extended parameters.
        std::vector<Filter> filters; ///< Optional list of filters.
        /// @brief Structural equality over all fields.
        bool operator==(TemporalQuery const &o) const noexcept 
        {
            return frame   == o.frame
                && date    == o.date
                && id      == o.id
                && type    == o.type
                && count   == o.count
                && extras  == o.extras
                && filters == o.filters;
        }

    };
    /**
     * @brief Convert @ref TemporalQuery to "key=value" fields (URL-encoded by default).
     *
     * @param q                  Source query.
     * @param include_timeframe  Include "timeframe=..." field.
     * @param include_count      Include "count=..." field.
     * @param include_extras     Include extras sorted by key.
     * @param include_filters    Include "filters=..." CSV field.
     * @param url_encode         URL-encode both keys and values if true.
     * @return Vector of "k=v" strings.
    */
    std::vector<std::string> to_fields
    (
        const temporal_query::TemporalQuery& q,
        bool include_timeframe = true,
        bool include_count     = true,
        bool include_extras    = true,
        bool include_filters   = true,
        bool url_encode        = true
    );
    /**
     * @brief Serialize as a canonical query-string (stable key ordering for extras).
     * @param q Source query.
     * @return "timeframe=...&date=...&id=...&type=...&count=...&extras..." (unencoded).
    */
    std::string serialize_kv(const TemporalQuery& q);
    /**
     * @brief Build a @ref TemporalQuery from a key→value map (unknown keys → extras).
     * @param params Input map (already decoded).
     * @return Filled query object.
     * @throws std::invalid_argument if timeframe tokens are invalid.
    */
    TemporalQuery from_params(const std::unordered_map<std::string, std::string>& params);
    /**
     * @brief Parse a "+""-joined timeframe (e.g., "before+after") into a bitmask.
     * @param view Input string.
     * @return Combined @ref Timeframe flags.
     * @throws std::invalid_argument on unrecognized tokens.
    */
    Timeframe string_to_timeframe(std::string_view view);
    /**
     * @brief Convert a @ref Timeframe bitmask back to string (order: now,before,after).
     * @param frame Flags to stringify.
     * @return Joined tokens (e.g., "before+after"). Empty if NONE.
    */
    std::string timeframe_to_string(Timeframe frame);
    /**
     * @brief Serialize to compact JSON (unknown keys inlined alongside known keys).
     * @param q Source query.
     * @return JSON string.
     *
     * @note `extras` entries are written as top-level keys (stringified).
     * @note `filters` are encoded as a CSV string "k1,k2,...".
    */
    std::string serialize_temporal_query(const TemporalQuery& q);
    /**
     * @brief Deserialize from JSON, query-string, or path-like input.
     * @param json_string Either JSON (starts with '{' or '['), "k=v&...", or a path-like form.
     * @return Parsed query object.
     * @throws nlohmann::json::parse_error on invalid JSON.
     * @throws std::invalid_argument on bad timeframe tokens (non-JSON forms).
    */
    TemporalQuery deserialize_temporal_query(std::string_view json_string);
    /**
     * @brief Parse filters CSV into vectors of @ref Filter (values set empty).
     * @param csv Comma-separated keys.
     * @return Vector of filters.
    */
    std::vector<Filter> parse_filters(std::string_view csv);
    /**
     * @brief Join filters back into a string (keys only).
     * @param filters Vector of filters.
     * @return "k1|k2|k3".
    */
    std::string pack_filters(const std::vector<Filter>& filters);
     /**
     * @brief Parse parameters from a path-like string (heuristics for legacy forms).
     * @param s Example: "/SNDS/car2/getByID:car2" or "/SNDS/Car/getByID:car2".
     * @return Decoded map (e.g., {"id":"car2"}).
     *
     * @note Heuristic: tries to extract id/type from "getByID" / "getByIDForType".
    */
    std::unordered_map<std::string,std::string> params_from_pathlike(std::string_view s);
    /**
     * @brief Parse a query-string form (k=v&k2=v2), URL-decoding keys/values.
     * @param qs Raw query-string (without leading '?').
     * @return Map of decoded key→value pairs.
    */
    std::unordered_map<std::string,std::string> params_from_querystring(std::string_view qs);
    /// Format a TemporalQuery as a path of the form
    ///   /[type]/[id]/params(date="…"&count="…"&timeframe="…")
    /// If q.type is empty, the leading “/type” is omitted.
    /**
     * @brief Format a TemporalQuery as a slash-separated path.
     *
     * @details
     * Output shape:
     *   /[type]/[id]/date=.../timeframe=.../count=.../[extras...]/filters=...
     * Entries are URL-encoded. Omitted fields are skipped.
     *
     * @param q                 Source.
     * @param include_timeframe Include timeframe component.
     * @param include_count     Include count component.
     * @param include_extras    Include extras components.
     * @param include_filters   Include filters CSV component.
     * @return Slash-separated path string (no leading slash).
    */
    std::string to_path(
        const TemporalQuery& q, 
        const bool include_timeframe=true, 
        const bool include_count=true, 
        const bool include_extras=true,
        const bool include_filters=true
    );
    /**
     * @brief Hasher for @ref Filter (stable across runs for identical content).
    */
    struct FilterHasher 
    {
        size_t operator()(const Filter& f) const noexcept 
        {
            size_t h1 = std::hash<std::string>()(f.key);
            size_t h2 = std::hash<std::string>()(f.value);
            hash_combine(h1, h2);
            return h1;
        }
    };
    /**
     * @brief Hasher for @ref TemporalQuery (stable mixing over all fields).
    */
    struct TemporalQueryHasher 
    {
        size_t operator()(const TemporalQuery& q) const noexcept 
        {
            size_t seed = 0;

            // enum
            hash_combine(seed, std::hash<uint8_t>()(static_cast<uint8_t>(q.frame)));

            // strings
            hash_combine(seed, std::hash<std::string>()(q.date));
            hash_combine(seed, std::hash<std::string>()(q.id));
            hash_combine(seed, std::hash<std::string>()(q.type));
            hash_combine(seed, std::hash<std::string>()(q.count));

            // extras (stable order)
            std::vector<std::pair<std::string,std::string>> items(q.extras.begin(), q.extras.end());
            std::sort(items.begin(), items.end());
            for (auto const& p : items) 
            {
                hash_combine(seed, std::hash<std::string>()(p.first));
                hash_combine(seed, std::hash<std::string>()(p.second));
            }

            // filters (keep order)
            for (auto const& f : q.filters) 
            {
                hash_combine(seed, FilterHasher{}(f));
            }
            return seed;
        }
    };
    /**
     * @brief Convenience: stringify the hash of a @ref TemporalQuery.
     * @param q Source.
     * @return std::to_string of @ref TemporalQueryHasher result.
    */
    inline std::string temporal_query_hash_string(TemporalQuery const &q) noexcept 
    {
        return std::to_string(TemporalQueryHasher{}(q));
    };

};
//------------------------------------------------------------------------------
// Inject hash specializations into std::
//------------------------------------------------------------------------------

/**
 * @brief std::hash specialization for @ref temporal_query::Filter.
*/
namespace std 
{
    template<>
    struct hash<temporal_query::Filter> : temporal_query::FilterHasher {};
    /**
     * @brief std::hash specialization for @ref temporal_query::TemporalQuery.
    */
    template<>
    struct hash<temporal_query::TemporalQuery> : temporal_query::TemporalQueryHasher {};
};
