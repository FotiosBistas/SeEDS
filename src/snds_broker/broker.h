#pragma once
#include "httplib.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

class Broker{
public: 
    Broker();
    ~Broker();
private:
    std::shared_ptr<spdlog::logger> logger;
    //TODO is this correct?
    // or should the broker be determined dynamically?
    std::unordered_map<std::string, std::string> service_locations;


    void listen();
};