#include <iostream>
#include <string>
#include <algorithm>

#include <spdlog/spdlog.h>
#include "cxxopts.hpp"

#include "broker.hpp"

static spdlog::level::level_enum parse_log_level(std::string s, spdlog::level::level_enum def) 
{
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

int main(int argc, char* argv[]) {
    // ---------------------------
    // CLI definition
    // ---------------------------
    cxxopts::Options opts("snds_broker_bin", "SNDS Broker (Mongo-backed) with CLI configuration");

    opts.add_options()
        // MongoDB
        ("mongo.uri",  "Full MongoDB URI (overrides user/pass/db if set)",
        cxxopts::value<std::string>()
            ->default_value("mongodb://127.0.0.1:10000"))
        ("mongo.user", "MongoDB username",
            cxxopts::value<std::string>()->default_value(""))
        ("mongo.pass", "MongoDB password",
            cxxopts::value<std::string>()->default_value(""))
        ("mongo.db",   "MongoDB database name",
            cxxopts::value<std::string>()->default_value("SeEDS"))

        // Broker service settings
        ("log-level",   "Log level (trace|debug|info|warn|error|critical|off)",
            cxxopts::value<std::string>()->default_value("debug"))
        ("log-file",    "Log file name",
            cxxopts::value<std::string>()->default_value("Broker.log"))
        ("service-name","Service label",
            cxxopts::value<std::string>()->default_value("Broker"))
        ("bind-ip",     "Bind IP for HTTP server",
            cxxopts::value<std::string>()->default_value("0.0.0.0"))
        ("bind-port",   "Bind port for HTTP server",
            cxxopts::value<int>()->default_value("10000"))

        ("h,help", "Show help");

    cxxopts::ParseResult result;
    try 
    {
        result = opts.parse(argc, argv);
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error parsing options: " << e.what() << "\n" << opts.help() << std::endl;
        return 1;
    }

    if (result.count("help")) 
    {
        std::cout << opts.help() << std::endl;
        return 0;
    }

    // ---------------------------
    // Resolve config
    // ---------------------------
    const auto lvl  = parse_log_level(result["log-level"].as<std::string>(), spdlog::level::debug);
    const auto file = result["log-file"].as<std::string>();
    const auto name = result["service-name"].as<std::string>();
    const auto ip   = result["bind-ip"].as<std::string>();
    const auto port = result["bind-port"].as<int>();

    std::string uri_str = result["mongo.uri"].as<std::string>();
    const auto user = result["mongo.user"].as<std::string>();
    const auto pass = result["mongo.pass"].as<std::string>();
    const auto db   = result["mongo.db"].as<std::string>();
    //if (user.empty() || pass.empty()) 
    //{
    //    std::cerr << "Missing Mongo credentials. Either provide --mongo.uri or both --mongo.user and --mongo.pass.\n\n" << opts.help() << std::endl;
    //    return 2;
    //}
    if (!user.empty() && !pass.empty()) 
    {
        uri_str.replace(uri_str.find("<user>"), 6, user);
        uri_str.replace(uri_str.find("<pass>"), 6, pass);
    }


    std::cout << "Using URI: " << uri_str << std::endl;

    // ---------------------------
    // Run broker
    // ---------------------------
    try 
    {
        mongocxx::instance instance{}; // one per process
        mongocxx::uri uri(uri_str);

        Broker broker
        (
            uri,
            db,
            lvl,
            file,
            name,
            ip,
            static_cast<uint16_t>(port)
        );
        broker.listen();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Broker startup failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
