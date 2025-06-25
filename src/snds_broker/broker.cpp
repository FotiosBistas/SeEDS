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
    //this->database = client["SeEDS"];
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
            throw std::runtime_error("Missing 'type' or 'id' in request body");
        }
        bsoncxx::document::element type(view["type"]);
        bsoncxx::document::element docId(view["id"]);

        logger->debug("Received type: {} and id: {}", type.get_value().get_string(), docId.get_value().get_string());

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::string timestamp_str = std::to_string(timestamp);


        // Construct compound _id { id: ..., timestamp: ... }
        auto compound_id = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("id", docId.get_value()),
            bsoncxx::builder::basic::kvp("timestamp", timestamp_str)
        );
        

        bsoncxx::builder::basic::document doc_builder;

        doc_builder.append(
            bsoncxx::builder::basic::kvp("_id", compound_id.view())
        );

        for(auto&& element: view){
            if(element.key() != "type" && element.key() != "id"){
                doc_builder.append(
                    bsoncxx::builder::basic::kvp(element.key(), element.get_value())
                );
            }
        }

        logger->debug("Constructed document: {}", bsoncxx::to_json(doc_builder));

        //auto collection = this->client["SeEDS"][type.key()];
        //auto result = collection.update_one(
        //    filter.view(), 
        //    update.view(), 
        //    mongocxx::options::update{}.upsert(true)
        //);
        //if (result && result->modified_count() + result->upserted_count() == 1) {
        //    logger->info("Document for '{}' updated or inserted successfully.", id);
        //} else {
        //    logger->warn("No document modified or inserted.");
        //}
        //return result;
    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        throw e;
    }
}

void Broker::get(){}

void Broker::remove(){}

void Broker::patch(){}



