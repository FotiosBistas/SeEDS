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
inline temporal_query::TemporalQuery
temporal_query_from_interest(const parsed_interest_name::ParsedInterestName& pi)
{
    temporal_query::TemporalQuery q;

    // Default/fallbacks based on your naming:
    //   /SNDS/<TYPE>/temporalQuery:id=...&type=...&...
    // Treat the path component as TYPE by default.
    q.type = pi.name;   // e.g., "CAR"
    // q.id stays empty until we parse "id="

    for (const auto& raw_param : pi.params)
    {
        // decode %-escapes (so timeframe "now%2Bbefore%2Bafter" → "now+before+after")
        std::string param = url_decode(raw_param);

        // split once on '='
        const auto eq = param.find('=');
        const std::string key = (eq == std::string::npos) ? param : param.substr(0, eq);
        const std::string val = (eq == std::string::npos) ? ""    : param.substr(eq + 1);

        if (key == "id") 
        {
            q.id = val;                     // <- primary source of ID (e.g., "car001")
        } 
        else if (key == "type") 
        {
            q.type = val;                   // <- explicit type overrides default (pi.name)
        } 
        else if (key == "date") 
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
        else 
        {
            // Preserve unknowns for debugging/forward-compat
            q.extras[key] = val;
        }
    }

    // If an Interest omitted "type=", we already set q.type = pi.name above.
    // If an Interest omitted "id=", leave q.id empty (or set a fallback here if you prefer).

    return q;
}
