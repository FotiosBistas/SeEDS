#ifndef SNDS_DAEMON_H
#define SNDS_DAEMON_H

#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "snds_service_base.h"  // This header declares ServiceBase and its init() method

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
     * @brief Starts all registered services by calling their init functions in separate threads.
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

    /**
     * @brief Stops the daemon by joining all service threads.
     */
    void stop() {
        logger->info("SNDSDaemon: Stopping all services...");
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
