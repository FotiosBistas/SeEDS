#ifndef SNDS_HTTP_SERVICE_H
#define SNDS_HTTP_SERVICE_H

#include "snds_service_base.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <functional>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

class SNDSHTTPService : public ServiceBase {
public:
    SNDSHTTPService(
            spdlog::level::level_enum logLevel = spdlog::level::info,
            std::string logFile = "HTTPServer.log",
            uint8_t numberOfSndsSrvs = 1,
            std::string sndsPrefix = "/SNDS",
            std::string mySndsId = "0",
            std::string idScopePrefix = "/SNDS_ID_scope_",
            std::string registryFilePostfix = "_registry",
            std::string serviceName = "HTTPServer")
    : ServiceBase(logLevel,
                  logFile,
                  numberOfSndsSrvs,
                  sndsPrefix,
                  mySndsId,
                  idScopePrefix,
                  registryFilePostfix,
                  "THIS_IS_AN_SNDS_ACK",  // Pass the ackTag explicitly
                  serviceName)            // Then pass the serviceName
    {}

    /**
     * @brief Starts the asynchronous HTTP server.
     *
     * Overrides the init() method from ServiceBase. This method creates an io_context,
     * a TCP acceptor listening on port 8080, and runs a thread pool to handle incoming
     * connections asynchronously. It will exit once the shutdown flag (running) is set to false.
     */
    void init();

private:
    /**
     * @brief Session class: Handles a single HTTP connection.
     *
     * Each session reads an HTTP request asynchronously, logs the method and target,
     * sends back a simple response, and then shuts down the connection.
     */
    class session : public std::enable_shared_from_this<session> {
    public:
        session(tcp::socket socket, std::shared_ptr<spdlog::logger> logger, const std::string& ackTag)
            : socket_(std::move(socket)), logger_(logger), sessionAckTag(ackTag) {}

        void run() {
            do_read();
        }

    private:
        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;
        std::shared_ptr<spdlog::logger> logger_;
        std::string sessionAckTag;

        // Asynchronously read an HTTP request.
        void do_read() {
            auto self = shared_from_this();
            http::async_read(socket_, buffer_, request_,
                [this, self](beast::error_code ec, std::size_t) {
                    if (!ec) {
                        handle_request();
                    } else {
                        logger_->error("HTTP SERVER: Session read error: {}", ec.message());
                    }
                });
        }

        // Handle the request and send a response.
        void handle_request(); 
    };
};

#endif // SNDS_HTTP_SERVICE_H
