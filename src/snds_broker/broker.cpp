#include "broker.h"

std::atomic<bool> Broker::shouldShutdown{false};

Broker::Broker(
    const mongocxx::uri& uri,
    const std::string& database,
    const spdlog::level::level_enum logLevel,
    const std::string& logFile,
    const std::string& brokerName,
    const std::string& ipAddress,
    const int          port
):ipAddress(ipAddress), port(port), database(database){

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

            auto id         = req.get_param_value("id");
            auto type       = req.get_param_value("type");
            auto count      = req.get_param_value("count");
            auto date_str   = req.get_param_value("date");
            auto timeframe  = req.get_param_value("timeframe");

            if (id.empty() || type.empty()) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'id' or 'type'})" "\n", "application/json");
                return;
            }

            std::string json_request = fmt::format(
                R"({{"id": "{}", "date": "{}", "type": "{}", "timeframe": "{}", "count": {}}})",
                id, date_str, type, timeframe,
                count.empty() ? 0 : std::stoi(count)
            );

            logger->info("Received: {}", json_request);
            auto results = get(json_request);

            std::ostringstream oss;
            oss << "{ \"results\": [";
            for (size_t i = 0; i < results.size(); ++i) {
                oss << results[i];
                if (i < results.size() - 1)
                    oss << ",";
            }
            oss << "] }\n";

            res.status = 200;
            res.set_content(oss.str(), "application/json");
        });

        svr.Post("/add", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received /add request: {}", req.body);
            auto result_versioned    = add_one(req.body, true);
            auto result_unversioned  = add_one(req.body, false);
            if (result_versioned && result_unversioned) {

                size_t count_versioned = 0;
                size_t count_unversioned = 0;

                // Extract count for versioned (insert_one)
                if (std::holds_alternative<mongocxx::result::insert_one>(*result_versioned)) {
                    count_versioned = std::get<mongocxx::result::insert_one>(*result_versioned).result().inserted_count();
                }

                // Extract count for unversioned (replace_one)
                if (std::holds_alternative<mongocxx::result::replace_one>(*result_unversioned)) {
                    auto& repl = std::get<mongocxx::result::replace_one>(*result_unversioned).result();
                    count_unversioned = repl.upserted_ids().size() == 0 ? repl.modified_count() : repl.upserted_ids().size();
                }

                logger->info("Inserted {} versioned, replaced/upserted {} non-versioned", count_versioned, count_unversioned);
                res.status = 200;
                std::string response_json = fmt::format(
                    "{{\"status\": \"success\", \"inserted_count_versioned\": {}, \"upserted_or_modified_count\": {}}}\n",
                    count_versioned, count_unversioned
                );
                res.set_content(response_json, "application/json");
            } else {
                logger->error("Insert failed with no result returned.");
                res.status = 500;
                res.set_content("{\"status\": \"failed to insert\"}\n", "application/json");
            }
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

std::optional<InsertOrReplaceResult> Broker::add_one(const std::string& body, const bool versioned){
    try{
        auto jsonData = bsoncxx::from_json(body);
        auto view = jsonData.view();
        if(!view["type"] || !view["id"]){
            logger->error("Missing 'type' or 'id' in request body");
            return std::nullopt;
        }
        bsoncxx::document::element type(view["type"]);
        bsoncxx::document::element docId(view["id"]);

        logger->debug("Received type: {} and id: {}", type.get_value().get_string(), docId.get_value().get_string());

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::string timestamp_str = std::to_string(timestamp);


        bsoncxx::builder::basic::document id_builder;
        if (versioned) {
            id_builder.append(
                bsoncxx::builder::basic::kvp("_id", docId.get_value()),
                bsoncxx::builder::basic::kvp("timestamp", timestamp_str)
            );
        }         

        bsoncxx::builder::basic::document doc_builder;
        if(versioned){
            doc_builder.append(
                bsoncxx::builder::basic::kvp("_id", id_builder.view())
            );
        }else{
            doc_builder.append(
                bsoncxx::builder::basic::kvp("_id", docId.get_value())
            );
        }

        for(auto&& element: view){
            if(element.key() != "type" && element.key() != "id"){
                doc_builder.append(
                    bsoncxx::builder::basic::kvp(element.key(), element.get_value())
                );
            }
        }

        logger->debug("Constructed ID document: {}", bsoncxx::to_json(doc_builder));

        // helps you retrieve versions
        if(versioned){
            auto type_str_version = std::string(type.get_string().value) + "_VERSIONED";
            auto collection = this->client[database][type_str_version];
            auto result = collection.insert_one(doc_builder.extract());
            return result;
        }

        auto filter = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("_id", docId.get_value())
        );

        logger->debug("Constructed filter document: {}", bsoncxx::to_json(filter));

        // helps you retrieve the most recent version
        auto type_str = type.get_string().value;
        auto collection = this->client[database][type_str];
        auto result = collection.replace_one(
            filter.view(),
            doc_builder.view(),
            mongocxx::options::replace{}.upsert(true)
        );
        return result;
    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        return std::nullopt;
    }
}

std::vector<std::string> Broker::get(const std::string& request){
    constexpr const char* delimeter = "+";
    try{
        auto jsonData = bsoncxx::from_json(request);
        auto view = jsonData.view();
        if(!view["type"] || !view["id"]){
            logger->error("Missing 'type' or 'id' in request body");
            return {};
        }
        bsoncxx::document::element timeframe(view["timeframe"]);

        auto timeframe_view = timeframe.get_value().get_string().value;
        auto split = [](std::string_view s, const std::string& delimiter) -> std::vector<std::string> {
            std::vector<std::string> tokens;
            size_t start = 0;
            size_t end;
        
            while ((end = s.find(delimiter, start)) != std::string::npos) {
                tokens.emplace_back(s.substr(start, end - start));
                start = end + delimiter.length();
            }
            // Add the last token missed from the loop
            tokens.emplace_back(s.substr(start));
        
            return tokens;
        };

        auto request = [this](const Timeframe& frame, bsoncxx::v_noabi::document::view json_data) -> std::string {

            std::string_view type = json_data["type"].get_value().get_string().value;
            std::string_view id = json_data["id"].get_value().get_string().value;

            logger->info("Processsing ID: {}, and type: {}", id, type);
            switch(frame){
                case Timeframe::NOW: {
                    auto collection = this->client[this->database][type];
                    auto search = bsoncxx::builder::basic::make_document(
                        bsoncxx::builder::basic::kvp("_id", id)
                    );
                    auto result = collection.find_one(search.view());
                    if(result){
                        logger->info("Received document from Timeframe::Now: {}", bsoncxx::to_json(*result));
                        return bsoncxx::to_json(*result);
                    }else{
                        logger->error("Could not retrieve ID from the database..."); 
                        return "";
                    }
                }
                case Timeframe::BEFORE:
                    return "";
                case Timeframe::AFTER:
                    return "";
            }
        };

        auto tokens = split(timeframe_view, delimeter);

        std::vector<std::string> results; 
        results.reserve(tokens.size());

        for(auto& token: tokens){
            logger->debug("Processing timeframe: {}", token);
            Timeframe frame = string_to_timeframe(token);
            auto result = request(frame, view);
            results.push_back(result);
        }

        return results;

    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        return {};
    }
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void Broker::remove(){}

void Broker::patch(){}



