#include "timeframes.h"


Timeframe string_to_timeframe(std::string_view view) {
    if (view == "now") return Timeframe::NOW;
    if (view == "before") return Timeframe::BEFORE;
    if (view == "after") return Timeframe::AFTER;
    throw std::invalid_argument("Invalid timeframe");
}