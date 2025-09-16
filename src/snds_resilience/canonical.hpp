#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace canonical {

    // Turn any set-like container into a sorted vector
    template <class SetLike>
    auto sorted_vector(const SetLike& s) 
    {
        using T = typename SetLike::value_type;
        std::vector<T> v(s.begin(), s.end());
        std::sort(v.begin(), v.end());
        return v;
    }

    // Map<string, SetLike> -> JSON object with sorted keys and sorted arrays
    template <class MapStringToSet>
    nlohmann::json map_of_sets(const MapStringToSet& m) 
    {
        std::vector<std::string> keys;
        keys.reserve(m.size());
        for (const auto& kv : m) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());

        nlohmann::json out = nlohmann::json::object();
        for (const auto& k : keys) {
            const auto& set = m.at(k);
            out[k] = sorted_vector(set);
        }
        return out;
    }

    // SetLike<string> -> JSON array(sorted)
    template <class SetLike>
    nlohmann::json set_to_json(const SetLike& s) 
    {
        return nlohmann::json(sorted_vector(s));
    }

}