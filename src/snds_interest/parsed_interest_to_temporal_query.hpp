#pragma once
#include "snds_utils.h"
#include "temporal_query.hpp" 
#include "parsed_interest_name.hpp"
/**
 * @brief Construct a TemporalQuery object from a parsed NDN Interest.
 *
 * @details
 * This function maps fields from a @ref parsed_interest_name::ParsedInterestName
 * into a @ref temporal_query::TemporalQuery. The mapping rules are:
 *  - `pi.name` → used as the query's ID (`q.id`).
 *  - Each element of `pi.params` is treated as a raw `"key=value"` token
 *    (percent-decoded before use).
 *    - `"date=<val>"` → sets `q.date`.
 *    - `"timeframe=<val>"` → parsed with @ref temporal_query::string_to_timeframe
 *      and stored in `q.frame`.
 *    - `"count=<val>"` → sets `q.count`.
 *    - `"filters=<val>"` → parsed with @ref temporal_query::parse_filters
 *      and stored in `q.filters`.
 *    - Any other parameter or missing `'='` is stored in `q.extras[key] = value`.
 *
 * @param pi Parsed Interest name with prefix, id, and parameter tokens.
 * @return A populated @ref temporal_query::TemporalQuery.
 *
 * @note The current implementation assumes:
 *   - `pi.prefix` denotes the type/service component, but it is not
 *     directly used (see commented code).
 *   - `pi.name` carries the ID to query.
 *
 * @warning This mapping is based on conventions. Adjust if your naming scheme differs.
*/
inline temporal_query::TemporalQuery temporal_query_from_interest(const parsed_interest_name::ParsedInterestName& pi)
{
    temporal_query::TemporalQuery q;
    // we’ll assume:
    //   pi.prefix  holds the “type” (or service) component
    //   pi.name holds the “id”
    // adjust as your naming scheme requires
    // q.type = pi.prefix;
    q.id   = pi.name;

    for (auto const& raw_param: pi.params) 
    {
        // decode any %-escapes
        std::string param = url_decode(raw_param);

        // split at the first '='
        auto eq = param.find('=');
        if (eq == std::string::npos) 
        {
            // no “key=value”? shove into extras
            q.extras[param] = "";
            continue;
        }
        std::string key = param.substr(0, eq);
        std::string val = param.substr(eq + 1);

        if (key == "date") 
        {
            q.date = val;
        }
        else if (key == "timeframe") 
        {
            q.frame = temporal_query::string_to_timeframe(val);
        }
        else if (key == "count") 
        {
            q.count = val;
        }
        else if (key == "filters") 
        {
            q.filters = temporal_query::parse_filters(val);
        }
    }

    return q;
}
