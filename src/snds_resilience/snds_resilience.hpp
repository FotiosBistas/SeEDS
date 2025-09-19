#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <cstdint>

#include <nlohmann/json.hpp>  
#include <spdlog/spdlog.h>

/// \namespace snds_resilience
/// \brief Lightweight versioned JSON store + mirror with canonical hashing and JSON Patch deltas.
namespace snds_resilience
{
    /** \brief Produce a deterministic, canonical JSON string.
     *
     *  Uses \c nlohmann::ordered_json to ensure a stable key order and emits a
     *  compact dump so the same logical object always serializes identically.
     *  \param j Source JSON object.
     *  \return Canonical, compact JSON string.
    */
    std::string canonical_dump(const nlohmann::json& j);
    /** \brief Compute a canonical content hash for a JSON object.
     *
     *  Hash is computed over \ref canonical_dump(j) via MD5 (for fast, stable
     *  fingerprints—not for cryptographic security).
     *  \param j Source JSON object.
     *  \return Lowercase hex MD5 of the canonical dump.
    */
    std::string canonical_hash(const nlohmann::json& j);
    /** \brief Minimal metadata describing a state head. */
    struct Meta 
    {
        /** \brief Monotonic state version (starts at 0). */
        uint64_t     version{};
        /** \brief Canonical content hash of the state at \ref version. */
        std::string  hash;

        /** \brief Serialize to a small self-describing JSON object. */
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

    /** \brief Full snapshot of the state (for re-sync or cold start). */
    struct Snapshot 
    {
        /** \brief Version of the included state. */
        uint64_t       version{};
        /** \brief Canonical hash for \ref state. */
        std::string    hash;
        /** \brief Entire canonical state object. */
        nlohmann::json state;
        /** \brief Serialize to a self-describing JSON envelope. */
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
    /** \brief Delta between two versions of the state.
     *
     *  \details \c ops is a JSON Patch (RFC 6902) produced by
     *  \c nlohmann::json::diff(base, target). Apply with \c state.patch(ops).
     */
    struct Patch 
    {
        /** \brief Base version the patch expects. */
        uint64_t     base_version{};
        /** \brief Version reached after applying the patch. */
        uint64_t     target_version{};
        /** \brief Canonical hash of the target state. */
        std::string  target_hash;
        /** \brief JSON Patch operations (RFC 6902). */
        nlohmann::json ops;

        /** \brief Serialize to a self-describing JSON envelope. */
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

    /** \brief Versioned, canonical JSON store with bounded history and patch generation.
     *
     *  \details The store maintains:
     *   - current \ref state_ (canonical JSON),
     *   - monotonic \ref version_,
     *   - canonical \ref hash_,
     *   - a bounded \ref history_ mapping version→state for patch creation.
     *
     *  Use \ref mutate to edit the state in-place or \ref assign to replace it.
     *  After each change, \ref version_ is incremented, \ref hash_ recomputed,
     *  and the new state is recorded in \ref history_ (trimmed to \ref keep_ entries).
    */
    class Store 
    {
        public:
        using LoggerPointer = std::shared_ptr<spdlog::logger>;

        /** \brief Construct an empty store (version=0, hash of "{}"). */
        explicit Store(LoggerPointer logger = spdlog::default_logger());
        /** \brief Set how many historical versions to keep (min 1). */
        inline void set_history_keep(size_t keep)
        { 
            keep_ = keep ? keep : 1; 
        }
        /** \brief Get the current history retention size. */
        inline size_t get_history_keep() const 
        { 
            return keep_; 
        }

        // Mutate canonical state. The functor gets a modifiable JSON.
        // After mutation we bump version, recompute hash, and snapshot history.
        /** \brief Mutate the canonical state via functor, bump version, recompute hash, snapshot history.
         *  \param fn Functor receiving a modifiable reference to the state JSON.
         *  \note History is trimmed to \ref keep_ after the update.
        */
        void mutate(std::function<void(nlohmann::json&)> fn);
        // Direct replacement (if you already built the whole object)
        /** \brief Replace the entire state object, bump version, recompute hash, snapshot history.
         *  \param new_state New canonical state to take ownership of.
        */
        void assign(nlohmann::json new_state);

        // Read-only accessors
        /** \brief Read-only metadata of the current head. */
        Meta get_meta() const
        {
            return Meta{version_, hash_};
        };
        /** \brief Read-only snapshot of the current head. */
        Snapshot get_snapshot() const
        {
            return Snapshot{version_, hash_, state_};
        };

        // Try to build a patch from base_version. If  base_version not in history,
        // returns std::nullopt (caller should send a full snapshot instead).
        /** \brief Build a patch from a known base version to the current head.
         *  \param base_version The version the consumer currently has.
         *  \return \ref Patch if the base version exists in \ref history_; \c std::nullopt otherwise.
         *  \note Callers should fall back to a full \ref Snapshot if \c std::nullopt is returned.
        */
        std::optional<Patch> make_patch_from(uint64_t base_version) const;

        private:
            /** \brief Current canonical state. */
            nlohmann::json  state_{};
            /** \brief Monotonic version counter. */
            uint64_t        version_   = 0;
            /** \brief Canonical hash of \ref state_. */
            std::string     hash_{};
            /** \brief Bounded history: version → state snapshot. */
            std::map<uint64_t, nlohmann::json> history_; // base version -> state at that version
            /** \brief Max history entries retained (default 16). */
            size_t          keep_  = 16;
            /** \brief Optional logger. */
            LoggerPointer   logger_;
    };
    /** \brief Follower-side mirror capable of applying snapshots and patches.
     *
     *  \details Maintains a local copy of \ref state_, along with its \ref version_
     *  and canonical \ref hash_. \ref apply_snapshot replaces local state, while
     *  \ref apply_patch applies JSON Patch ops provided the base version matches.
    */
    class Mirror 
    {
        public:
            using LoggerPointer = std::shared_ptr<spdlog::logger>;
            /** \brief Construct an empty mirror (version=0, hash of "{}"). */
            explicit Mirror(LoggerPointer logger = spdlog::default_logger());
            // Local mirrored state
            /** \brief Current local version. */
            uint64_t version() const
            {
                return version_;
            };
            /** \brief Current local canonical hash. */
            const std::string& hash() const
            {
                return hash_;
            };
            /** \brief Current local JSON state (by value). */
            nlohmann::json state() const
            {
                return state_;
            };
            // Apply a snapshot or patch. Returns true if the local
            // state now matches the advertised target (version/hash).
            /** \brief Apply a full snapshot to the mirror.
             *  \param snap Snapshot to adopt.
             *  \return Always \c true; warns and normalizes if the recomputed hash mismatches.
            */
            bool apply_snapshot(const Snapshot& snap);
            /** \brief Apply a patch to advance the mirror.
             *  \param p Patch describing ops from \c base_version to \c target_version.
             *  \return \c true if applied (and local state advanced), \c false if the base version
             *          mismatches or if JSON Patch application throws.
             *  \note On success, a post-apply hash mismatch is only warned and normalized.
            */
            bool apply_patch(const Patch& p);
        private:
            /** \brief Local canonical state. */
            nlohmann::json     state_{};
            /** \brief Local version. */
            uint64_t           version_   = 0;
            /** \brief Local canonical hash. */
            std::string        hash_{};
            /** \brief Optional logger. */
            LoggerPointer      logger_;
    };

}