#pragma once

#include "httplib.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

class Broker{
public: 

    Broker(
        const mongocxx::uri& uri,
        const std::string& database,
        const spdlog::level::level_enum logLevel = spdlog::level::info,
        const std::string& logFile = "Broker.log",
        const std::string& brokerName = "Broker",
        const std::string& ipAddress = "0.0.0.0",
        const int          port = 8080
    );

    inline static void initiateShutdown(){
        shouldShutdown.store(true);
    };

    inline static void handleSigint(int){
        initiateShutdown();
    };


    ~Broker();
    void listen();
private:

    std::string logFile;
    std::string ipAddress;
    std::string database;
    int port;

    mongocxx::client client; 

    static std::atomic<bool> shouldShutdown;

    std::shared_ptr<spdlog::logger> logger;
    //TODO is this correct?
    // or should the broker be determined dynamically?
    std::unordered_map<std::string, std::string> service_locations;


    std::optional<mongocxx::v_noabi::result::insert_one> add_one(const std::string& body);
    void get();
    void remove();
    void patch();
};