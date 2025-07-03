#include "temporal_query.h"

// Enable bitwise OR and AND operations on Timeframe values.
//    This makes combining and testing flags easy.
inline Timeframe operator|(Timeframe a, Timeframe b) {
    // Combine flag bits by casting to integer, OR, then cast back.
    return static_cast<Timeframe>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline Timeframe operator&(Timeframe a, Timeframe b) {
    // Test common bits by AND-ing underlying integer values.
    return static_cast<Timeframe>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}


// Parse a "+"-delimited string (e.g., "before+after")
//    into a single Timeframe flag bitmask.
Timeframe string_to_timeframe(std::string_view view) {
    constexpr const char* delimiter = "+";
    Timeframe result = Timeframe::NONE;  // start with no flags
    size_t start = 0;

    // Loop until we've processed the entire input string
    while (start < view.size()) {
        // Find end of current token
        auto pos = view.find(delimiter, start);
        // Extract substring from start up to '+' or end of string
        auto part = view.substr(start,
                                 (pos == std::string_view::npos)
                                   ? view.size() - start
                                   : pos - start);

        // Map token to the corresponding enum flag
        if (part == "now")         result = result | Timeframe::NOW;
        else if (part == "before") result = result | Timeframe::BEFORE;
        else if (part == "after")  result = result | Timeframe::AFTER;
        else{
            throw std::invalid_argument(std::string("Invalid timeframe token: ") + std::string(part));
        }

        // If no more '+' found, we're done
        if (pos == std::string_view::npos) break;
        // Otherwise, skip past the '+' and parse next token
        start = pos + 1;
    }

    return result;
}

/// Convert a Timeframe bitmask back into a "+"-delimited string.
std::string timeframe_to_string(Timeframe frame) {
    constexpr const char* delimiter = "+";
    std::string out;
    bool first = true;  // track whether to prepend '+'

    // Helper lambda: if the flag is set, append its string
    auto add = [&](std::string_view token, Timeframe flag) {
        if ((frame & flag) == flag) {
            if (!first) out += delimiter; // not the first token? prepend '+'
            out += token;
            first = false;
        }
    };

    // Check each possible timeframe in a fixed order
    add("now",    Timeframe::NOW);
    add("before", Timeframe::BEFORE);
    add("after",  Timeframe::AFTER);

    return out;  // e.g., "before+after" or "now"
}

std::string to_path(const TemporalQuery& q, const bool include_timeframe, const bool include_count){
    std::ostringstream oss;

    // optional /type
    if (!q.type.empty()) {
        oss << '/' << q.type;
    }

    // always /id
    oss << '/' << q.id;

    // date or version
    // TODO note the date will be an md5 digest
    oss << '/' << url_encode("date=") << url_encode(q.date);

    if(include_timeframe && q.frame != Timeframe::NONE){
        // timeframe (convert bitmask to e.g. "before+after")
        oss << '/' << url_encode("timeframe=")
            << url_encode(timeframe_to_string(q.frame));
    }

    if(include_count && !q.count.empty()){
        // count
        oss << '/' << url_encode("count=") << url_encode(q.count);
    }

    return oss.str();
}

std::string serialize_temporal_query(const TemporalQuery& q) {
    nlohmann::json j;
    j["timeframe"] = timeframe_to_string(q.frame);
    j["date"]      = q.date;
    j["id"]        = q.id;
    j["type"]      = q.type;
    j["count"]     = q.count;
    return j.dump();
}

// Parse a JSON string back into a TemporalQuery struct.
TemporalQuery deserialize_temporal_query(std::string_view json_string) {
    // Parse input into JSON object
    auto j = nlohmann::json::parse(json_string);

    // Validate required keys
    for (auto key : {"timeframe", "date", "id", "type", "count"}) {
        if (!j.contains(key)) {
            throw std::invalid_argument(
              std::string("Missing key in TemporalQuery JSON: ") + key
            );
        }
    }

    TemporalQuery q;
    // Correct field name for timeframe
    q.frame = string_to_timeframe(j.at("timeframe").get<std::string>());
    q.date  = j.at("date").get<std::string>();
    q.id    = j.at("id").get<std::string>();
    q.type  = j.at("type").get<std::string>();
    q.count = j.at("count").get<std::string>();
    return q;
}
