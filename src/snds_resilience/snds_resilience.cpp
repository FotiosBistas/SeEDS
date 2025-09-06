#include "snds_resilience.hpp"
#include "snds_utils.h"

std::string snds_resilience::canonical_dump(const nlohmann::json& j) 
{
    nlohmann::ordered_json oj = j; // stable key order
    return oj.dump();              // compact, deterministic
}

std::string snds_resilience::canonical_hash(const nlohmann::json& j) 
{
    const auto s = canonical_dump(j);
    return md5_hex(s.data(), s.size());
}

snds_resilience::Store::Store(LoggerPointer logger): logger_(std::move(logger)) 
{
    // empty state, ver=0, hash of "{}"
    hash_ = canonical_hash(state_);
    history_[version_] = state_;
}

void snds_resilience::Store::mutate(std::function<void(nlohmann::json&)> fn) 
{
    fn(state_);
    ++version_;
    hash_ = canonical_hash(state_);
    history_[version_] = state_;
    // trim
    while (history_.size() > keep_) history_.erase(history_.begin());
    if (logger_) logger_->debug("[resilience] mutate → ver={} hash={}", version_, hash_);
}

void snds_resilience::Store::assign(nlohmann::json new_state) 
{
    state_ = std::move(new_state);
    ++version_;
    hash_ = canonical_hash(state_);
    history_[version_] = state_;
    while (history_.size() > keep_) history_.erase(history_.begin());
    if (logger_) logger_->debug("[resilience] assign → ver={} hash={}", version_, hash_);
}

std::optional<snds_resilience::Patch> snds_resilience::Store::make_patch_from(uint64_t base_version) const {
    auto it = history_.find(base_version);
    if (it == history_.end()) return std::nullopt;

    nlohmann::json diff = nlohmann::json::diff(it->second, state_);
    return snds_resilience::Patch
    {
        .base_version   = base_version,
        .target_version = version_,
        .target_hash    = hash_,
        .ops            = std::move(diff)
    };
}

snds_resilience::Mirror::Mirror(LoggerPointer logger): logger_(std::move(logger))
{
    // empty state, ver=0, hash of "{}"
    hash_ = canonical_hash(state_);
}


bool snds_resilience::Mirror::apply_snapshot(const Snapshot& snap) 
{
    state_    = snap.state;
    version_  = snap.version;
    hash_     = snap.hash;

    // sanity: recompute and warn if mismatch (won’t reject, but logs it)
    const auto recomputed = canonical_hash(state_);
    if (recomputed != hash_ && logger_) 
    {
        logger_->warn("[resilience] snapshot hash mismatch: advertised={} recomputed={}", hash_, recomputed);
        hash_ = recomputed; // normalize locally
    }

    return true;
}

bool snds_resilience::Mirror::apply_patch(const Patch& p) {

    if (p.base_version != version_) 
    {
        if (logger_) logger_->debug("[resilience] patch base {} != local {}", p.base_version, version_);
        return false; // caller should fetch snapshot
    }

    try 
    {
        state_ = state_.patch(p.ops);
    } 
    catch (const std::exception& e) 
    {
        if (logger_) logger_->warn("[resilience] patch apply failed: {}", e.what());
        return false;
    }

    version_  = p.target_version;
    hash_     = p.target_hash;

    // sanity check
    const auto recomputed = canonical_hash(state_);

    if (recomputed != hash_) 
    {
        if (logger_) logger_->warn("[resilience] post-patch hash mismatch: adv={} calc={}", hash_, recomputed);
        hash_ = recomputed;
    }

    return true;
}
