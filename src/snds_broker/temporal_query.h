#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>



enum class Timeframe{
    NOW,
    BEFORE,
    AFTER
};

struct TemporalQuery{
    Timeframe         frame;
    std::string  date;
    std::string  id;
    std::string  type;
    std::string  count;
};

Timeframe string_to_timeframe(std::string_view input);
std::string timeframe_to_string(Timeframe frame);
std::string serialize_temporal_query(const TemporalQuery& q);
TemporalQuery deserialize_temporal_query(std::string_view json_string);


