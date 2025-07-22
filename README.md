# Compile the project

The below cmake configuration will compile: 

- NFD
- NFD-tools 
- ndn-cxx ( the simple version )


```
mkdir build
cd build
cmake ..
make
```


Note that you might need to install the following dependencies: 

```
sudo apt install libsqlite3-dev
sudo apt install libpcap-dev
sudo apt install libsystemd
sudo apt install libboost-all-dev
```

Also you probably need to use: 

```
sudo make
```

# Run 

You should be able to run everything from: 

```
sudo ./run-nfd.sh
```

Note you might need to copy the file that is located inside: 
```
build/NFD/install/etc/ndn/ndf.conf.sample
```

Inside the same folder: 

```
build/NFD/install/etc/ndn/ndf.conf
```

It is the same file without the .sample

# Requests 

```
curl -v -i -X POST http://127.0.0.1:8080 -d id="car1&type=CAR"
```

or loop for multiple ids, e.g., 10 ids car1 to car10: 

```
$ for i in {1..10}; 
do curl -v -i -X POST http://127.0.0.1:8080 -d id="car${i}&type=CAR"; 
sleep 0.5; 
done
```

Run the subscriber indicating either id or typee.g., send a GET HTTP message with URL param `?id=car1" or `?type=CAR`. Can be done via browser or curl

```
curl -i -X GET http://localhost:8080/?id=car1
```

## Subscriptions

For subscriptions append the `isSub=true&suburl=MYURL` at the url. Add quotes to all variables e.g., `?id=car1&isSub=true&suburl=MYURL`

```
curl -i -X GET http://localhost:8080/?id=car1&isSub=true&suburl=MYURL
```

## Publish versioned ID 
Note this will try to publish it to endpoint add. If you don't have an endpoint setup you will get an error: 
```cpp
if(message.get()->getMessageType() == SNDS_INTRA_MESSAGE_TYPE::TEMPORAL_QUERY_REQUEST){
    m_logger->info("Got new Temporal Query request...\n{}", message.get()->printDetails());
    auto query = deserialize_temporal_query(content);
    //query.date = md5_hex(&query.date, query.date.size());
    auto name = to_path(query, false, false);
    m_logger->info("Annoucing name {}", name);
    m_face.setInterestFilter(name,
      std::bind(&SNDSProducer::onInterest, this, std::placeholders::_2),
      std::bind(&SNDSProducer::onRegisterSuccess, this, std::placeholders::_1),
      std::bind(&SNDSProducer::onRegisterFailed, this, std::placeholders::_1, std::placeholders::_2));

    std::string payload = serialize_temporal_query(query);
    httplib::Client cli(broker_ip.c_str(), broker_port);
    auto res = cli.Post("/add", payload, "application/json");
    if (res && res->status == 200) {
        m_logger->info("Successfully POSTed to /add: {}", res->body);
    } else {
        m_logger->error("Failed to POST to /add: {}", res ? res->body : "No response");
    }
    continue;
}
```
The request can look like this: 
```
curl -X POST http://localhost:8080/ \
  --data-urlencode "id=car2" \
  --data-urlencode "date=2025-06-28T15:41:50Z" \
  -H "Accept: application/json"
```
## Temporal Query:

This will retrieve the metafile from the database:

```
curl -G http://localhost:8080/ \
  --data-urlencode "id=car2" \
  --data-urlencode "date=2025-06-28T15:41:50Z" \
  --data-urlencode "timeframe=before+after" \
  --data-urlencode "count=10" \
  -H "Accept: application/json"
```

# Broker


You can run test the broker as a standalone binary. No need to run through the snds_broker_bin.

## Compile 

Compile everything along with the broker:

```
sudo make (-j) 
```

Compile only the broker target: 

```
sudo cmake --build . --target snds_broker_bin (-j)
```

## Run


Note you need to provide the arguments like port,ip inside the main function.

```cpp
#include <iostream>
#include <string>
#include <sstream>

#include "broker.h"


int main(int argc, char* argv[]){

    if (argc != 4) {
        std::cerr << "Usage: ./app <username> <password> <database>\n";
        return 1;
    }

    std::string username = argv[1];
    std::string password = argv[2];
    std::string database = argv[3];

    std::string uri_str = "mongodb+srv://" + username + ":" + password +
                          "@cluster0.czndtc9.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0";

    try {
        mongocxx::instance instance{};
        mongocxx::uri uri(uri_str);
        Broker broker(uri, database, spdlog::level::debug, "Broker.log", "Broker", "0.0.0.0", 10000);
        broker.listen();
    } catch (const std::exception& e) {
        std::cerr << "MongoDB Exception: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
```

The only thing you need to provide through the command line is the password, username and the database name of the MongoDB instance.

```
Usage: ./app <username> <password> <database>
```

## Requests

Currently the HTTP API supports: 

- (POST) add-one. The type parameter does not have to be filled. If its not filled or doesn't exist the system will default to the `GENERIC` type:
```
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -d '{
    "id": "car2",
    "date": "2025-06-28T15:41:50Z",
    "type": "CAR"
    // rest_of_the_data...
  }'
```
- (GET) temporal-query. The type parameter does not have to be filled. If its not filled or doesn't exist the system will default to the `GENERIC` type: 
```
curl -G http://localhost:10000/temporal-query \
  --data-urlencode "id=car2" \
  --data-urlencode "type=CAR" \
  --data-urlencode "count=10" \
  --data-urlencode "date=2025-06-28T15:41:50Z" \
  --data-urlencode "timeframe=before+after" \
  -H "Accept: application/json"
```

- (GET) state:
```
curl -G http://localhost:10000/state \
  --data-urlencode "type=sensor" \
  -H "Accept: application/json"
```