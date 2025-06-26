#include <string>
#include <stdexcept>

enum class Timeframe{
    NOW,
    BEFORE,
    AFTER
};

Timeframe string_to_timeframe(std::string_view input);


