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
        const std::string& logFile = "Broker.log",
        const std::string& brokerName = "Broker",
        const std::string& ipAddress = "0.0.0.0",
        const int          port = 8080,
        const spdlog::level::level_enum logLevel = spdlog::level::info
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
    int port;

    static std::atomic<bool> shouldShutdown;

    std::shared_ptr<spdlog::logger> logger;
    //TODO is this correct?
    // or should the broker be determined dynamically?
    std::unordered_map<std::string, std::string> service_locations;


    void add();
    void get();
    void remove();
    void patch();
};