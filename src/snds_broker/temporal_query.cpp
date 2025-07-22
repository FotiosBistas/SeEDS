#include "temporal_query.h"

// Parse a "+"-delimited string (e.g., "before+after")
//    into a single Timeframe flag bitmask.
Timeframe string_to_timeframe(std::string_view view)
{
    constexpr const char *delimiter = "+";
    Timeframe result = Timeframe::NONE; // start with no flags
    size_t start = 0;

    // Loop until we've processed the entire input string
    while (start < view.size())
    {
        // Find end of current token
        auto pos = view.find(delimiter, start);
        // Extract substring from start up to '+' or end of string
        auto part = view.substr(start,
                                (pos == std::string_view::npos)
                                    ? view.size() - start
                                    : pos - start);

        // Map token to the corresponding enum flag
        if (part == "now")
            result = result | Timeframe::NOW;
        else if (part == "before")
            result = result | Timeframe::BEFORE;
        else if (part == "after")
            result = result | Timeframe::AFTER;
        else
        {
            throw std::invalid_argument(std::string("Invalid timeframe token: ") + std::string(part));
        }

        // If no more '+' found, we're done
        if (pos == std::string_view::npos)
            break;
        // Otherwise, skip past the '+' and parse next token
        start = pos + 1;
    }

    return result;
}

/// Convert a Timeframe bitmask back into a "+"-delimited string.
std::string timeframe_to_string(Timeframe frame)
{
    constexpr const char *delimiter = "+";
    std::string out;
    bool first = true; // track whether to prepend '+'

    // Helper lambda: if the flag is set, append its string
    auto add = [&](std::string_view token, Timeframe flag)
    {
        if ((frame & flag) == flag)
        {
            if (!first)
                out += delimiter; // not the first token? prepend '+'
            out += token;
            first = false;
        }
    };

    // Check each possible timeframe in a fixed order
    add("now", Timeframe::NOW);
    add("before", Timeframe::BEFORE);
    add("after", Timeframe::AFTER);

    return out; // e.g., "before+after" or "now"
}

/// Turn a CSV of filter-keys into a vector<Filter> (value left empty)
std::vector<Filter> parse_filters(std::string_view csv) 
{
    std::vector<Filter> out;
    std::string temp{csv};
    std::stringstream ss(temp);
    std::string item;
    while (std::getline(ss, item, ',')) 
    {
        if (!item.empty()) 
        {
            out.push_back(Filter{item, ""});
        }
    }
    return out;
}

/// Join filters back into a CSV of keys
std::string filters_to_csv(const std::vector<Filter>& filters) 
{
    std::string out;
    bool first = true;
    for (auto const &f : filters) 
    {
        if (!first) out += ',';
        out += f.key;
        first = false;
    }
    return out;
}

std::string to_path(
    const TemporalQuery &q,
    const bool include_timeframe,
    const bool include_count,
    const bool include_extras,
    const bool include_filters)
{
    std::ostringstream oss;

    // optional /type
    if (!q.type.empty())
    {
        oss << '/' << q.type;
    }

    // always /id
    oss << '/' << q.id;

    // date or version
    // TODO note the date will be an md5 digest
    oss << '/' << url_encode("date=") << url_encode(q.date);

    if (include_timeframe && q.frame != Timeframe::NONE)
    {
        // timeframe (convert bitmask to e.g. "before+after")
        oss << '/' << url_encode("timeframe=")
            << url_encode(timeframe_to_string(q.frame));
    }

    if (include_count && !q.count.empty())
    {
        // count
        oss << '/' << url_encode("count=") << url_encode(q.count);
    }

    if (!q.extras.empty() && include_extras)
    {
        // nest all unknown params under "extras"
        nlohmann::json extra_obj = nlohmann::json::object();
        for (auto const &[key, val] : q.extras)
        {
            oss << '/' << url_encode(key + "=") << val;
        }
    }

    if (include_extras && !q.filters.empty())
    {
        oss << '/'
            << url_encode("filters=")
            << url_encode(filters_to_csv(q.filters));
    }

    return oss.str();
}

std::string serialize_temporal_query(const TemporalQuery &q)
{
    nlohmann::json j;
    j["timeframe"] = timeframe_to_string(q.frame);
    j["date"] = q.date;
    j["id"] = q.id;
    j["type"] = q.type;
    j["count"] = q.count;

    if (!q.extras.empty())
    {
        // nest all unknown params under "extras"
        nlohmann::json extra_obj = nlohmann::json::object();
        for (auto const &[key, val] : q.extras)
        {
            j[key] = val;
        }
    }

    if (!q.filters.empty())
    {
        j["filters"] = filters_to_csv(q.filters);
    }
    return j.dump();
}

// Parse a JSON string back into a TemporalQuery struct.
TemporalQuery deserialize_temporal_query(std::string_view json_string)
{
    // Parse input into JSON object
    auto j = nlohmann::json::parse(json_string);

    // Validate required keys
    for (auto key : {"timeframe", "date", "id", "type", "count"})
    {
        if (!j.contains(key))
        {
            throw std::invalid_argument(
                std::string("Missing key in TemporalQuery JSON: ") + key);
        }
    }

    TemporalQuery q;
    // load filters CSV → vector<Filter>
    if (j.contains("filters"))
    {
        auto csv = j.at("filters").get<std::string>();
        q.filters = parse_filters(csv);
    }
    // Correct field name for timeframe
    q.frame = string_to_timeframe(j.at("timeframe").get<std::string>());
    q.date = j.at("date").get<std::string>();
    q.id = j.at("id").get<std::string>();
    q.type = j.at("type").get<std::string>();
    q.count = j.at("count").get<std::string>();
    // Capture any additional fields as "extras"
    for (auto &item : j.items())
    {
        const auto &key = item.key();
        if (key == "timeframe" ||
            key == "date" ||
            key == "id" ||
            key == "type" ||
            key == "count" ||
            key == "filters")
        {
            continue;
        }
        // Store the value as string (or adapt if you need other types)
        q.extras[key] = item.value().get<std::string>();
    }
    return q;
}
