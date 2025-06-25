#include "broker.h"

Broker::Broker(
    const std::string& logFile,
    const std::string& brokerName,
    const spdlog::level::level_enum logLevel
){

    // Logger initialization
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // Updated pattern includes [%n] to print the logger's name (i.e., serviceName)
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
    logger = std::make_shared<spdlog::logger>(brokerName, sinks.begin(), sinks.end());
    spdlog::register_logger(logger);
    logger->set_level(logLevel);
}

Broker::~Broker() = default;


void Broker::listen(){

    try{
        // Create a cpp-httplib server.
        httplib::Server svr;

        svr.Get("/state", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);
        });

        svr.Get("/temporal-query", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);
        });

        svr.Post("/add", [this](const httplib::Request& req, const httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);
        });


    }catch(const std::exception& e){
        logger->info("HTTP Server Error: {}", e.what());
    };
}

void Broker::add(){}

void Broker::get(){}

void Broker::remove(){}

void Broker::patch(){}



