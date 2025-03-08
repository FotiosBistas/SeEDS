#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

void testHTTPServer() {
    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        // Resolve "127.0.0.1" at port "8080"
        auto const results = resolver.resolve("127.0.0.1", "8080");
        tcp::socket socket(ioc);
        net::connect(socket, results.begin(), results.end());

        // Prepare a GET request targeting the root resource.
        http::request<http::string_body> req{http::verb::get, "/", 11};
        req.set(http::field::host, "127.0.0.1");
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the GET request
        http::write(socket, req);

        // Read the response
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(socket, buffer, res);

        logger->info("HTTP GET response: {}", res.body());

        // Shutdown the socket for the GET request.
        socket.shutdown(tcp::socket::shutdown_both);
    } catch (const std::exception& e) {
        logger->error("HTTP GET test failed: {}", e.what());
    }

    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve("127.0.0.1", "8080");
        tcp::socket socket(ioc);
        net::connect(socket, results.begin(), results.end());

        // Prepare a POST request with a simple body.
        std::string postBody = "test post data";
        http::request<http::string_body> req{http::verb::post, "/", 11};
        req.set(http::field::host, "127.0.0.1");
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "text/plain");
        req.body() = postBody;
        req.prepare_payload();

        // Send the POST request.
        http::write(socket, req);

        // Read the response.
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(socket, buffer, res);

        logger->info("HTTP POST response: {}", res.body());

        socket.shutdown(tcp::socket::shutdown_both);
    } catch (const std::exception& e) {
        logger->error("HTTP POST test failed: {}", e.what());
    }
}

int main(){
    testHTTPServer();
}