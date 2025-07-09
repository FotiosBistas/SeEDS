#pragma once

#include "snds_utils.h"
#include "temporal_query.h" 
#include "parsed_interest.h"

inline TemporalQuery makeTemporalQuery(const ParsedInterest& pi)
{
    TemporalQuery q;
    // we’ll assume:
    //   pi.prefix  holds the “type” (or service) component
    //   pi.command holds the “id”
    // adjust as your naming scheme requires
    q.type = pi.prefix;
    q.id   = pi.command;

    for (auto const& raw_param: pi.params) {
        // decode any %-escapes
        std::string param = url_decode(raw_param);

        // split at the first '='
        auto eq = param.find('=');
        if (eq == std::string::npos) {
            // no “key=value”? shove into extras
            q.extras[param] = "";
            continue;
        }
        std::string key = param.substr(0, eq);
        std::string val = param.substr(eq + 1);

        if (key == "date") {
            q.date = val;
        }
        else if (key == "timeframe") {
            q.frame = string_to_timeframe(val);
        }
        else if (key == "count") {
            q.count = val;
        }
        else if (key == "filters") {
            q.filters = parse_filters(val);
        }
        else {
            // anything else: unknown extra
            q.extras[key] = val;
        }
    }

    return q;
}
