#include "broker.h"

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

nlohmann::json Broker::normalize_json(const nlohmann::json& j) {
    if (j.is_object()) {
        std::vector<std::string> keys;
        for (auto& kv : j.items()) keys.push_back(kv.key());
        std::sort(keys.begin(), keys.end());

        nlohmann::json out = nlohmann::json::object();
        for (auto& k : keys) {
            out[k] = normalize_json(j.at(k));
        }
        return out;
    }
    else if (j.is_array()) {
        nlohmann::json out = nlohmann::json::array();
        for (auto& el : j) {
            out.push_back(normalize_json(el));
        }
        return out;
    }
    return j;
}

std::string Broker::hash_document(bsoncxx::document::view document) {
    // to_json gives a predictable ordering for primitives
    auto json              = bsoncxx::to_json(document);                
    auto parsed_document   = nlohmann::json::parse(json);      
    auto canonical_json    = normalize_json(parsed_document);               
    auto compact           = canonical_json.dump();                         
    return md5_hex(compact.data(), compact.size());
}

std::string Broker::collection_fingerprint(mongocxx::collection& collection) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    mongocxx::options::find opts;

    for (auto&& doc : collection.find({}, opts)) {
        auto h = hash_document(doc);
        if (EVP_DigestUpdate(ctx, h.data(), h.size()) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestUpdate failed");
        }
    }

    unsigned char final_digest[EVP_MAX_MD_SIZE];
    unsigned int  final_len = 0;
    if (EVP_DigestFinal_ex(ctx, final_digest, &final_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < final_len; ++i) {
        oss << std::setw(2) << static_cast<int>(final_digest[i]);
    }
    return oss.str();
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

std::optional<bsoncxx::document::value> Broker::set_generic_type(bsoncxx::v_noabi::document::view view){
    if (view["type"] && !view["type"].get_string().value.empty()) {
        logger->debug("Type is well defined. Not setting generic");
        return std::nullopt;
    }

    logger->warn("Missing 'type' in request body, defaulting to 'generic'");
    bsoncxx::builder::basic::document builder;
    for (auto&& elem : view) {
        if (elem.get_value().type() == bsoncxx::type::k_string &&
            elem.get_value().get_string().value.empty()) {
            logger->debug("Empty element discovered skipping");
            continue;
        }
        builder.append(bsoncxx::builder::basic::kvp(elem.key(), elem.get_value()));
    }

    builder.append(bsoncxx::builder::basic::kvp("type", "GENERIC"));
    return bsoncxx::document::value(builder.extract());
}

void Broker::filter_to_only_id(mongocxx::v_noabi::pipeline& pipeline){
    pipeline.project(
        bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("_id", 1)
        )
    );
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
        filter_to_only_id(pipeline);
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