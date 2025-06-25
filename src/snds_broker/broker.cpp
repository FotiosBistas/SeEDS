#include "broker.h"

std::atomic<bool> Broker::shouldShutdown{false};

Broker::Broker(
    const mongocxx::uri& uri,
    const spdlog::level::level_enum logLevel,
    const std::string& logFile,
    const std::string& brokerName,
    const std::string& ipAddress,
    const int          port
):ipAddress(ipAddress), port(port){

    // Logger initialization
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // Updated pattern includes [%n] to print the logger's name (i.e., serviceName)
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
    logger = std::make_shared<spdlog::logger>(brokerName, sinks.begin(), sinks.end());
    spdlog::register_logger(logger);
    logger->set_level(logLevel);

    mongocxx::client client(uri);
    this->client = std::move(client); 
}

Broker::~Broker() = default;


void Broker::listen(){
    std::signal(SIGINT, handleSigint);

    try{
        // Create a cpp-httplib server.
        httplib::Server svr;

        svr.Get("/state", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);
        });

        svr.Get("/temporal-query", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);
        });

        svr.Post("/add", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received /add request: {}", req.body);
            add(req.body);
            //if (result) {
            //    res.status = 200;
            //    res.set_content("{\"status\": \"success\"}", "application/json");
            //} else {
            //    res.status = 500;
            //    res.set_content("{\"status\": \"failed to insert\"}", "application/json");
            //}
        });

        // Start the HTTP loop on all interfaces, port 8080.
        std::thread httpThread([&](){
            logger->info("HTTP server listening on {}:{}", ipAddress, port);
            if (!svr.listen(ipAddress, port)) {
              logger->error("Failed to bind HTTP server");
            }
        });

        while(!shouldShutdown.load()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        logger->info("Shutdown signal received, stopping HTTP server...");
        svr.stop();
        httpThread.join();

    }catch(const std::exception& e){
        logger->info("HTTP Server Error: {}", e.what());
    };
}

void Broker::add(const std::string& body){
    try{
        auto jsonData = bsoncxx::from_json(body);
        auto view = jsonData.view();
        if(!view["type"] || !view["id"]){
            logger->error("Missing 'type' or 'id' in request body");
            return;
        }
        bsoncxx::document::element type(view["type"]);
        bsoncxx::document::element docId(view["id"]);

        logger->debug("Received type: {} and id: {}", type.get_value().get_string(), docId.get_value().get_string());

        bsoncxx::builder::basic::document doc_builder;

        doc_builder.append(
            bsoncxx::builder::basic::concatenate(
                jsonData.view()
        ));

        doc_builder.append(
            bsoncxx::builder::basic::kvp(
                "_id", 
                docId.get_value()
            ),
            bsoncxx::builder::basic::kvp(
                "timestamp", 
                bsoncxx::types::b_date(std::chrono::system_clock::now()
            )
        ));

    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        throw;
    }


    //auto collection = this->client["SeEDS"][type.key()];
    //auto result = collection.insert_one(doc_builder.view());
    //return result;
}

void Broker::get(){}

void Broker::remove(){}

void Broker::patch(){}



