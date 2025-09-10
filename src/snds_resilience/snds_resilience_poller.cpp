#include "snds_resilience_poller.hpp"

ResiliencePoller::ResiliencePoller(
    ndn::Face &face,
    std::shared_ptr<spdlog::logger> log,
    std::string snds_prefix,
    std::string peer_is_alive_name,
    std::string peer_state_name,
    std::chrono::milliseconds alive_period,
    std::chrono::milliseconds meta_period) : face_(face), log_(std::move(log)), prefix_(std::move(snds_prefix)), peer_alive_(std::move(peer_is_alive_name)), peer_state_(std::move(peer_state_name)), alive_period_(alive_period), meta_period_(meta_period)
{
}

ResiliencePoller::~ResiliencePoller()
{
    stop();
}

void ResiliencePoller::start()
{
    if (running_.exchange(true))
        return;
    if (log_)
        log_->info("[poller] start peer_alive='{}' peer_state='{}'", peer_alive_, peer_state_);
    alive_thr_ = std::thread(&ResiliencePoller::alive_loop_, this);
    meta_thr_ = std::thread(&ResiliencePoller::meta_loop_, this);
}

void ResiliencePoller::stop()
{
    if (!running_.exchange(false))
        return;
    if (alive_thr_.joinable())
        alive_thr_.join();
    if (meta_thr_.joinable())
        meta_thr_.join();
    if (log_)
        log_->info("[poller] stop");
}

void ResiliencePoller::alive_loop_()
{
    // fire immediately, then periodic
    while (running_.load())
    {
        uint64_t nonce = rng_();
        face_.getIoContext().post([this, nonce]
                                  { send_is_alive_(nonce); });
        std::this_thread::sleep_for(alive_period_);
    }
}

void ResiliencePoller::meta_loop_()
{
    while (running_.load())
    {
        face_.getIoContext().post([this]
                                  { send_meta_(); });
        std::this_thread::sleep_for(meta_period_);
    }
}

std::string ResiliencePoller::make_alive_name_(uint64_t nonce) const
{
    return parsed_interest_name::to_custom_ndn_name(
        parsed_interest_name::make_parsed_interest_name(
            prefix_,
            peer_alive_,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_IS_ALIVE,
            {std::to_string(nonce)}));
}

std::string ResiliencePoller::make_snapshot_name_(uint64_t to_version) const
{
    return parsed_interest_name::to_custom_ndn_name(
        parsed_interest_name::make_parsed_interest_name(
            prefix_,
            peer_state_,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_SNAPSHOT,
            {std::to_string(to_version)}));
}

std::string ResiliencePoller::make_patch_name_(uint64_t from_version, uint64_t to_version) const
{
    return parsed_interest_name::to_custom_ndn_name(
        parsed_interest_name::make_parsed_interest_name(
            prefix_,
            peer_state_,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_PATCH,
            {std::to_string(from_version), std::to_string(to_version)}));
}

std::string ResiliencePoller::make_meta_name_() const
{
    return parsed_interest_name::to_custom_ndn_name(
        parsed_interest_name::make_parsed_interest_name(
            prefix_,
            peer_state_,
            parsed_interest_name::interest_message_type::INTEREST_MESSAGE_TYPE::GET_RESILIENCE_META,
            {}));
}

// ===== handlers =====
void ResiliencePoller::handle_is_alive_data_(const ndn::Interest &, const ndn::Data &data)
{
    const auto &block = data.getContent();
    std::string payload(reinterpret_cast<const char *>(block.value()), block.value_size());

    const parsed_interest_name::ParsedInterestName pi = parsed_interest_name::parse_from_string(url_decode(data.getName().toUri()));

    log_->info("[info] Received is alive data: {}", payload);

    bool ok = false;
    try
    {
        auto j = nlohmann::json::parse(payload);
        auto ack = j.value("ack", false);
        auto ns = j.value("nonce", std::string{});
        auto maybe_nonce = pi.get_interest_parameter(0);

        if (maybe_nonce.has_value())
        {
            auto nonce = maybe_nonce.value();
            ok = ack && ns == nonce;

            log_->info("[info] Set ok to: {}", ok);
        }
    }
    catch (...)
    {
        ok = false;
        log_->warn("[warning] Set ok to: {}", ok);
    }

    last_alive_ok_.store(ok);
    if (!ok && log_)
        log_->warn("[poller] is_alive bad ack/nonce");
    if (on_alive)
        on_alive(ok);
}

void ResiliencePoller::handle_is_alive_nack_(const ndn::Interest &, const ndn::lp::Nack &)
{
    last_alive_ok_.store(false);
    if (log_)
        log_->warn("[poller] is_alive NACK storing false");
    if (on_alive)
        on_alive(false);
}

void ResiliencePoller::handle_is_alive_timeout_(const ndn::Interest &)
{
    last_alive_ok_.store(false);
    if (log_)
        log_->warn("[poller] is_alive timeout storing false");
    if (on_alive)
        on_alive(false);
}

void ResiliencePoller::handle_meta_data_(const ndn::Interest &, const ndn::Data &data)
{
    const auto &block = data.getContent();
    std::string payload(reinterpret_cast<const char *>(block.value()), block.value_size());

    log_->info("[info] Received meta data: {}", payload);
    try
    {
        auto j = nlohmann::json::parse(payload);
        Meta m;
        m.version = j.value("version", 0ULL);
        m.hash = j.value("hash", std::string{});
        m.ts = j.value("ts", std::string{});
        last_meta_ = m;
        if (on_meta)
            on_meta(m);
        if (log_)
            log_->debug("[poller] meta v={} hash={}", m.version, m.hash);

        if (!last_applied_version_.has_value())
        {
            // first time: initialize via snapshot to ensure deterministic state
            if (log_)
                log_->info("[poller] no local version; fetching initial snapshot v={}", m.version);
            face_.getIoContext().post([this, v = m.version]
                                      { send_snapshot_(v); });
            return;
        }

        if (m.version > *last_applied_version_)
        {
            const uint64_t from_v = *last_applied_version_;
            const uint64_t to_v = m.version;

            // don't flood: only one patch at a time
            if (!patch_in_flight_.exchange(true))
            {
                if (log_)
                    log_->info("[poller] remote advanced: requesting patch {} -> {}", from_v, to_v);
                face_.getIoContext().post([this, from_v, to_v]
                                          { send_patch_(from_v, to_v); });
            }
            else
            {
                if (log_)
                    log_->debug("[poller] patch already in flight; skipping {} -> {}", from_v, to_v);
            }
        }
    }
    catch (const std::exception &e)
    {
        if (on_error)
            on_error(e.what());
        if (log_)
            log_->warn("[poller] meta parse error: {}", e.what());
    }
}

void ResiliencePoller::handle_meta_nack_(const ndn::Interest &, const ndn::lp::Nack &)
{
    if (on_error)
        on_error("meta NACK");
    if (log_)
        log_->warn("[poller] meta NACK");
}

void ResiliencePoller::handle_meta_timeout_(const ndn::Interest &)
{
    if (on_error)
        on_error("meta timeout");
    if (log_)
        log_->warn("[poller] meta timeout");
}

void ResiliencePoller::handle_patch_data_(const ndn::Interest& /*interest*/, const ndn::Data& data)
{
    const auto& content = data.getContent();
    std::string payload(reinterpret_cast<const char*>(content.value()), content.value_size());

    log_->info("[info] Received patch data: {}", payload);

    uint64_t from_version = 0, to_version = 0;

    try 
    {
        auto j = nlohmann::json::parse(payload);
        from_version = j.value("base_version",   0ULL);
        to_version   = j.value("target_version", 0ULL);
    } 
    catch (...) 
    {
        // keep going; we'll try the name
    }

    // Fallback: parse from Data name (DECODE FIRST!)
    if (from_version == 0 || to_version == 0) 
    {
        try 
        {
            const auto pi = parsed_interest_name::parse_from_string
            (
                url_decode(data.getName().toUri())
            );
            if (auto version = pi.get_interest_parameter(0); version) from_version = std::stoull(*version);
            if (auto version = pi.get_interest_parameter(1); version) to_version   = std::stoull(*version);
        } 
        catch (...) 
        {
            // ignore
        }
    }

    bool ok = false;
    if (on_apply_patch) 
    {
        try 
        {
            ok = on_apply_patch(payload);
        } 
        catch (const std::exception& e) 
        {
            if (log_) log_->warn("[poller] patch apply threw: {}", e.what());
            ok = false;
        }
    } 
    else 
    {
        if (log_) log_->warn("[poller] no on_apply_patch callback set; cannot apply patch");
    }

    if (ok) 
    {
        if (to_version) last_applied_version_ = to_version;
        patch_in_flight_.store(false);
        if (on_patch_applied_ok && from_version && to_version) on_patch_applied_ok(from_version, to_version);
        if (log_) log_->info("[poller] patch applied ok {}->{}", from_version, to_version);
    } 
    else 
    {
        // Fallback to snapshot if we now know the target version
        if (to_version) 
        {
            if (log_) log_->info("[poller] patch apply failed; fetching snapshot for v={}", to_version);
            face_.getIoContext().post([this, to_version]{ send_snapshot_(to_version); });
        } 
        else 
        {
            if (log_) log_->warn("[poller] patch apply failed and could not infer target version");
        }
        patch_in_flight_.store(false);
    }
}

void ResiliencePoller::handle_patch_nack_(const ndn::Interest& interest, const ndn::lp::Nack&)
{
    try 
    {
        const auto pi = parsed_interest_name::parse_from_string
        (
            url_decode(interest.getName().toUri())
        );
        if (auto version = pi.get_interest_parameter(1); version) 
        {
            uint64_t to_version = std::stoull(*version);
            if (log_) log_->warn("[poller] patch NACK; fallback to snapshot v={}", to_version);
            face_.getIoContext().post([this, to_version]{ send_snapshot_(to_version); });
            patch_in_flight_.store(false);
            return;
        }
    } 
    catch (...) 
    {

    }
    if (log_) log_->warn("[poller] patch NACK; no fallback possible");
    patch_in_flight_.store(false);
}

void ResiliencePoller::handle_patch_timeout_(const ndn::Interest& interest)
{
    try 
    {
        const auto pi = parsed_interest_name::parse_from_string
        (
            url_decode(interest.getName().toUri())
        );
        if (auto version = pi.get_interest_parameter(1); version) 
        {
            uint64_t to_version = std::stoull(*version);
            if (log_) log_->warn("[poller] patch timeout; fallback to snapshot v={}", to_version);
            face_.getIoContext().post([this, to_version]{ send_snapshot_(to_version); });
            patch_in_flight_.store(false);
            return;
        }
    } 
    catch (...) 
    {

    }
    if (log_) log_->warn("[poller] patch timeout; no fallback possible");
    patch_in_flight_.store(false);
}

// ===== snapshot handlers =====
void ResiliencePoller::handle_snapshot_data_(const ndn::Interest&, const ndn::Data& data)
{
    const auto& content = data.getContent();
    std::string payload(reinterpret_cast<const char*>(content.value()), content.value_size());

    log_->info("[info] Received snapshot data: {}", payload);

    uint64_t to_version = 0;

    try 
    {
        auto j = nlohmann::json::parse(payload);
        to_version = j.value("version", 0ULL);
    } 
    catch (...) 
    {
    }

    // Fallback: parse from the Data name (but decode first!)
    if (!to_version) 
    {
        try 
        {
            const auto pi = parsed_interest_name::parse_from_string(
                url_decode(data.getName().toUri())
            );
            if (auto v = pi.get_interest_parameter(0); v.has_value()) to_version = static_cast<uint64_t>(std::stoull(*v));
        } 
        catch (...) 
        {

        }
    }

    bool ok = true; // default to success if no callback
    if (on_apply_snapshot) 
    {
        try 
        { 
            ok = on_apply_snapshot(payload); 
        }
        catch (const std::exception& e) 
        { 
            if (log_) log_->warn("[poller] snapshot apply threw: {}", e.what()); 
            ok = false; 
        }
    } 
    else 
    {
        if (log_) log_->warn("[poller] no on_apply_snapshot callback set; cannot apply snapshot");
    }

    if (ok) 
    {
        if (to_version == 0 && log_) log_->warn("[poller] snapshot applied but could not determine version; leaving last_applied_version_ unchanged");
        if (to_version) last_applied_version_ = to_version;
        patch_in_flight_.store(false);
        if (on_snapshot_applied_ok && to_version) on_snapshot_applied_ok(to_version);
        if (log_) log_->info("[poller] snapshot applied ok v={}", to_version);
    } 
    else 
    {
        if (log_) log_->warn("[poller] snapshot apply failed");
        if (on_error) on_error("snapshot apply failed");
    }
}

void ResiliencePoller::handle_snapshot_nack_(const ndn::Interest & /*interest*/, const ndn::lp::Nack & /*nack*/)
{
    if (log_)
        log_->warn("[poller] snapshot NACK");
    if (on_error)
        on_error("snapshot NACK");
}

void ResiliencePoller::handle_snapshot_timeout_(const ndn::Interest & /*interest*/)
{
    if (log_)
        log_->warn("[poller] snapshot timeout");
    if (on_error)
        on_error("snapshot timeout");
}

void ResiliencePoller::send_is_alive_(uint64_t nonce)
{
    ndn::Interest interest(make_alive_name_(nonce));
    interest.setCanBePrefix(true);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::milliseconds(3000));

    if (log_)
        log_->info("[info] expressing interest: {}", interest.getName().toUri());

    face_.expressInterest(
        interest,
        std::bind(&ResiliencePoller::handle_is_alive_data_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_is_alive_nack_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_is_alive_timeout_, this, _1));
}

void ResiliencePoller::send_meta_()
{
    ndn::Interest interest(make_meta_name_());
    interest.setCanBePrefix(true);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::milliseconds(2000));

    if (log_)
        log_->info("[info] expressing interest: {}", interest.getName().toUri());

    face_.expressInterest(
        interest,
        std::bind(&ResiliencePoller::handle_meta_data_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_meta_nack_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_meta_timeout_, this, _1));
}

void ResiliencePoller::send_patch_(uint64_t from_version, uint64_t to_version)
{
    ndn::Interest interest(make_patch_name_(from_version, to_version));
    interest.setCanBePrefix(true);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::seconds(4));

    if (log_)
        log_->info("[poller] expressing patch: {}", interest.getName().toUri());

    face_.expressInterest(
        interest,
        std::bind(&ResiliencePoller::handle_patch_data_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_patch_nack_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_patch_timeout_, this, _1));
}

void ResiliencePoller::send_snapshot_(uint64_t to_version)
{
    ndn::Interest interest(make_snapshot_name_(to_version));
    interest.setCanBePrefix(true);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::seconds(6));

    if (log_)
        log_->info("[poller] expressing snapshot: {}", interest.getName().toUri());

    face_.expressInterest(
        interest,
        std::bind(&ResiliencePoller::handle_snapshot_data_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_snapshot_nack_, this, _1, _2),
        std::bind(&ResiliencePoller::handle_snapshot_timeout_, this, _1));
}
