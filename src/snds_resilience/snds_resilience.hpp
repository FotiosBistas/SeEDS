#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>  
#include <spdlog/spdlog.h>


namespace snds_resilience{

    std::string canonical_dump(const nlohmann::json& j);
    std::string canonical_hash(const nlohmann::json& j);

    struct Meta 
    {
        uint64_t     version{};
        std::string  hash;

        nlohmann::json to_json() const 
        {
            return nlohmann::json
            {
                {"kind",    "meta"},
                {"version", version},
                {"hash",    hash}
            };
        }
    };

    struct Snapshot 
    {
        uint64_t       version{};
        std::string    hash;
        nlohmann::json state;

        nlohmann::json to_json() const 
        {
            return nlohmann::json
            {
                {"kind",    "snapshot"},
                {"version", version},
                {"hash",    hash},
                {"state",   state}
            };
        }
    };

    struct Patch 
    {
        uint64_t     base_version{};
        uint64_t     target_version{};
        std::string  target_hash;
        nlohmann::json ops;
        nlohmann::json to_json() const 
        {
            return nlohmann::json
            {
                {"kind",           "patch"},
                {"base_version",   base_version},
                {"target_version", target_version},
                {"target_hash",    target_hash},
                {"ops",            ops}
            };
        }
    };

    class Store 
    {
        public:
        using LoggerPointer = std::shared_ptr<spdlog::logger>;

        explicit Store(LoggerPointer logger = spdlog::default_logger());

        inline void set_history_keep(size_t keep)
        { 
            keep_ = keep ? keep : 1; 
        }

        inline size_t get_history_keep() const 
        { 
            return keep_; 
        }

        // Mutate canonical state. The functor gets a modifiable JSON.
        // After mutation we bump version, recompute hash, and snapshot history.
        void mutate(std::function<void(nlohmann::json&)> fn);

        // Direct replacement (if you already built the whole object)
        void assign(nlohmann::json new_state);

        // Read-only accessors
        Meta get_meta() const
        {
            return Meta{version_, hash_};
        };

        Snapshot get_snapshot() const
        {
            return Snapshot{version_, hash_, state_};
        };

        // Try to build a patch from base_version. If  base_version not in history,
        // returns std::nullopt (caller should send a full snapshot instead).
        std::optional<Patch> make_patch_from(uint64_t base_version) const;

        private:

            nlohmann::json  state_;
            uint64_t        version_   = 0;
            std::string     hash_;
            std::map<uint64_t, nlohmann::json> history_; // base version -> state at that version
            size_t          keep_  = 16;
            LoggerPointer   logger_;
    };


    class Mirror 
    {
        public:
            using LoggerPointer = std::shared_ptr<spdlog::logger>;

            explicit Mirror(LoggerPointer logger = spdlog::default_logger());

            // Local mirrored state
            uint64_t version() const
            {
                return version_;
            };

            const std::string& hash() const
            {
                return hash_;
            };

            nlohmann::json state() const
            {
                return state_;
            };

            // Apply a snapshot or patch. Returns true if the local
            // state now matches the advertised target (version/hash).
            bool apply_snapshot(const Snapshot& snap);
            bool apply_patch(const Patch& p);

        private:

            nlohmann::json     state_;
            uint64_t           version_   = 0;
            std::string        hash_;
            LoggerPointer      logger_;
    };

}