#ifndef SNDS_DAEMON_H
#define SNDS_DAEMON_H

#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "snds_service_base.h"


class SNDSDaemon {
public:
    SNDSDaemon(spdlog::level::level_enum logLevel = spdlog::level::info,
               const std::string& logFile = "SNDSDaemon.log")
    {
        // Set up logging with both console and file sinks.
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        std::vector<spdlog::sink_ptr> sinks { consoleSink, fileSink };
        logger = std::make_shared<spdlog::logger>("SNDSDaemon", sinks.begin(), sinks.end());
        spdlog::register_logger(logger);
        logger->set_level(logLevel);
    }

    /**
     * @brief Registers a new service.
     *
     * @param service Pointer to a ServiceBase‑derived service.
     */
    void addService(std::unique_ptr<ServiceBase> service) {
        services.push_back(std::move(service));
    }


    /**
     * @brief Starts all registered service components by calling their init functions in separate threads.
     *
     * Since service's init (e.g. func_proxy) runs a perpetual loop,
     * this launches each service concurrently.
     */
    void start() {
        logger->info("SNDSDaemon: Starting all services...");
        for (auto& service : services) {
            threads.emplace_back([this, servicePtr = service.get()]() {
                servicePtr->init();
            });
        }
        logger->info("SNDSDaemon: All services have been started.");
    }

    void broadcastDummyData(){

        auto data1 = std::make_unique<SNDS_INTRA_MESSAGE>(
            "/Car1",
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::CONSUMER,
            "Car"
        ); 
        ServiceBase::v_to_consumer->writeMessage(std::move(data1));
        auto data2 = std::make_unique<SNDS_INTRA_MESSAGE>(
            "/Car1",
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::PRODUCER,
            "Car"
        ); 
        ServiceBase::v_to_producer->writeMessage(std::move(data2));
        auto data3 = std::make_unique<SNDS_INTRA_MESSAGE>(
            "/Car1/id=hey",
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::DIGITAL_TWIN,
            "Car"
        ); 
        ServiceBase::v_to_dt->writeMessage(std::move(data3));
        auto data4 = std::make_unique<SNDS_INTRA_MESSAGE>(
            "/Car1/type=car",
            SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
            SNDS_CLIENT_TYPES::DIGITAL_TWIN,
            "Car"
        ); 
        ServiceBase::v_to_dt->writeMessage(std::move(data4));
    }

    /**
     * @brief Stops the daemon by joining all service threads.
     */
    void stop() {
        logger->info("SNDSDaemon: Stopping all services...");
        
        // Signal each service to stop.
        for (auto& service : services) {
            service->stop();  
        }
        
        // Also signal the message queues to shut down.
        ServiceBase::v_to_proxy->shutdown();
        ServiceBase::v_to_dt->shutdown();
        ServiceBase::v_to_producer->shutdown();
        ServiceBase::v_to_consumer->shutdown();
        ServiceBase::v_to_service->shutdown();
        ServiceBase::v_to_worker->shutdown();
        
        // Now join all threads.
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        logger->info("SNDSDaemon: All services stopped.");
    }

private:
    std::vector<std::unique_ptr<ServiceBase>> services;
    std::vector<std::thread> threads;
    std::shared_ptr<spdlog::logger> logger;
};

#endif // SNDS_DAEMON_H