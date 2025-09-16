#include "temporal_query.hpp"

// Parse a "+"-delimited string (e.g., "before+after")
//    into a single Timeframe flag bitmask.
temporal_query::Timeframe temporal_query::string_to_timeframe(std::string_view view)
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
std::string temporal_query::timeframe_to_string(temporal_query::Timeframe frame)
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
std::vector<temporal_query::Filter> temporal_query::parse_filters(std::string_view csv) 
{
    std::vector<Filter> out{};
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

temporal_query::TemporalQuery temporal_query::from_params(const std::unordered_map<std::string, std::string>& params) 
{
    TemporalQuery q{};

    static const std::unordered_map<std::string, std::function<void(TemporalQuery&, const std::string&)>> map 
    {
        { "id",        [](auto& q, const std::string& v){ q.id = v; } },
        { "date",      [](auto& q, const std::string& v){ q.date = v; } },
        { "count",     [](auto& q, const std::string& v){ q.count = v; } },
        { "type",      [](auto& q, const std::string& v){ q.type = v; } },
        { "timeframe", [](auto& q, const std::string& v){ q.frame = string_to_timeframe(v); } },
        { "filters",   [](auto& q, const std::string& v){ q.filters = parse_filters(v); } },
        // When adding a new field, add a row here. Unknowns fall into extras automatically.
    };

    for (const auto& [k, v] : params) 
    {
        if (auto it = map.find(k); it != map.end()) it->second(q, v);
        else q.extras[k] = v;
    }

    return q;
}

/// Join filters back into a CSV of keys
std::string temporal_query::filters_to_csv(const std::vector<temporal_query::Filter>& filters) 
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

std::string temporal_query::to_path
(
    const temporal_query::TemporalQuery &q,
    const bool include_timeframe,
    const bool include_count,
    const bool include_extras,
    const bool include_filters
) {
    std::ostringstream oss;
    bool first = true;

    auto append = [&](std::string const& piece) 
    {
        if (!first) 
        {
            oss << '/';
        }
        oss << piece;
        first = false;
    };

    if (!q.type.empty()) 
    {
        append(q.type);
    }

    if (!q.id.empty()) 
    {
        append(q.id);
    }

    if (!q.date.empty()) 
    {
        append(url_encode("date=") + url_encode(q.date));
    }

    if (include_timeframe && q.frame != Timeframe::NONE) 
    {
        append(url_encode("timeframe=") + url_encode(timeframe_to_string(q.frame)));
    }

    if (include_count && !q.count.empty()) 
    {
        append(url_encode("count=") + url_encode(q.count));
    }

    if (include_extras) 
    {
        for (auto const& [key, val] : q.extras) 
        {
            append(url_encode(key + "=") + url_encode(val));
        }
    }

    if (include_filters && !q.filters.empty()) 
    {
        append(url_encode("filters=") + url_encode(filters_to_csv(q.filters)));
    }

    return oss.str();
}

std::vector<std::string> temporal_query::to_fields
(
    const temporal_query::TemporalQuery& q,
    bool include_timeframe,
    bool include_count,
    bool include_extras,
    bool include_filters,
    bool url_encode
)
{
    std::vector<std::string> fields;
    fields.reserve(6 + q.extras.size() + (include_filters ? 1 : 0));

    auto enc = [&](std::string_view s) -> std::string 
    {
        return url_encode ? ::url_encode(std::string{s}) : std::string{s};
    };

    auto kv = [&](std::string_view k, const std::string& v) -> std::string 
    {
        // will yield "key=" when v is empty
        return enc(enc(k) + "=" + enc(v));
    };

    fields.emplace_back(kv("id",    q.id));
    fields.emplace_back(kv("type",  q.type));
    fields.emplace_back(kv("date",  q.date));

    if (include_count) fields.emplace_back(kv("count", q.count));
    if (include_timeframe) fields.emplace_back(kv("timeframe", timeframe_to_string(q.frame)));
    if (include_filters) 
    {
        const auto csv = filters_to_csv(q.filters);
        fields.emplace_back(kv("filters", csv));
    }

    if (include_extras) 
    {
        std::vector<std::pair<std::string,std::string>> items(q.extras.begin(), q.extras.end());
        std::sort(items.begin(), items.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
        for (auto& [k, v] : items) fields.emplace_back(kv(k, v));
    }

    return fields;
}



std::string temporal_query::serialize_temporal_query(const temporal_query::TemporalQuery &q)
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

std::string temporal_query::serialize_kv(const temporal_query::TemporalQuery& q)
{
    std::string out;

    auto append = [&](std::string_view k, std::string_view v)
    {
        if (out.size()) out.push_back('&');
        out.append(k);
        out.push_back('=');
        out.append(std::string(v));
    };

    append("timeframe", timeframe_to_string(q.frame));
    append("date",      q.date);
    append("id",        q.id);
    append("type",      q.type);
    append("count",     q.count);

    if (!q.extras.empty())
    {
        std::vector<std::pair<std::string, std::string>> extras(q.extras.begin(), q.extras.end());
        std::sort(extras.begin(), extras.end(), [](auto& a, auto& b){ return a.first < b.first; });
        for (auto& [k, v] : extras) append(k, v);
    }

    if (!q.filters.empty()) append("filters", filters_to_csv(q.filters));

    return out;
}


temporal_query::TemporalQuery temporal_query::deserialize_temporal_query(std::string_view input)
{
    const std::string s = trim_copy(input);

    // Try JSON (only if it *looks* like JSON)
    if (!s.empty() && (s.front() == '{' || s.front() == '[')) 
    {
        auto j = nlohmann::json::parse(s);
        TemporalQuery q;

        if (j.contains("filters")) q.filters = parse_filters(j.at("filters").get<std::string>());
        if (j.contains("timeframe")) q.frame = string_to_timeframe(j.at("timeframe").get<std::string>());
        if (j.contains("date"))  q.date  = j.at("date").get<std::string>();
        if (j.contains("id"))    q.id    = j.at("id").get<std::string>();
        if (j.contains("type"))  q.type  = j.at("type").get<std::string>();
        if (j.contains("count")) q.count = j.at("count").get<std::string>();

        // extras
        for (auto &item : j.items()) 
        {
            const auto &key = item.key();
            if (key=="timeframe"||key=="date"||key=="id"||key=="type"||key=="count"||key=="filters") continue;
            q.extras[key] = item.value().dump(); // preserve as string
        }
        return q;
    }

    // Try query-string form: "id=car2&type=getByID&timeframe=now"
    if (s.find('=') != std::string::npos) 
    {
        auto params = params_from_querystring(s);
        return from_params(params);
    }

    // Fallback: path-like strings
    auto params = params_from_pathlike(s);
    return from_params(params);
}

std::unordered_map<std::string,std::string> temporal_query::params_from_querystring(std::string_view qs) 
{
    std::unordered_map<std::string,std::string> out;
    size_t pos = 0;
    while (pos < qs.size()) 
    {
        auto amp = qs.find('&', pos);
        auto part = qs.substr(pos, amp==std::string_view::npos ? qs.size()-pos : amp-pos);
        auto eq = part.find('=');
        std::string k = eq==std::string_view::npos ? std::string(part) : std::string(part.substr(0,eq));
        std::string v = eq==std::string_view::npos ? "" : std::string(part.substr(eq+1));
        out[url_decode(k)] = url_decode(v);
        if (amp == std::string_view::npos) break;
        pos = amp + 1;
    }
    return out;
}

// Accept forms like:
//   "/SNDS/car2/getByID:car2"
//   "/SNDS/Car/getByID:car2"
std::unordered_map<std::string,std::string> temporal_query::params_from_pathlike(std::string_view s)
{
    std::unordered_map<std::string,std::string> out;
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start < s.size()) 
    {
        auto slash = s.find('/', start);
        auto tok = s.substr(start, slash==std::string_view::npos ? s.size()-start : slash-start);
        if (!tok.empty()) parts.push_back(tok);
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }

    for (auto p : parts) 
    {
        auto colon = p.find(':');
        if (colon == std::string_view::npos) continue;
        auto left  = p.substr(0, colon);
        auto right = p.substr(colon+1);

        if (left == "getByID") 
        {
            out["id"]   = std::string(right);
        } 
        else if (left == "getByIDForType") 
        {
            // expect "<id>&<type>"
            auto amp = right.find('&');
            if (amp != std::string_view::npos) 
            {
                out["id"]   = std::string(right.substr(0, amp));
                out["type"] = std::string(right.substr(amp+1));
            } 
            else 
            {
                // fallback: if no '&', treat whole thing as id
                out["id"] = std::string(right);
            }
        }
    }

    // heuristic fallback if still no id
    if (!out.count("id")) 
    {
        for (size_t i=0;i+1<parts.size();++i) 
        {
            if (parts[i] == "SNDS" && i+1 < parts.size()) 
            {
                out["id"] = std::string(parts[i+1]);
                break;
            }
        }
    }

    return out;
}