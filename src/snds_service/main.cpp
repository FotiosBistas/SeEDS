#include "snds_producer.h"
#include "snds_consumer.h"
#include "snds_daemon.h" 
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>

// Global atomic flag to signal shutdown.
std::atomic<bool> g_stop{false};

// Signal handler for SIGINT (Ctrl-C)
void signalHandler(int signal) {
    if(signal == SIGINT) {
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
    auto consumer = std::make_unique<SNDSConsumer>();

    // Add the services to the daemon.
    daemon.addService(std::move(producer));
    daemon.addService(std::move(consumer));

    // Start the daemon (and its services).
    daemon.start();

    std::cout << "Running... Press Ctrl-C to stop." << std::endl;
    
    // Wait until Ctrl-C is pressed.
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop the daemon gracefully.
    daemon.stop();
    return 0;
}
