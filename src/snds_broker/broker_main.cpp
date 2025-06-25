#include "broker.h"


int main(){

    mongocxx::instance instance{};
    mongocxx::uri uri("mongodb://localhost:27017");
    Broker broker(uri, spdlog::level::debug); 

    broker.listen();
    return 0;
}