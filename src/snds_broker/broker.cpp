#include "broker.h"


void Broker::listen(){

    try{
        // Create a cpp-httplib server.
        httplib::Server svr;

        svr.Get("/state", [this](const httplib::Request& req, httplib::Response& res){

        });

        svr.Get("/temporal-query", [this](const httplib::Request& req, httplib::Response& res){

        });

    }catch(const std::exception& e){
        logger->info("HTTP Server Error: {}", e.what());
    };
}


