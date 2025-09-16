// main.cpp
#include <memory>
#include <string>
#include <chrono>
#include <functional>
#include <csignal>

#include <ndn-cxx/face.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

#include "snds_resilience_poller.hpp"
#include "snds_resilience.hpp"
#include "snds_ipc_tcp.hpp"



using std::placeholders::_1;
using std::placeholders::_2;

std::atomic<int>  miss_streak{0};
std::atomic<bool> promoted{false};

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
    // Make host/port configurable (argv[6], argv[7]) with sensible defaults
    const std::string prefix       = (argc > 1) ? argv[1] : "SNDS";
    const std::string peer_alive   = (argc > 2) ? argv[2] : "is_alive_0";
    const std::string peer_state   = (argc > 3) ? argv[3] : "state_0";
    const int         alive_ms     = (argc > 4) ? std::stoi(argv[4]) : 2000;
    const int         meta_ms      = (argc > 5) ? std::stoi(argv[5]) : 2000;
    const std::string ipc_host = (argc > 6) ? argv[6] : "127.0.0.1";
    const uint16_t    ipc_port = (argc > 7) ? static_cast<uint16_t>(std::stoi(argv[7])) : 47832;


    // ---- logger ----
    auto logger = spdlog::stdout_color_mt("poller");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    logger->info("[main] starting poller: prefix='{}' alive='{}' state='{}' alive_ms={} meta_ms={}", prefix, peer_alive, peer_state, alive_ms, meta_ms);
    logger->info("[main] starting IPC communication: ipc_host='{}', ipc_port='{}'", ipc_host, ipc_port);

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


    auto ipc_send = [&](const char* kind, const std::string& payload)
    {
        try 
        {
            nlohmann::json env = {{"kind", kind}, {"payload", nlohmann::json::parse(payload)}};

            boost::asio::io_context ioc;
            boost::asio::ip::tcp::socket sock{ioc};
            boost::asio::ip::tcp::endpoint ep(
            boost::asio::ip::make_address(ipc_host), ipc_port);
            sock.connect(ep);
            snds_ipc_tcp::send_json(sock, env);
            sock.close();
        } 
        catch (...) 
        {
            if (logger) logger->debug("[ipc] send failed (non-fatal)");
        }
    };
    // wire callbacks
    poller.on_alive = [&, logger](bool ok) 
    {
        if (ok) 
        {
            if (miss_streak.load() != 0 && logger) logger->info("[poller] heartbeat OK (miss streak {} -> 0)", miss_streak.load());
            miss_streak.store(0);
            return;
        }

        const int m = miss_streak.fetch_add(1) + 1;
        if (logger) logger->warn("[poller] heartbeat missed (streak = {})", m);

        if (m >= 3 && !promoted.exchange(true)) 
        {
            if (logger) logger->warn("[poller] promoting to PRIMARY ({} consecutive misses)", m);
            nlohmann::json role = 
            {
                {"role",   "primary"},
                {"reason", "alive-missed"},
                {"misses", m}
            };
            ipc_send("role", role.dump());

            // --- stop this process cleanly after promotion ---
            if (logger) logger->warn("[poller] stopping poller and IO after promotion");
            poller.stop();
            face.getIoContext().stop();
        }
    };

    poller.on_meta = [&](const ResiliencePoller::Meta& m) {
        if (logger) logger->info("[poller] remote version={} hash={}", m.version, m.hash);

        // Recreate the meta JSON the producer expects to log
        nlohmann::json meta = {
            {"version", m.version},
            {"hash",    m.hash},
            {"ts",      m.ts}
            // You can also include state_name if you want, e.g. {"state_name", peer_state}
        };

        if (logger) logger->info("[poller] Sending META to the IPC channel");
        ipc_send("meta", meta.dump());
    };

    // Apply snapshot/patch via Mirror (std::bind, no lambdas)
    poller.on_apply_snapshot = [&](const std::string& payload) 
    {
        const bool ok = apply_snapshot_from_json(&mirror, logger, payload);
        if (ok) 
        {
            if(logger) logger->info("[poller] Sending data to the IPC channel");
            ipc_send("snapshot", payload);
        }
        return ok;
    };

    poller.on_apply_patch = [&](const std::string& payload) 
    {
        const bool ok = apply_patch_from_json(&mirror, logger, payload);
        if (ok) 
        {
            if(logger) logger->info("[poller] Sending data to the IPC channel");
            ipc_send("patch", payload);
        }
        return ok;
    };

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
