#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>

#include "snds_consumer.hpp"
#include "snds_http_server.hpp"
#include "snds_producer.hpp"
#include "snds_digital_twin.hpp"
#include "snds_worker.hpp"
#include "snds_daemon.hpp"

// Global atomic flag to signal shutdown.
std::atomic<bool> g_stop{false};

// Signal handler for SIGINT (Ctrl-C)
void signalHandler(int signal) 
{
    if(signal == SIGINT) 
    {
        g_stop.store(true);
    }
}

int main() {
    // Install the signal handler for Ctrl-C
    std::signal(SIGINT, signalHandler);

    // Create a daemon instance.
    SNDSDaemon daemon;

    // Create a producer and consumer instance.
    auto producer = std::make_unique<SNDSProducer>(spdlog::level::debug);
    auto consumer = std::make_unique<SNDSConsumer>(spdlog::level::debug);
    auto dt = std::make_unique<DigitalTwin>(spdlog::level::debug);
    auto http = std::make_unique<SNDSHTTPService>(spdlog::level::debug);
    auto worker = std::make_unique<Worker>(spdlog::level::debug);

    // Add the services to the daemon.
    daemon.addService(std::move(producer));
    daemon.addService(std::move(consumer));
    daemon.addService(std::move(dt));
    daemon.addService(std::move(http));
    daemon.addService(std::move(worker));

    // Start the daemon (and its services).
    daemon.start();

    std::cout << "Running... Press Ctrl-C to stop." << std::endl;
    
    // daemon.broadcastDummyData();
    // Wait until Ctrl-C is pressed.
    while (!g_stop.load()) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop the daemon gracefully.
    daemon.stop();
    return 0;
}
