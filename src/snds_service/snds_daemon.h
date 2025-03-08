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

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

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

    void sendDummyDatatoHTTP(){
        logger->info("Waiting for HTTP server to start...");

        net::io_context ioc;
        tcp::resolver resolver(ioc);
        bool serverReady = false;
        
        // Retry loop for up to 5 seconds
        for (int i = 0; i < 50; ++i) {
            try {
                auto const results = resolver.resolve("127.0.0.1", "8080");
                tcp::socket socket(ioc);
                net::connect(socket, results.begin(), results.end());
                serverReady = true;
                socket.shutdown(tcp::socket::shutdown_both);
                break;  // Server is ready, exit the loop
            } catch (const std::exception&) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait before retrying
            }
        }

        if (!serverReady) {
            logger->error("HTTP Server did not start within 5 seconds.");
            return;
        }
        
        logger->info("HTTP Server is ready. Sending test requests...");

        // Now we can safely send GET and POST requests
        try {
            tcp::socket socket(ioc);
            auto const results = resolver.resolve("127.0.0.1", "8080");
            net::connect(socket, results.begin(), results.end());

            // Prepare a GET request
            http::request<http::string_body> req{http::verb::get, "/", 11};
            req.set(http::field::host, "127.0.0.1");
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            http::write(socket, req);
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(socket, buffer, res);
            logger->info("HTTP GET response: {}", res.body());

            socket.shutdown(tcp::socket::shutdown_both);
        } catch (const std::exception& e) {
            logger->error("HTTP GET test failed: {}", e.what());
        }

        try {
            tcp::socket socket(ioc);
            auto const results = resolver.resolve("127.0.0.1", "8080");
            net::connect(socket, results.begin(), results.end());

            // Prepare a POST request
            std::string postBody = "test post data";
            http::request<http::string_body> req{http::verb::post, "/", 11};
            req.set(http::field::host, "127.0.0.1");
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "text/plain");
            req.body() = postBody;
            req.prepare_payload();

            http::write(socket, req);
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(socket, buffer, res);
            logger->info("HTTP POST response: {}", res.body());

            socket.shutdown(tcp::socket::shutdown_both);
        } catch (const std::exception& e) {
            logger->error("HTTP POST test failed: {}", e.what());
        }
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
