// main.cpp
#include <ndn-cxx/face.hpp>
#include <boost/asio/signal_set.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "snds_resilience_poller.hpp"
#include "snds_resilience.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <chrono>
#include <functional>
#include <csignal>

using std::placeholders::_1;
using std::placeholders::_2;

// --- helpers wired via std::bind ---

static bool apply_snapshot_from_json(snds_resilience::Mirror* mirror, std::shared_ptr<spdlog::logger> log, const std::string& payload)
{
    try 
    {
        auto j = nlohmann::json::parse(payload);
        snds_resilience::Snapshot snap
        {
            j.value("version", 0ULL),
            j.value("hash", std::string{}),
            j.at("state")
        };
        return mirror->apply_snapshot(snap);
    } 
    catch (const std::exception& e) 
    {
        if (log) log->warn("[poller] snapshot parse/apply failed: {}", e.what());
        return false;
    }
}

static bool apply_patch_from_json(snds_resilience::Mirror* mirror, std::shared_ptr<spdlog::logger> log, const std::string& payload)
{
    try 
    {
        auto j = nlohmann::json::parse(payload);
        snds_resilience::Patch p
        {
            j.value("base_version",   0ULL),
            j.value("target_version", 0ULL),
            j.value("target_hash",    std::string{}),
            j.at("ops")
        };
        return mirror->apply_patch(p);
    } 
    catch (const std::exception& e) 
    {
        if (log) log->warn("[poller] patch parse/apply failed: {}", e.what());
        return false;
    }
}

static void on_patch_applied_log(std::shared_ptr<spdlog::logger> log, uint64_t from_v, uint64_t to_v)
{
    if (log) log->info("[poller] mirror advanced {} -> {}", from_v, to_v);
}

static void on_snapshot_applied_log(std::shared_ptr<spdlog::logger> log, uint64_t to_v)
{
    if (log) log->info("[poller] mirror snap to v={}", to_v);
}

static void handle_signal(const boost::system::error_code& ec, int signum, ResiliencePoller* poller, ndn::Face* face, std::shared_ptr<spdlog::logger> log)
{
    if (ec) return; // operation aborted
    if (log) log->warn("[main] caught signal {}, stopping…", signum);
    if (poller) poller->stop();
    if (face)   face->getIoContext().stop();
}

int main(int argc, char** argv)
{
    // ---- config from argv (optional) ----
    // argv[1]: prefix (default "SNDS")
    // argv[2]: peer is_alive name (default "is_alive_0")
    // argv[3]: peer state name   (default "state_0")
    // argv[4]: alive period ms   (default 2000)
    // argv[5]: meta  period ms   (default 2000)
    const std::string prefix       = (argc > 1) ? argv[1] : "SNDS";
    const std::string peer_alive   = (argc > 2) ? argv[2] : "is_alive_0";
    const std::string peer_state   = (argc > 3) ? argv[3] : "state_0";
    const int         alive_ms     = (argc > 4) ? std::stoi(argv[4]) : 2000;
    const int         meta_ms      = (argc > 5) ? std::stoi(argv[5]) : 2000;

    // ---- logger ----
    auto logger = spdlog::stdout_color_mt("poller");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    logger->info("[main] starting poller: prefix='{}' alive='{}' state='{}' alive_ms={} meta_ms={}", prefix, peer_alive, peer_state, alive_ms, meta_ms);

    // ---- face & signals ----
    ndn::Face face;
    boost::asio::signal_set signals(face.getIoContext(), SIGINT, SIGTERM);

    // ---- poller ----
    ResiliencePoller poller(
        face,
        logger,
        prefix,
        peer_alive,
        peer_state,
        std::chrono::milliseconds(alive_ms),
        std::chrono::milliseconds(meta_ms)
    );

    snds_resilience::Mirror mirror(logger);

    // wire callbacks
    poller.on_alive = std::bind(
        [](std::shared_ptr<spdlog::logger> log, bool ok){
            if (!ok && log) log->warn("[poller] heartbeat missed");
        },
        logger, _1
    );

    poller.on_meta = std::bind(
        [](std::shared_ptr<spdlog::logger> log, const ResiliencePoller::Meta& m){
            if (log) log->info("[poller] remote version={} hash={}", m.version, m.hash);
        },
        logger, _1
    );

    // Apply snapshot/patch via Mirror (std::bind, no lambdas)
    poller.on_apply_snapshot      = std::bind(&apply_snapshot_from_json, &mirror, logger, _1);
    poller.on_apply_patch         = std::bind(&apply_patch_from_json,    &mirror, logger, _1);
    poller.on_patch_applied_ok    = std::bind(&on_patch_applied_log,     logger, _1, _2);
    poller.on_snapshot_applied_ok = std::bind(&on_snapshot_applied_log,  logger, _1);

    poller.on_error = std::bind(
        [](std::shared_ptr<spdlog::logger> log, const std::string& e){
            if (log) log->warn("[poller] error: {}", e);
        },
        logger, _1
    );

    poller.start();

    // arm signal handler (now that poller exists)
    signals.async_wait(std::bind(&handle_signal, _1, _2, &poller, &face, logger));

    // ---- run event loop ----
    try 
    {
        face.getIoContext().run();
    } 
    catch (const std::exception& e) 
    {
        logger->error("[main] exception: {}", e.what());
    }

    logger->info("[main] exited");
    return 0;
}
