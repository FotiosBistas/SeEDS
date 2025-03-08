#include "snds_http_service.h"



void SNDSHTTPService::init(){
    m_logger->info("HTTP SERVER: Starting service: {}", serviceName);
    try {
        // Create an io_context.
        net::io_context ioc{1};

        // Set up a TCP endpoint on port 8080.
        tcp::endpoint endpoint(tcp::v4(), 8080);
        tcp::acceptor acceptor(ioc, endpoint);

        // Lambda that will asynchronously accept connections.
        std::function<void()> do_accept;
        do_accept = [&]() {
            acceptor.async_accept(
                [&](beast::error_code ec, tcp::socket socket) {
                    if (!ec) {
                        // For each new connection, create a session.
                        std::make_shared<session>(std::move(socket), m_logger, m_ackTag)->run();
                    } else {
                        m_logger->error("Accept error: {}", ec.message());
                    }
                    if (running.load()) { // Continue accepting if not shutting down.
                        do_accept();
                    }
                });
        };

        do_accept();  // Start the asynchronous accept loop.

        // Create a thread pool to run the io_context.
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 2;
        std::vector<std::thread> threadPool;
        for (unsigned int i = 0; i < numThreads; ++i) {
            threadPool.emplace_back([&ioc]() { ioc.run(); });
        }

        // Wait until the service is signaled to shut down.
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        ioc.stop();
        for (auto& t : threadPool) {
            if (t.joinable())
                t.join();
        }
        m_logger->info("HTTP SERVER: Shutting down.");
    } catch (const std::exception& e) {
        m_logger->error("HTTP SERVER: error {}", e.what());
    }

}


void SNDSHTTPService::session::handle_request(){
    // Log the request method and target.
    std::string method = std::string(request_.method_string());
    std::string target = std::string(request_.target());
    std::string body = std::string(request_.body());
    logger_->info("HTTP SERVER: Received {} request for {} with body {}", method, target);

    // Prepare an HTTP response.
    http::response<http::string_body> response{http::status::ok, request_.version()};
    response.set(http::field::server, "SNDSHTTPService");
    response.set(http::field::content_type, "text/plain");
    response.body() = sessionAckTag;
    response.prepare_payload();


    if (request_.method() == http::verb::get){
        logger_->info("HTTP SERVER: Received GET request from {}", socket_.remote_endpoint().address().to_string());
        std::unique_ptr<SNDS_INTRA_MESSAGE> message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(target),
            SNDS_INTRA_MESSAGE_TYPE::GENERIC,
            SNDS_CLIENT_TYPES::HTTP_SRV,
            ""
        );
        logger_->debug("HTTP SERVER: Writing message {} from GET request to service queue", message.get()->printDetails());
        v_to_service.get()->writeMessage(std::move(message));
    } else if(request_.method() == http::verb::post){
        logger_->info("HTTP SERVER: Received POST request from {}", socket_.remote_endpoint().address().to_string());
        std::unique_ptr<SNDS_INTRA_MESSAGE> message = std::make_unique<SNDS_INTRA_MESSAGE>(
            std::move(body),
            SNDS_INTRA_MESSAGE_TYPE::GENERIC,
            SNDS_CLIENT_TYPES::HTTP_SRV,
            ""
        );
        logger_->debug("HTTP SERVER: Writing message {} from POST request to service queue", message.get()->printDetails());
        v_to_service.get()->writeMessage(std::move(message));
    } else {
        logger_->error("HTTP SERVER: Unsupported HTTP method received.");
    }

    auto self = shared_from_this();
    http::async_write(socket_, response,
        [this, self](beast::error_code ec, std::size_t) {
            // Shutdown the socket after sending the response.
            socket_.shutdown(tcp::socket::shutdown_send, ec);
            if (ec)
                logger_->error("HTTP SERVER: Session shutdown error: {}", ec.message());
        });

}