#pragma once
/**
 * @file snds_daemon.hpp
 * @brief Process orchestrator for SNDS services.
 *
 * @details
 * The **SNDSDaemon** owns a set of ServiceBase-derived components (producer,
 * consumer, HTTP server, worker, etc.), takes care of logger setup, and
 * coordinates lifecycle:
 *  - **addService()** to register components,
 *  - **start()** to spawn each service’s `init()` loop on its own thread,
 *  - **stop()** to signal shutdown, close queues, and join threads.
 *
 * Typical usage:
 * @code{.cpp}
 * SNDSDaemon daemon(spdlog::level::debug, "daemon.log");
 * daemon.addService(std::make_unique<SNDSProducer>(...));
 * daemon.addService(std::make_unique<SNDSConsumer>(...));
 * daemon.start();
 * // ...
 * daemon.stop();
 * @endcode
*/
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "snds_service_base.hpp"
/**
 * @class SNDSDaemon
 * @brief Owns and runs multiple ServiceBase-derived services in parallel threads.
 *
 * @note Logging is initialized with both a colored console sink and a file sink.
 *       The daemon does **not** own OS process lifetime; callers should create a
 *       main loop (e.g., wait on a signal) and invoke @ref stop() for a
 *       graceful shutdown.
*/
class SNDSDaemon {
public:
    /**
     * @brief Construct the daemon and initialize logging.
     *
     * @param logLevel Minimum log level for the daemon logger.
     * @param logFile  Path to the daemon log file (created/append mode).
     *
     * @post A named logger "SNDSDaemon" is registered in spdlog and used by
     *       the daemon for lifecycle messages.
    */
    SNDSDaemon
    (
        spdlog::level::level_enum logLevel = spdlog::level::info,
        const std::string& logFile = "SNDSDaemon.log"
    )
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
     * @brief Register a service to be managed by the daemon.
     *
     * @param service Owning pointer to a @ref ServiceBase implementation.
     *
     * @note The service is stored and later started on its own thread by @ref start().
     *       Ownership is transferred to the daemon.
    */
    void addService(std::unique_ptr<ServiceBase> service) 
    {
        services.push_back(std::move(service));
    }
    /**
     * @brief Start all registered services concurrently.
     *
     * @details
     * Spawns one thread per service and invokes `init()` on each. Since service
     * loops are typically blocking (reactors), this method returns immediately
     * after threads are created.
     *
     * @warning Ensure services were added via @ref addService() before calling.
     *          Subsequent calls without stopping first may create duplicate
     *          threads for the same services.
    */
    void start() 
    {
        logger->info("SNDSDaemon: Starting all services...");
        for (auto& service : services) 
        {
            threads.emplace_back
            (
                [this, servicePtr = service.get()]() 
                {
                    servicePtr->init();
                }
            );
        }
        logger->info("SNDSDaemon: All services have been started.");
    }

    //void broadcastDummyData(){

    //    auto data1 = std::make_unique<SNDS_INTRA_MESSAGE>(
    //        "/Car1",
    //        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
    //        SNDS_CLIENT_TYPES::CONSUMER,
    //        "Car"
    //    ); 
    //    ServiceBase::v_to_consumer->writeMessage(std::move(data1));
    //    auto data2 = std::make_unique<SNDS_INTRA_MESSAGE>(
    //        "/Car1",
    //        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
    //        SNDS_CLIENT_TYPES::PRODUCER,
    //        "Car"
    //    ); 
    //    ServiceBase::v_to_producer->writeMessage(std::move(data2));
    //    auto data3 = std::make_unique<SNDS_INTRA_MESSAGE>(
    //        "/Car1/id=hey",
    //        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
    //        SNDS_CLIENT_TYPES::DIGITAL_TWIN,
    //        "Car"
    //    ); 
    //    ServiceBase::v_to_dt->writeMessage(std::move(data3));
    //    auto data4 = std::make_unique<SNDS_INTRA_MESSAGE>(
    //        "/Car1/type=car",
    //        SNDS_INTRA_MESSAGE_TYPE::BY_ID_RESPONSE,
    //        SNDS_CLIENT_TYPES::DIGITAL_TWIN,
    //        "Car"
    //    ); 
    //    ServiceBase::v_to_dt->writeMessage(std::move(data4));
    //}

    /**
     * @brief Stop all services and join their threads.
     *
     * @details
     * Calls `stop()` on each registered service (cooperative shutdown), then
     * shuts down all shared queues exposed by @ref ServiceBase, and finally
     * joins every service thread.
     *
     * @post No service threads remain joinable.
    */
    void stop() 
    {
        logger->info("SNDSDaemon: Stopping all services...");
        
        // Signal each service to stop.
        for (auto& service : services) 
        {
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
        for (auto& t : threads) 
        {
            if (t.joinable()) 
            {
                t.join();
            }
        }
        logger->info("SNDSDaemon: All services stopped.");
    }

private:
    /**
     * @brief Owned service instances managed by the daemon.
     *
     * @invariant Elements are unique_ptrs; ownership is exclusive to the daemon.
    */
    std::vector<std::unique_ptr<ServiceBase>> services;
    /**
     * @brief Threads running each service’s `init()` loop.
     *
     * @note Lifetimes are managed in @ref start() / @ref stop().
    */
    std::vector<std::thread> threads;
    /**
     * @brief Daemon-scoped logger (console + file sinks).
    */
    std::shared_ptr<spdlog::logger> logger;
};
