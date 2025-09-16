#pragma once

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

    inline void hash_combine(std::size_t &seed, std::size_t h) 
    {
        // from boost::hash_combine
        seed ^= h + 0x9e3779b97f4a7c15ULL + (seed<<6) + (seed>>2);
    }

    struct Filter 
    {

        std::string key, 
        value;

        bool operator==(Filter const &o) const noexcept 
        {
            return key == o.key
                && value == o.value;
        }
    };


    /// Timeframe is a bitmask enum so multiple values
    ///    like NOW and AFTER can be OR'd together into one variable.
    enum class Timeframe : uint8_t 
    {
        NONE   = 0,        // no timeframe selected
        NOW    = 1 << 0,   // binary 0001
        BEFORE = 1 << 1,   // binary 0010
        AFTER  = 1 << 2,   // binary 0100
    };

    // Enable bitwise OR and AND operations on Timeframe values.
    //    This makes combining and testing flags easy.
    inline Timeframe operator|(Timeframe a, Timeframe b) 
    {
        // Combine flag bits by casting to integer, OR, then cast back.
        return static_cast<Timeframe>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    inline Timeframe operator&(Timeframe a, Timeframe b) 
    {
        // Test common bits by AND-ing underlying integer values.
        return static_cast<Timeframe>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    struct TemporalQuery
    {
        Timeframe         frame;
        std::string  date;
        std::string  id;
        std::string  type;
        std::string  count;
        std::unordered_map<std::string,std::string> extras;
        std::vector<Filter> filters;

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

    std::vector<std::string> to_fields
    (
        const temporal_query::TemporalQuery& q,
        bool include_timeframe = true,
        bool include_count     = true,
        bool include_extras    = true,
        bool include_filters   = true,
        bool url_encode        = true
    );

    std::string serialize_kv(const TemporalQuery& q);


    TemporalQuery from_params(const std::unordered_map<std::string, std::string>& params);
    Timeframe string_to_timeframe(std::string_view view);
    std::string timeframe_to_string(Timeframe frame);
    std::string serialize_temporal_query(const TemporalQuery& q);
    TemporalQuery deserialize_temporal_query(std::string_view json_string);
    std::vector<Filter> parse_filters(std::string_view csv);
    std::string filters_to_csv(const std::vector<Filter>& filters);
    std::unordered_map<std::string,std::string> params_from_pathlike(std::string_view s);
    std::unordered_map<std::string,std::string> params_from_querystring(std::string_view qs);
    /// Format a TemporalQuery as a path of the form
    ///   /[type]/[id]/params(date="…"&count="…"&timeframe="…")
    /// If q.type is empty, the leading “/type” is omitted.
    std::string to_path(
        const TemporalQuery& q, 
        const bool include_timeframe=true, 
        const bool include_count=true, 
        const bool include_extras=true,
        const bool include_filters=true
    );

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

    inline std::string temporal_query_hash_string(TemporalQuery const &q) noexcept 
    {
        return std::to_string(TemporalQueryHasher{}(q));
    };


};

//------------------------------------------------------------------------------
// Inject hash specializations into std::
//------------------------------------------------------------------------------
namespace std 
{
    template<>
    struct hash<temporal_query::Filter> : temporal_query::FilterHasher {};

    template<>
    struct hash<temporal_query::TemporalQuery> : temporal_query::TemporalQueryHasher {};
};
