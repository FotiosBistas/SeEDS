#pragma once

#include <string>
#include <stdexcept>
#include <sstream>
#include <nlohmann/json.hpp>
#include "snds_utils.h"



/// Timeframe is a bitmask enum so multiple values
///    like NOW and AFTER can be OR'd together into one variable.
enum class Timeframe : uint8_t {
    NONE   = 0,        // no timeframe selected
    NOW    = 1 << 0,   // binary 0001
    BEFORE = 1 << 1,   // binary 0010
    AFTER  = 1 << 2,   // binary 0100
};

struct TemporalQuery{
    Timeframe         frame;
    std::string  date;
    std::string  id;
    std::string  type;
    std::string  count;
};

Timeframe string_to_timeframe(std::string_view view);
std::string timeframe_to_string(Timeframe frame);
std::string serialize_temporal_query(const TemporalQuery& q);
TemporalQuery deserialize_temporal_query(std::string_view json_string);
/// Format a TemporalQuery as a path of the form
///   /[type]/[id]/params(date="…"&count="…"&timeframe="…")
/// If q.type is empty, the leading “/type” is omitted.
std::string to_path(const TemporalQuery& q, const bool include_timeframe=true, const bool include_count=true);



