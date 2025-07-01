#include "temporal_query.h"


Timeframe string_to_timeframe(std::string_view view) {
    if (view == "now") return Timeframe::NOW;
    if (view == "before") return Timeframe::BEFORE;
    if (view == "after") return Timeframe::AFTER;
    throw std::invalid_argument("Invalid timeframe");
}

std::string timeframe_to_string(Timeframe frame) {
    switch (frame) {
        case Timeframe::NOW:    return "now";
        case Timeframe::BEFORE: return "before";
        case Timeframe::AFTER:  return "after";
    }
    throw std::invalid_argument("Invalid Timeframe enum value");
}

std::string serialize_temporal_query(const TemporalQuery& q) {
    nlohmann::json j;
    j["frame"] = timeframe_to_string(q.frame);
    j["date"]  = q.date;
    j["id"]    = q.id;
    j["type"]  = q.type;
    j["count"] = q.count;
    return j.dump();
}

TemporalQuery deserialize_temporal_query(std::string_view json_string) {
    auto j = nlohmann::json::parse(json_string);

    for (auto key : {"frame","date","id","type","count"}) {
        if (!j.contains(key))
            throw std::invalid_argument(std::string("Missing key in TemporalQuery JSON: ") + key);
    }

    TemporalQuery q;
    q.frame = string_to_timeframe(j.at("frame").get<std::string>());
    q.date  = j.at("date" ).get<std::string>();
    q.id    = j.at("id"   ).get<std::string>();
    q.type  = j.at("type" ).get<std::string>();
    q.count = j.at("count").get<std::string>();
    return q;
}