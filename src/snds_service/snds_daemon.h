#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>

class SNDSDaemon {
public:
    /**
     * @brief Initializes the daemon and all its services.
     */
    void init() {
        log("Initializing SNDSDaemon...");

        // Initialize services
        for (const auto& service : services) {
            service->init();
        }

        log("Initialization complete.");
    }

    /**
     * @brief Starts all services in separate threads.
     */
    void start() {
        log("Starting all services...");

        for (const auto& service : services) {
            threads.emplace_back(&ServiceBase::run, service.get());
        }

        log("All services started.");
    }

    /**
     * @brief Stops the daemon and waits for all threads to join.
     */
    void stop() {
        log("Stopping SNDSDaemon...");

        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        log("All services stopped.");
    }

    /**
     * @brief Adds a service to the daemon.
     * @param service The service to add (inherits from ServiceBase).
     */
    void addService(std::unique_ptr<ServiceBase> service) {
        services.push_back(std::move(service));
    }

private:
    std::vector<std::unique_ptr<ServiceBase>> services;
    std::vector<std::thread> threads;
    std::mutex logMutex;

    /**
     * @brief Thread-safe logging utility.
     * @param message The message to log.
     */
    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::cout << "[SNDSDaemon] " << message << std::endl;
    }
};
