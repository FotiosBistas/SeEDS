#include "snds_http_service.h"
#include "httplib.h" // Use the header-only library

void SNDSHTTPService::init() {
    m_logger->info("HTTP SERVER: Starting service: {}", serviceName);
    try {
        // Create a cpp-httplib server.
        httplib::Server svr;

        // Handle GET requests at "/"
        svr.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
            m_logger->info("Received GET request for {} with body {}", req.path, req.body);
            m_logger->info("Received GET request for {} with target {}", req.path, req.target);
            m_logger->info("Received GET request from {}", req.remote_addr);
            // Create a new internal message based on the request target.
            auto outbound_message = std::make_unique<SNDS_INTRA_MESSAGE>
            (
                std::move(std::string(req.target)),
                SNDS_INTRA_MESSAGE_TYPE::GENERIC,
                SNDS_CLIENT_TYPES::HTTP_SRV,
                ""
            );

            m_logger->debug("Writing message from GET request to service queue...\n{}", outbound_message->printDetails());
            v_to_worker.get()->writeMessage(std::move(outbound_message));

            auto inbound_message = v_to_service->readMessage();
            if (!inbound_message) {
                m_logger->debug("Queue shutdown detected, returning HTTP 500");
                res.status = 500;
                res.set_content("Internal Server Error", "text/plain");
                return;
            }

            auto payload = inbound_message->getMessage();
            auto pos = payload.find_first_of("{[");
            if (pos != std::string::npos) {
                payload = payload.substr(pos);
            }

            res.status = 200;
            res.set_content(payload, "text/plain");
        });

        // Handle POST requests at "/"
        svr.Post("/", [this](const httplib::Request& req, httplib::Response& res) {
            m_logger->info("Received POST request for {} with body {}", req.path, req.body);
            m_logger->info("Received POST request from {}", req.remote_addr);
            // Create a new internal message based on the request body.
            std::unique_ptr<SNDS_INTRA_MESSAGE> message = std::make_unique<SNDS_INTRA_MESSAGE>(
                std::move(std::string(req.body)),
                SNDS_INTRA_MESSAGE_TYPE::GENERIC,
                SNDS_CLIENT_TYPES::HTTP_SRV,
                ""
            );
            m_logger->debug("Writing message from POST request to service queue...\n{}", message->printDetails());
            v_to_dt.get()->writeMessage(std::move(message));
            res.set_content(m_ackTag, "text/plain");
        });

        // Run the server in a separate thread so that we can check for shutdown.
        std::thread serverThread([this, &svr]() {
            m_logger->info("HTTP SERVER: Listening on port 8080...");
            svr.listen("0.0.0.0", 8080);
        });

        // Main loop waiting for shutdown signal.
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Stop the server and join the thread.
        svr.stop();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        m_logger->info("HTTP SERVER: Shutting down.");
    } catch (const std::exception& e) {
        m_logger->error("HTTP SERVER: error {}", e.what());
    }
}
