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

std::vector<std::string> Broker::split(std::string_view s, const std::string& delimiter){
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


std::pair<std::chrono::_V2::system_clock::time_point, bsoncxx::types::b_date> Broker::parse_timestamp(std::string_view date){
    //TODO this is not robsut
    constexpr const std::array<std::string_view, 4> formats = {
        "%Y-%m-%d %H:%M:%S",     // e.g., 2025-06-26 14:30:00
        "%Y-%m-%dT%H:%M:%SZ",    // e.g., 2025-06-26T14:30:00Z
        "%Y-%m-%dT%H:%M:%S",     // e.g., 2025-06-26T14:30:00
        "%Y-%m-%d"               // e.g., 2025-06-26
    };
    std::tm tm = {};
    std::istringstream ss(date.data());
    bool parsed = false;
    for (const auto& fmt : formats) {
        ss.clear();
        ss.str(date.data());
        ss >> std::get_time(&tm, fmt.data());
        if (!ss.fail()) {
            parsed = true;
            break;
        }
    }

    logger->debug("Transformed to date type: {}", ss.str());

    auto target_tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    auto target_date = bsoncxx::types::b_date(target_tp);

    return {target_tp, target_date};
}


mongocxx::pipeline Broker::create_date_pipeline(
    const Timeframe& frame,
    const size_t count,
    std::string_view id,
    const std::chrono::_V2::system_clock::time_point& base_date,
    const bsoncxx::types::b_date& parsed_date
){

    mongocxx::pipeline pipeline;

    switch(frame){
        case Timeframe::NOW: {
            pipeline.match(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id._id", id)
                )
            );
            pipeline.add_fields(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("diff",
                        bsoncxx::builder::basic::make_document(
                            bsoncxx::builder::basic::kvp("$abs",
                                bsoncxx::builder::basic::make_document(
                                    bsoncxx::builder::basic::kvp("$subtract",
                                        bsoncxx::builder::basic::make_array(
                                            "$_id.timestamp",
                                            parsed_date
                                        )
                                    )
                                )
                            )
                        )
                    )
                )
            );
            pipeline.sort(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("diff", 1)
                )
            );
            pipeline.limit(count);
            break;
        }
        case Timeframe::BEFORE: {
            pipeline.match(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id._id", id)
                )
            );
            pipeline.match(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id.timestamp",
                        bsoncxx::builder::basic::make_document(
                            bsoncxx::builder::basic::kvp("$lte", parsed_date)
                        )
                    )
                )
            );
            pipeline.sort(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id.timestamp", 1)
                )
            );
            pipeline.limit(count);
            break;
        }
        case Timeframe::AFTER: {
            pipeline.match(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id._id", id)
                )
            );
            pipeline.match(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id.timestamp",
                        bsoncxx::builder::basic::make_document(
                            bsoncxx::builder::basic::kvp("$gte", parsed_date)
                        )
                    )
                )
            );
            pipeline.sort(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("_id.timestamp", 1)
                )
            );
            pipeline.limit(count);
            break;
        }
    }

    return pipeline;
}

std::string Broker::get_from_database(const Timeframe& frame, bsoncxx::v_noabi::document::view json_data){

    //type and ID are always specified by the get handler
    std::string_view type = json_data["type"].get_value().get_string().value;
    std::string_view id = json_data["id"].get_value().get_string().value;

    //date might not be specified
    std::string_view date;
    if(!json_data["date"].get_value().get_string().value.empty()){
        date = std::move(json_data["date"].get_value().get_string().value);
    }else{
        date = "";
    }

    int32_t count = json_data["count"].get_value().get_int32();

    logger->info("Processing ID: {}, and type: {}", id, type);


    std::string correct_type = std::string(type.data());

    if(!date.empty()){
        correct_type = std::string(type.data()) + "_VERSIONED";
        auto collection = this->client[this->database][correct_type];
        auto [base_timestamp, parsed_timestamp] = parse_timestamp(date);
        auto pipeline = create_date_pipeline(frame, count, id, base_timestamp, parsed_timestamp);
        auto cursor = collection.aggregate(pipeline);

        std::ostringstream oss;
        for (auto&& doc : cursor) {
            logger->debug("Reading document from database: {}", bsoncxx::to_json(doc));
            oss << bsoncxx::to_json(doc) << "\n";
        }

        std::string flat_json = oss.str();
        logger->debug("Flat JSON created:\n{}", flat_json);
        return flat_json;
    }

    auto collection = this->client[this->database][correct_type];

    auto search = bsoncxx::builder::basic::make_document(
        bsoncxx::builder::basic::kvp("_id", id)
    );
    auto result = collection.find_one(search.view());
    if(result){
        logger->info("Received document from database: {}", bsoncxx::to_json(*result));
        return bsoncxx::to_json(*result);
    }else{
        logger->error("Could not retrieve ID from the database..."); 
        return "";
    }
};

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
        bsoncxx::types::b_date bson_date(now);

        bsoncxx::builder::basic::document id_builder;
        if (versioned) {
            id_builder.append(
                bsoncxx::builder::basic::kvp("_id", docId.get_value()),
                bsoncxx::builder::basic::kvp("timestamp", bson_date)
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
            if(element.key() != "type"){
                doc_builder.append(
                    bsoncxx::builder::basic::kvp(element.key(), element.get_value())
                );
            }
        }
        // insert timestamp anyway
        doc_builder.append(
            bsoncxx::builder::basic::kvp("timestamp", bson_date)
        );

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
        auto tokens = split(timeframe_view, delimeter);

        std::vector<std::string> results; 
        results.reserve(tokens.size());

        for(auto& token: tokens){
            logger->debug("Processing timeframe: {}", token);
            Timeframe frame = string_to_timeframe(token);
            auto result = get_from_database(frame, view);
            results.push_back(result);
        }

        return results;

    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        return {};
    }
}


void Broker::remove(){}

void Broker::patch(){}



