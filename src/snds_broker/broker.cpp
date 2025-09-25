#include "broker.hpp"

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
            auto type = req.get_param_value("type");
            if (type.empty()) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Missing required query parameter 'type'"})" "\n",
                    "application/json"
                );
                return;
            }
        
            std::string json_request = fmt::format(R"({{"type": "{}"}})", type);
            std::string body = state(json_request);     // <- body will already be JSON
        
            if (body.empty()) {
                res.status = 500;
                res.set_content(R"({"error":"Failed to compute state"})" "\n", "application/json");
                return;
            }
            res.status = 200;
            res.set_content(body + "\n", "application/json");
        });

        svr.Post("/copy", [this](const httplib::Request& req, httplib::Response& res) {
            logger->info("Received /copy request (len={} bytes)", req.body.size());
        
            nlohmann::json body;
            try {
                body = nlohmann::json::parse(req.body);
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(
                    nlohmann::json{{"status","error"},{"message",std::string("invalid JSON: ")+e.what()}}.dump(2),
                    "application/json"
                );
                return;
            }

            logger->debug("Received body: {}", body.dump(2));
        
            const bool mirror_to_secondary = body.value("mirror_to_secondary", true);
            logger->debug("Mirror to secondary: {}", mirror_to_secondary);
            const std::string suffix = body.value("secondary_suffix", std::string("_SECONDARY"));
        
            if (!body.contains("collections") || !body["collections"].is_object()) {
                res.status = 400;
                res.set_content(nlohmann::json{
                    {"status","error"},
                    {"message","missing 'collections' object"}
                }.dump(2), "application/json");
                return;
            }
        
            auto ends_with = [](const std::string& s, const std::string& suf){
                return s.size() >= suf.size() && s.compare(s.size()-suf.size(), suf.size(), suf) == 0;
            };
        
            auto is_versioned_name = [&](const std::string& name){
                return ends_with(name, std::string("_VERSIONED"));
            };
        
            nlohmann::json summary = nlohmann::json::object();
            size_t inserted = 0, replaced = 0, failed = 0, skipped = 0;
        
            auto db = this->client[this->database];
        
            for (auto& kv : body["collections"].items()) {
                const std::string source_name = kv.key();
                const nlohmann::json& docs    = kv.value();
            
                if (!docs.is_array()) { skipped++; continue; }
            
                // Decide target collection name
                const std::string target_name = mirror_to_secondary
                    ? (source_name + suffix)          // e.g. CAR -> CAR_SECONDARY, CAR_VERSIONED -> CAR_VERSIONED_SECONDARY
                    : source_name;                    // exact same name (1:1 copy)
                
                logger->debug("Created target name to copy: {}...", target_name);
                auto target = db[target_name];
            
                logger->info("[/copy] {} -> {} ({} docs)", source_name, target_name, docs.size());
            
                const bool versioned = is_versioned_name(source_name);

                logger->debug("Is versioned: {}", versioned);
            
                for (const auto& jdoc : docs) {
                    if (!jdoc.is_object()) { skipped++; continue; }
                
                    try {
                        // Re-hydrate BSON from extended JSON exactly as dumped
                        auto bval = bsoncxx::from_json(jdoc.dump());
                        auto view = bval.view();
                    
                        if (versioned) {
                            // Versioned collections use an embedded _id with { _id: <id>, timestamp: <date> }
                            auto r = target.insert_one(view);
                            if (r && r->result().inserted_count() == 1) inserted++;
                            else failed++;
                        } else {
                            // Unversioned: replace/upsert by string _id
                            auto id_el = view["_id"];
                            if (!id_el || id_el.type() != bsoncxx::type::k_string) {
                                logger->warn("[/copy] unversioned doc missing string _id, skipping: {}", jdoc.dump());
                                skipped++;
                                continue;
                            }
                            auto filter = bsoncxx::builder::basic::make_document(
                                bsoncxx::builder::basic::kvp("_id", id_el.get_string().value)
                            );
                        
                            auto r = target.replace_one(
                                filter.view(),
                                view,
                                mongocxx::options::replace{}.upsert(true)
                            );
                            if (r) {
                                // Upsert => either modified or upserted
                                if (r->result().upserted_ids().size() > 0 || r->result().modified_count() > 0)
                                    replaced++;
                                else
                                    // If nothing changed (identical), count as replaced to be generous
                                    replaced++;
                            } else {
                                failed++;
                            }
                        }
                    } catch (const std::exception& e) {
                        failed++;
                        logger->warn("[/copy] failed doc in {} -> {}: {}", source_name, target_name, e.what());
                    }
                }
            }
        
            logger->debug("Sending summary back to client: {}", summary.dump());
            summary["status"]   = "ok";
            summary["inserted"] = inserted;
            summary["replaced"] = replaced;
            summary["skipped"]  = skipped;
            summary["failed"]   = failed;
        
            res.status = 200;
            res.set_content(summary.dump(2), "application/json");
        });

        svr.Get("/temporal-query", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);

            auto id         = req.get_param_value("id");
            auto type       = req.get_param_value("type");
            auto count      = req.get_param_value("count");
            auto date_str   = req.get_param_value("date");
            auto timeframe  = req.get_param_value("timeframe");

            if (id.empty()) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'id'})" "\n", "application/json");
                return;
            }


            if(type.empty()){
                logger->warn("Missing 'type' in /get body, defaulting to 'GENERIC'");
                type = "GENERIC";
            }

            std::string json_request = fmt::format(
                R"({{"id": "{}", "date": "{}", "type": "{}", "timeframe": "{}", "count": {}}})",
                id, date_str, type, timeframe,
                count.empty() ? 0 : std::stoi(count)
            );

            logger->info("Received: {}", json_request);
            auto results = get(json_request);

            res.status = 200;
            res.set_content(results.dump(2), "application/json");
        });

        svr.Get("/dump", [this](const httplib::Request& req, httplib::Response& res)
        {
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);
        
            // Optional: dump a single collection with ?type=CollectionName
            std::string type = req.get_param_value("type");
        
            nlohmann::json result;
            if (!type.empty()) 
            {
                result = dump_database(type);
            } 
            else 
            {
                result = dump_database(); // all collections
            }
        
            // Simple success / error handling
            if (result.is_object() && result.contains("status") && result["status"] == "error") 
            {
                res.status = 500;
                res.set_content(result.dump(2) + "\n", "application/json");
                return;
            }
        
            res.status = 200;
            res.set_content(result.dump(2) + "\n", "application/json");
        });


        svr.Get("/get", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received HTTP request for {}, with target {}", req.body, req.target);

            auto id         = req.get_param_value("id");
            auto type       = req.get_param_value("type");

            if (id.empty()) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'id'})" "\n", "application/json");
                return;
            }

            if (type.empty()) {
                logger->warn("Missing 'type' in /get body, defaulting to 'GENERIC'");
                type = "GENERIC";
            }

            std::string json_request = fmt::format(
                R"({{"id": "{}", "type": "{}"}})",
                id, type
            );

            logger->info("Received: {}", json_request);
            auto results = get_one(json_request);

            res.status = 200;
            res.set_content(results.dump(2), "application/json");
        });

        svr.Post("/add", [this](const httplib::Request& req, httplib::Response& res){
            logger->info("Received /add request: {}", req.body);
            // Parse and ensure a default type
            nlohmann::json body;
            try {
                body = nlohmann::json::parse(req.body);
            }
            catch (const std::exception& e) {
                logger->error("Invalid JSON in /add: {}", e.what());
                res.status = 400;
                res.set_content(R"({"status":"error","message":"invalid JSON"})", "application/json");
                return;
            }

            if (!body.contains("type") || body["type"].get<std::string>().empty()) {
                logger->warn("Missing 'type' in /add body, defaulting to 'GENERIC'");
                body["type"] = "GENERIC";
            }


            auto payload = body.dump(2);
            auto result_versioned    = add_one(payload, true);
            auto result_unversioned  = add_one(payload, false);
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
        if(!view["id"]){
            logger->error("Missing 'id' in request body");
            return std::nullopt;
        }

        bsoncxx::document::element type(view["type"]);
        bsoncxx::document::element docId(view["id"]);

        logger->debug("Received type: {} and id: {} inside", type.get_value().get_string(), docId.get_value().get_string());

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
            doc_builder.append(
                bsoncxx::builder::basic::kvp(element.key(), element.get_value())
            );
        }
        // insert timestamp anyway
        doc_builder.append(
            bsoncxx::builder::basic::kvp("timestamp", bson_date)
        );

        logger->debug("Constructed ID document: {}", bsoncxx::to_json(doc_builder));

        if (versioned) {
            std::string type_str = std::string(type.get_string().value);
        
            // If the incoming type already carries the SECONDARY suffix, strip it for now.
            bool is_secondary = false;
            if (type_str.size() >= std::strlen(secondary_suffix.data()) &&
                type_str.compare(type_str.size() - std::strlen(secondary_suffix.data()),
                                 std::strlen(secondary_suffix.data()),
                                 secondary_suffix) == 0)
            {
                is_secondary = true;
                type_str.erase(type_str.size() - std::strlen(secondary_suffix.data())); // drop trailing "_SECONDARY"
            }
        
            // Always form TYPE_VERSIONED, then (optionally) append _SECONDARY
            std::string collection_name = type_str + "_VERSIONED";
            if (is_secondary) collection_name += "_SECONDARY";
        
            logger->debug("Created type version: {}", collection_name);
            auto collection = this->client[database][collection_name];
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

nlohmann::json Broker::get_one(const std::string& request){
    try{
        auto json_data = bsoncxx::from_json(request);
        auto view = json_data.view();
        if(!view["id"]){
            logger->error("Missing 'id' in request body");
            return nlohmann::json{};
        }

        //type and ID are always specified by the get handler
        std::string_view type = view["type"].get_value().get_string().value;
        std::string_view id = view["id"].get_value().get_string().value;

        logger->debug("Type is: {}", type);
        logger->debug("ID is: {}", id);

        auto collection = this->client[this->database][type];

        auto search = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("_id", id)
        );
        auto result = collection.find_one(search.view());
        if(result){
            logger->info("Received document from database: {}", bsoncxx::to_json(*result));
            return nlohmann::json::parse(bsoncxx::to_json(*result));
        }else{
            logger->error("Could not retrieve ID from the database..."); 
            return nlohmann::json{};
        }
    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        return {};
    }
}

nlohmann::json Broker::get(const std::string& request){
    constexpr const char* delimeter = "+";
    try{
        auto jsonData = bsoncxx::from_json(request);
        auto view = jsonData.view();
        if(!view["id"]){
            logger->error("Missing 'id' in request body");
            return {};
        }

        bsoncxx::document::element timeframe(view["timeframe"]);

        auto timeframe_view = timeframe.get_value().get_string().value;
        auto tokens = split(timeframe_view, delimeter);

        nlohmann::json array = nlohmann::json::array(); 

        for (auto& token : tokens) {
            logger->debug("Processing timeframe: {}", token);
            temporal_query::Timeframe frame         = temporal_query::string_to_timeframe(token);
            logger->debug("Parsed timeframe: {}", temporal_query::timeframe_to_string(frame));
            auto      result        = get_from_database(frame, view);
            auto      parsed_result = nlohmann::json::parse(result);
            nlohmann::json entry;
            entry[token] = parsed_result;
            array.push_back(entry);
        }

        return array;

    }catch(const std::exception& e){
        logger->error("Error: {}", e.what());
        return {};
    }
}



void Broker::remove(){}

std::string Broker::state(const std::string& request)
{
    auto is_secondary = [&](const std::string& name) 
    {
        return name.find(secondary_suffix) != std::string::npos;
    };

    try 
    {
        auto jsonData = bsoncxx::from_json(request);
        auto view = jsonData.view();

        const auto type_el = view["type"];
        if (!type_el || type_el.type() != bsoncxx::type::k_string) {
            logger->error("Invalid or missing 'type' in /state request");
            return R"({"status":"error","message":"missing or invalid 'type'"})";
        }

        std::string_view type_sv = type_el.get_value().get_string().value;

        // Skip SECONDARY collections while still returning valid JSON
        if (is_secondary(std::string(type_sv)))
        {
            logger->info("Skipping fingerprint for SECONDARY collection: {}", type_sv);
            nlohmann::json out = 
            {
                {"collection", std::string(type_sv)},
                {"secondary",  true},
                {"algo",       ""},
                {"hash",       ""},
                {"count",      0}
            };
            return out.dump();
        }

        // Normal path
        const std::string type{type_sv};
        auto coll = this->client[this->database][type];
        std::string fp = collection_fingerprint(coll);
        logger->info("Collection fingerprint: {}", fp);
        auto count = coll.count_documents({});

        nlohmann::json out = 
        {
            {"collection", type},
            {"algo",       "SHA-256"},
            {"hash",       fp},
            {"count",      count}
        };

        logger->info("Collection {} fingerprint: {}", type, fp);
        return out.dump();
    }
    catch (const std::exception& e) 
    {
        logger->error("Error computing fingerprint: {}", e.what());
        return R"({"status":"error","message":"failed to compute fingerprint"})";
    }
}

nlohmann::json Broker::dump_database(const std::optional<std::string>& collection) 
{
    nlohmann::json out = nlohmann::json::object();

    auto is_secondary = [&](const std::string& name) 
    {
        return name.find(secondary_suffix) != std::string::npos;
    };

    try 
    {
        auto db = this->client[this->database];

        std::vector<std::string> collections;
        if (collection && !collection->empty()) 
        {
            collections.push_back(*collection);
        } 
        else 
        {
            collections = db.list_collection_names();
        }

        for (const auto& coll_name : collections) 
        {
            if (is_secondary(coll_name)) 
            {
                logger->debug("Skipping secondary collection in dump: {}", coll_name);
                continue;
            }

            logger->info("Dumping collection: {}", coll_name);
            auto coll = db[coll_name];

            nlohmann::json docs = nlohmann::json::array();
            mongocxx::options::find opts;
            // (optional) can set a batch size if there are very large collections
            // opts.batch_size(1000);

            auto cursor = coll.find({}, opts);
            for (auto&& doc : cursor) 
            {
                // bsoncxx::to_json preserves special types as extended JSON (e.g. {"$oid":...})
                docs.push_back(nlohmann::json::parse(bsoncxx::to_json(doc)));
            }

            out[coll_name] = std::move(docs);
        }

        return out;
    } 
    catch (const std::exception& e) 
    {
        logger->error("dump_database failed: {}", e.what());
        // Return a structured error object; caller can decide how to respond.
        return nlohmann::json
        {
            {"status", "error"},
            {"message", e.what()}
        };
    }
}

// needs paid version
//std::string_view Broker::state(const std::string& request){
//
//    try{
//        auto jsonData = bsoncxx::from_json(request);
//        auto view = jsonData.view();
//        std::string_view type = view["type"].get_value().get_string().value;
//        bsoncxx::builder::basic::array collections{};
//        collections.append(type);
//        bsoncxx::builder::basic::document document; 
//        document.append(bsoncxx::builder::basic::kvp("dbHash", 1));
//        document.append(bsoncxx::builder::basic::kvp("collections", collections));
//        auto database = client[this->database];
//        // run the { dbHash: 1, collections: [...] } command
//        auto result = database.run_command(document.view());
//        auto result_view   = result.view();
//
//        // extract the "md5" field
//        if (result_view["ok"].get_double() != 1.0)
//          throw std::runtime_error("dbHash failed: " + bsoncxx::to_json(view));
//
//        logger->debug("Received MD5 hash: {}", result_view["md5"].get_value().get_string().value);
//        return result_view["md5"].get_value().get_string().value;
//    }catch(const std::exception& e){
//        logger->error("Error: {}", e.what());
//        return "";
//    }
//}



