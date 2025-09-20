#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <algorithm>

#include <spdlog/spdlog.h>
#include "cxxopts.hpp"

// project headers
#include "snds_consumer.hpp"
#include "snds_http_server.hpp"
#include "snds_producer.hpp"
#include "snds_digital_twin.hpp"
#include "snds_worker.hpp"
#include "snds_daemon.hpp"

// ----------------------------------------------------------------------------
// Global shutdown flag + handler
// ----------------------------------------------------------------------------
std::atomic<bool> g_stop{false};
void signalHandler(int s) { if (s == SIGINT) g_stop.store(true); }

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------
static spdlog::level::level_enum parse_log_level(std::string s, spdlog::level::level_enum def) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "trace") return spdlog::level::trace;
    if (s == "debug") return spdlog::level::debug;
    if (s == "info")  return spdlog::level::info;
    if (s == "warn" || s == "warning") return spdlog::level::warn;
    if (s == "err"  || s == "error")   return spdlog::level::err;
    if (s == "critical")               return spdlog::level::critical;
    if (s == "off")                    return spdlog::level::off;
    return def;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signalHandler);

    // ----------------------------------------------------------------------------
    // Define CLI
    // ----------------------------------------------------------------------------
    cxxopts::Options opts("snds", "SNDS daemon with CLI configuration");

    // Global defaults (act as fallbacks for each service unless overridden)
    opts.add_options("global")
        ("log-level", "Global log level (trace|debug|info|warn|error|critical|off)",
            cxxopts::value<std::string>()->default_value("info"))
        ("num-services", "Global number of SNDS services",
            cxxopts::value<size_t>()->default_value("1"))
        ("snds-prefix", "Global SNDS root prefix",
            cxxopts::value<std::string>()->default_value("SNDS"))
        ("state-prefix", "Global state prefix",
            cxxopts::value<std::string>()->default_value("state"))
        ("is-alive-prefix", "Global liveness prefix",
            cxxopts::value<std::string>()->default_value("is_alive"))
        ("snds-id", "Global SNDS node id",
            cxxopts::value<std::string>()->default_value("0"))
        ("id-scope-prefix", "Global ID scope prefix",
            cxxopts::value<std::string>()->default_value("SNDS_ID_scope_"))
        ("registry-file-postfix", "Global registry file postfix",
            cxxopts::value<std::string>()->default_value("_registry"))
        ("help", "Show help")
    ;

    // Producer (adds broker + resilience IPC + registry type)
    opts.add_options("producer")
        ("producer.log-level", "Producer log level",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.log-file", "Producer log file",
            cxxopts::value<std::string>()->default_value("Producer.log"))
        ("producer.num-services", "Producer num services",
            cxxopts::value<size_t>()->default_value("0")) // 0 => use global
        ("producer.snds-prefix", "Producer SNDS prefix",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.state-prefix", "Producer state prefix",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.is-alive-prefix", "Producer liveness prefix",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.snds-id", "Producer SNDS id",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.id-scope-prefix", "Producer ID scope prefix",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.registry-file-postfix", "Producer registry postfix",
            cxxopts::value<std::string>()->default_value(""))
        ("producer.service-name", "Producer service name",
            cxxopts::value<std::string>()->default_value("Producer"))
        ("producer.broker-ip", "Producer broker IP/host",
            cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("producer.broker-port", "Producer broker port",
            cxxopts::value<size_t>()->default_value("10000"))
        ("producer.registry-type", "Producer initial registry type",
            cxxopts::value<std::string>()->default_value("CAR"))
        ("producer.enable-resilience-ipc", "Enable resilience IPC (true/false)",
            cxxopts::value<std::string>()->default_value("false"))
        ("producer.resilience-ipc-host", "IPC host",
            cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("producer.resilience-ipc-port", "IPC port",
            cxxopts::value<size_t>()->default_value("47832"))
    ;

    // Consumer
    opts.add_options("consumer")
        ("consumer.log-level", "Consumer log level", cxxopts::value<std::string>()->default_value(""))
        ("consumer.log-file", "Consumer log file", cxxopts::value<std::string>()->default_value("Consumer.log"))
        ("consumer.num-services", "Consumer num services", cxxopts::value<size_t>()->default_value("0"))
        ("consumer.snds-prefix", "Consumer SNDS prefix", cxxopts::value<std::string>()->default_value(""))
        ("consumer.state-prefix", "Consumer state prefix", cxxopts::value<std::string>()->default_value(""))
        ("consumer.is-alive-prefix", "Consumer liveness prefix", cxxopts::value<std::string>()->default_value(""))
        ("consumer.snds-id", "Consumer SNDS id", cxxopts::value<std::string>()->default_value(""))
        ("consumer.id-scope-prefix", "Consumer ID scope prefix", cxxopts::value<std::string>()->default_value(""))
        ("consumer.registry-file-postfix", "Consumer registry postfix", cxxopts::value<std::string>()->default_value(""))
        ("consumer.service-name", "Consumer service name", cxxopts::value<std::string>()->default_value("Consumer"))
    ;

    // Worker (note: your ctor forwards is_alive then state to ServiceBase)
    opts.add_options("worker")
        ("worker.log-level", "Worker log level", cxxopts::value<std::string>()->default_value(""))
        ("worker.log-file", "Worker log file", cxxopts::value<std::string>()->default_value("Worker.log"))
        ("worker.num-services", "Worker num services", cxxopts::value<size_t>()->default_value("0"))
        ("worker.snds-prefix", "Worker SNDS prefix", cxxopts::value<std::string>()->default_value(""))
        ("worker.state-prefix", "Worker state prefix", cxxopts::value<std::string>()->default_value(""))
        ("worker.is-alive-prefix", "Worker liveness prefix", cxxopts::value<std::string>()->default_value(""))
        ("worker.snds-id", "Worker SNDS id", cxxopts::value<std::string>()->default_value(""))
        ("worker.id-scope-prefix", "Worker ID scope prefix", cxxopts::value<std::string>()->default_value(""))
        ("worker.registry-file-postfix", "Worker registry postfix", cxxopts::value<std::string>()->default_value(""))
        ("worker.service-name", "Worker service name", cxxopts::value<std::string>()->default_value("Worker"))
    ;

    // HTTP
    opts.add_options("http")
        ("http.log-level", "HTTP log level", cxxopts::value<std::string>()->default_value(""))
        ("http.log-file", "HTTP log file", cxxopts::value<std::string>()->default_value("HTTPServer.log"))
        ("http.num-services", "HTTP num services", cxxopts::value<size_t>()->default_value("0"))
        ("http.snds-prefix", "HTTP SNDS prefix", cxxopts::value<std::string>()->default_value(""))
        ("http.state-prefix", "HTTP state prefix", cxxopts::value<std::string>()->default_value(""))
        ("http.is-alive-prefix", "HTTP liveness prefix", cxxopts::value<std::string>()->default_value(""))
        ("http.snds-id", "HTTP SNDS id", cxxopts::value<std::string>()->default_value(""))
        ("http.id-scope-prefix", "HTTP ID scope prefix", cxxopts::value<std::string>()->default_value(""))
        ("http.registry-file-postfix", "HTTP registry postfix", cxxopts::value<std::string>()->default_value(""))
        ("http.service-name", "HTTP service name", cxxopts::value<std::string>()->default_value("HTTPServer"))
    ;

    // DigitalTwin
    opts.add_options("digitaltwin")
        ("dt.log-level", "DigitalTwin log level", cxxopts::value<std::string>()->default_value(""))
        ("dt.log-file", "DigitalTwin log file", cxxopts::value<std::string>()->default_value("DigitalTwin.log"))
        ("dt.num-services", "DigitalTwin num services", cxxopts::value<size_t>()->default_value("0"))
        ("dt.snds-prefix", "DigitalTwin SNDS prefix", cxxopts::value<std::string>()->default_value(""))
        ("dt.state-prefix", "DigitalTwin state prefix", cxxopts::value<std::string>()->default_value(""))
        ("dt.is-alive-prefix", "DigitalTwin liveness prefix", cxxopts::value<std::string>()->default_value(""))
        ("dt.snds-id", "DigitalTwin SNDS id", cxxopts::value<std::string>()->default_value(""))
        ("dt.id-scope-prefix", "DigitalTwin ID scope prefix", cxxopts::value<std::string>()->default_value(""))
        ("dt.registry-file-postfix", "DigitalTwin registry postfix", cxxopts::value<std::string>()->default_value(""))
        ("dt.service-name", "DigitalTwin service name", cxxopts::value<std::string>()->default_value("DigitalTwin"))
    ;

    // ----------------------------------------------------------------------------
    // Parse CLI
    // ----------------------------------------------------------------------------
    auto result = opts.parse(argc, argv);
    if (result.count("help")) {
        std::cout << opts.help({"global","producer","consumer","worker","http","digitaltwin"}) << "\n";
        return 0;
    }

    // Read global fallbacks
    const auto g_log_level  = parse_log_level(result["log-level"].as<std::string>(), spdlog::level::info);
    const auto g_num        = result["num-services"].as<size_t>();
    const auto g_snds       = result["snds-prefix"].as<std::string>();
    const auto g_state      = result["state-prefix"].as<std::string>();
    const auto g_alive      = result["is-alive-prefix"].as<std::string>();
    const auto g_id         = result["snds-id"].as<std::string>();
    const auto g_scope      = result["id-scope-prefix"].as<std::string>();
    const auto g_regpost    = result["registry-file-postfix"].as<std::string>();

    // Helper lambdas to map overridden-or-global
    auto pick_str = [&](const char* key) -> std::string {
        return result[key].as<std::string>().empty() ? std::string() : result[key].as<std::string>();
    };
    auto pick_size = [&](const char* key) -> size_t {
        size_t v = result[key].as<size_t>();
        return v == 0 ? g_num : v;
    };
    auto pick_level = [&](const char* key) -> spdlog::level::level_enum {
        auto s = result[key].as<std::string>();
        return s.empty() ? g_log_level : parse_log_level(s, g_log_level);
    };
    auto coalesce = [](const std::string& a, const std::string& b){ return a.empty()? b : a; };

    // ----------------------------------------------------------------------------
    // Resolve Producer args
    // ----------------------------------------------------------------------------
    const auto p_level = pick_level("producer.log-level");
    const auto p_file  = result["producer.log-file"].as<std::string>();
    const auto p_num   = pick_size("producer.num-services");
    const auto p_snds  = coalesce(pick_str("producer.snds-prefix"), g_snds);
    const auto p_state = coalesce(pick_str("producer.state-prefix"), g_state);
    const auto p_alive = coalesce(pick_str("producer.is-alive-prefix"), g_alive);
    const auto p_id    = coalesce(pick_str("producer.snds-id"), g_id);
    const auto p_scope = coalesce(pick_str("producer.id-scope-prefix"), g_scope);
    const auto p_rpost = coalesce(pick_str("producer.registry-file-postfix"), g_regpost);
    const auto p_name  = result["producer.service-name"].as<std::string>();
    const auto p_bip   = result["producer.broker-ip"].as<std::string>();
    const auto p_bport = result["producer.broker-port"].as<size_t>();
    const auto p_rtype = result["producer.registry-type"].as<std::string>();
    const auto p_ipc_en = [](const std::string& s){ 
        std::string t=s; std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        return (t=="1"||t=="true"||t=="yes"||t=="on");
    }(result["producer.enable-resilience-ipc"].as<std::string>());
    const auto p_ipc_host = result["producer.resilience-ipc-host"].as<std::string>();
    const auto p_ipc_port = result["producer.resilience-ipc-port"].as<size_t>();

    // Consumer
    const auto c_level = pick_level("consumer.log-level");
    const auto c_file  = result["consumer.log-file"].as<std::string>();
    const auto c_num   = pick_size("consumer.num-services");
    const auto c_snds  = coalesce(pick_str("consumer.snds-prefix"), g_snds);
    const auto c_state = coalesce(pick_str("consumer.state-prefix"), g_state);
    const auto c_alive = coalesce(pick_str("consumer.is-alive-prefix"), g_alive);
    const auto c_id    = coalesce(pick_str("consumer.snds-id"), g_id);
    const auto c_scope = coalesce(pick_str("consumer.id-scope-prefix"), g_scope);
    const auto c_rpost = coalesce(pick_str("consumer.registry-file-postfix"), g_regpost);
    const auto c_name  = result["consumer.service-name"].as<std::string>();

    // Worker (note ctor forwards is_alive then state to ServiceBase)
    const auto w_level = pick_level("worker.log-level");
    const auto w_file  = result["worker.log-file"].as<std::string>();
    const auto w_num   = pick_size("worker.num-services");
    const auto w_snds  = coalesce(pick_str("worker.snds-prefix"), g_snds);
    const auto w_state = coalesce(pick_str("worker.state-prefix"), g_state);
    const auto w_alive = coalesce(pick_str("worker.is-alive-prefix"), g_alive);
    const auto w_id    = coalesce(pick_str("worker.snds-id"), g_id);
    const auto w_scope = coalesce(pick_str("worker.id-scope-prefix"), g_scope);
    const auto w_rpost = coalesce(pick_str("worker.registry-file-postfix"), g_regpost);
    const auto w_name  = result["worker.service-name"].as<std::string>();

    // HTTP
    const auto h_level = pick_level("http.log-level");
    const auto h_file  = result["http.log-file"].as<std::string>();
    const auto h_num   = pick_size("http.num-services");
    const auto h_snds  = coalesce(pick_str("http.snds-prefix"), g_snds);
    const auto h_state = coalesce(pick_str("http.state-prefix"), g_state);
    const auto h_alive = coalesce(pick_str("http.is-alive-prefix"), g_alive);
    const auto h_id    = coalesce(pick_str("http.snds-id"), g_id);
    const auto h_scope = coalesce(pick_str("http.id-scope-prefix"), g_scope);
    const auto h_rpost = coalesce(pick_str("http.registry-file-postfix"), g_regpost);
    const auto h_name  = result["http.service-name"].as<std::string>();

    // DigitalTwin
    const auto d_level = pick_level("dt.log-level");
    const auto d_file  = result["dt.log-file"].as<std::string>();
    const auto d_num   = pick_size("dt.num-services");
    const auto d_snds  = coalesce(pick_str("dt.snds-prefix"), g_snds);
    const auto d_state = coalesce(pick_str("dt.state-prefix"), g_state);
    const auto d_alive = coalesce(pick_str("dt.is-alive-prefix"), g_alive);
    const auto d_id    = coalesce(pick_str("dt.snds-id"), g_id);
    const auto d_scope = coalesce(pick_str("dt.id-scope-prefix"), g_scope);
    const auto d_rpost = coalesce(pick_str("dt.registry-file-postfix"), g_regpost);
    const auto d_name  = result["dt.service-name"].as<std::string>();

    // ----------------------------------------------------------------------------
    // Build + run
    // ----------------------------------------------------------------------------
    SNDSDaemon daemon;

    auto producer = std::make_unique<SNDSProducer>(
        p_level, p_file, p_num, p_snds, p_state, p_alive, p_id, p_scope, p_rpost,
        p_name, p_bip, p_bport, p_rtype, p_ipc_en, p_ipc_host, p_ipc_port
    );

    auto consumer = std::make_unique<SNDSConsumer>(
        c_level, c_file, c_num, c_snds, c_state, c_alive, c_id, c_scope, c_rpost, c_name
    );

    auto worker = std::make_unique<Worker>(
        w_level, w_file, w_num, w_snds, w_state, w_alive, w_id, w_scope, w_rpost, w_name
    );

    auto http = std::make_unique<SNDSHTTPService>(
        h_level, h_file, h_num, h_snds, h_state, h_alive, h_id, h_scope, h_rpost, h_name
    );

    auto dt = std::make_unique<DigitalTwin>(
        d_level, d_file, d_num, d_snds, d_state, d_alive, d_id, d_scope, d_rpost, d_name
    );

    daemon.addService(std::move(producer));
    daemon.addService(std::move(consumer));
    daemon.addService(std::move(dt));
    daemon.addService(std::move(http));
    daemon.addService(std::move(worker));

    daemon.start();
    std::cout << "Running... Press Ctrl-C to stop.\n";
    while (!g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    daemon.stop();
    return 0;
}
