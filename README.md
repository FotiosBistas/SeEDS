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


By ID:

```
curl -i -X GET http://localhost:8080/?id=car1&isSub=true&suburl=MYURL
```

By Type: 

```
curl -i -X GET http://localhost:8080/?type=CAR&isSub=true&suburl=MYURL
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


# Components

## Service 

At runtime, SeEDS is a small distributed system composed of five cooperating services that run inside one process (each on its own thread) and talk over in-memory message queues. External I/O happens via NDN Interests/Data and a tiny HTTP API:

- SNDSHTTPService: small HTTP server (cpp-httplib) that turns HTTP GET/POST into internal messages.
- Worker: Parses requests, deduplicates, tracks state, aggregates results, and sends responses back to HTTP.
- SNDSConsumer: emits NDN Interests constructed from Worker’s requests, receives Data packets, and forwards payloads back.
- SNDSProducer: answers incoming NDN Interests (publishes Data) and also initiates outbound Interests when needed.
- DigitalTwin: drives advertisements (by-ID / by-type / scope) and forwards “temporal query” announcements to Producer.


### Main Requests 

#### GET by ID ( Forwarded to the NDN through the temporal query API )

HTTP receives ```GET /?id=<ID>``` (or POST body). It packs params into an intra-message and sends to Worker.

Worker:
- Deduplicates with pending_get_by_id_requests (set).
- Builds an NDN name: ```/<snds_prefix>/<id>/getByID:<id>``` using parsed_interest_name helpers.
- Sends a GET_BY_ID_REQUEST message to Consumer.

Consumer:
- Expresses the NDN Interest. Note that all the interests are forwarded through the temporal query API.
- On Data: forwards payload to Worker as GET_BY_ID_RESPONSE.

Worker:
- Confirms the ID was pending; clears it; optionally tells DigitalTwin to erase pending state.
- Writes the final response to the HTTP queue as TEMPORAL_QUERY_RESPONSE (used uniformly for JSON).

HTTP:
- Waits for one message from the service queue and returns 200 with the payload.

#### GET by TYPE 

HTTP receives ```GET /?type=<TYPE>```.

Worker:
- Deduplicates with pending_get_by_type_requests.
- Computes <TYPE>_registry (configurable postfix).
- Sends GET_BY_TYPE_REQUEST to Consumer with NDN name: ```/<snds_prefix>/<TYPE>_registry/getByType:<TYPE>``` or ```/<snds_prefix>/<TYPE>/getByType:<TYPE>``` which is equivalent.

Consumer: 
- fetches the registry file (a list of IDs) and sends it to Worker as REGISTRY_PAYLOAD.
Worker ( TODO this should be changed when we optimize cache hits ):
- Parses IDs with parse_ids_from_registry_file_payload(...).
- For each ID, sends GET_BY_ID_FOR_TYPE_REQUEST to Consumer with name: ```/<snds_prefix>/<TYPE>/getByIDForType:<ID>&<TYPE>```
- Tracks which IDs are outstanding in pending_ids_to_type_map[TYPE].
- Consumer replies for each ID with GET_BY_ID_FOR_TYPE_RESPONSE.( Forwarded to the NDN through the temporal query API )

Worker: 
- accumulates (id,payload) in payloads_for_get_by_type[TYPE].

When the last ID arrives, it assembles and sends it to HTTP as a single message:
```json
{ "type": "<TYPE>", "items": [ {"id": "...", "payload": "..."} ] }
```

#### Subscriptions & scope

Users can subscribe to events by sending ```GET /?id=<ID>&isSub=true&suburl=<URL>``` (or by type in the place of id=).

HTTP → Worker: BY_ID_SUBSCRIPTION or BY_TYPE_SUBSCRIPTION.

Worker:
- Stores the subscriber URL in subscribed_by_id_users or subscribed_by_type_users.
- Only on the first subscription for a key, it computes the node scope name
(getSNDSNodeScopeNameForSubscription) and asks Consumer to notify the appropriate scope:

```
/<snds_prefix>/<node_scope>/newIdSubscription:<ID>&<our_scope>
```
or: 
```
/<snds_prefix>/<node_scope>/newTypeSubscription:<type>&<our_scope>
```

#### Temporal queries (historical data)
Temporal queries fetch historical “metafile” records around a date, optionally filtered.

HTTP POST body or query string, e.g.:

```
id=car2&date=2025-06-28T15:41:50Z&timeframe=before+after&count=10
```

Worker: 
- Worker parses to a TemporalQuery
- frame: bitmask of NOW|BEFORE|AFTER (temporal_query::string_to_timeframe + to_fields)
- date, id, optional type, count, optional filters and extras
- Worker computes a stable hash (temporal_query_hash_string) and inserts the query into
pending_temporal_queries[hash]. Then it forwards the serialized query to Consumer

Consumer: 
```
/<snds_prefix>/<entity>/temporalQuery:id=...&date=...&timeframe=...&count=...[&filters=...]
```
where ```entity = (type.empty() ? id : type)```.

Network returns one of two shapes:

a) Direct mode (single item)

  > Payload returns only one items back to the consumer.Consumer detects it (_is_direct_temporal_payload), extracts the item, converts it into a Version envelope (with is_metafile=false, mode="direct"), and sends one VERSION_RESPONSE back to Worker via a canonical query-URI (using build_query_uri).

b) Manifest mode (metafile list)

> Payload is an array of documents with _id containing { _id: "<id>", timestamp: {"$date": <ms>} }. Consumer fans out: for each doc, it constructs a metafile interest 
```
/<snds_prefix>/<id>/version?id=...&type=...&date=...&metafile=true&parentQueryHash=...
```
> It sends all but the last with lastInterest=false, and the last with lastInterest=true.(A duplicate filter with a canonicalized name avoids resending.)

Worker aggregation

a) Direct mode: Worker extracts the single item and immediately returns:

```json
{"mode":"direct","items":[{...}]}
```
b) Manifest mode:

- Worker buffers every VERSION_RESPONSE per parentQueryHash in temporal_buffers[hash].
- Each slice is sanitized (fix_mixed_utf8_cp1252) and optionally field-filtered (if filters present).
- On the lastInterest it sends a single bundle back to HTTP:
```json
{
  "mode": "manifest",
  "parentQueryHash": "<hash>",
  "query": "<serialized query>",
  "items": [ <filtered JSON slices> ]
}
```
- Then it clears pending_temporal_queries[hash] and the buffer.
- Filtering: users can pass filters=a,b,c in the query; Worker will keep only those keys per item (object-wise).


## Broker
You can run test the broker as a standalone binary. No need to run through the snds_broker_bin.

### Summary 

The Broker is a small HTTP service in front of MongoDB that stores, retrieves,fingerprints, and bulk-copies JSON documents. It supports both latest (unversioned) and historical (versioned) views of the same data and exposes a handful of endpoints for SNDS components (producers/consumers, resilience tools).

#### Key ideas
Two collections per type:
- Unversioned: <TYPE> — contains only the latest record for each id (upserted).
- Versioned: <TYPE>_VERSIONED — append-only history: _id is a document { _id: "<id>", timestamp: <date> }.
- For mirroring, a secondary suffix may be used: *_SECONDARY.

Deterministic fingerprints:
- Each document is canonicalized (keys sorted) and MD5’d.
- A collection fingerprint is SHA-256 over the per-doc MD5s, iterating in _id order.
- GET /state?type=<TYPE> returns { algo, hash, count }. Secondary collections are skipped.

Timestamps always present
- Every record gets a server-side timestamp (BSON date). Versioned _id also embeds the timestamp.

Graceful runtime:
- Single binary HTTP server (cpp-httplib), structured logs (spdlog), SIGINT-aware shutdown.

#### Endpoints (quick reference)

- POST /add: Insert/Upsert a document into both stores. Body must include at least id; type defaults to "GENERIC" if missing. 
  1. Unversioned: replace_one(..., upsert=true) on <TYPE>.
  2. Versioned: insert_one on <TYPE>_VERSIONED (or *_VERSIONED_SECONDARY if the incoming type name already carries _SECONDARY).
- GET /get?id=<ID>&type=<TYPE>: Returns the latest document for that id from <TYPE>.
- GET /temporal-query?id=<ID>&type=<TYPE>&date=<ISO>&timeframe=<NOW|BEFORE|AFTER[+...]>&count=<N>. Reads from <TYPE>_VERSIONED using MongoDB pipelines. This returns an array with one element per timeframe token:
  1. NOW: nearest entries to date
  2. BEFORE: entries <= date (ascending)
  3. AFTER: entries >= date (ascending)

- GET /state?type=<TYPE> Returns a fingerprint of <TYPE>:
  1. ``{ "collection": "TYPE", "algo": "SHA-256", "hash": "<hex>", "count": <n> }``
  2. For *_SECONDARY types, returns an empty fingerprint and secondary: true.

- GET /dump[?type=<TYPE>]: Export all (or one) non-secondary collections as Extended JSON.

- POST /copy: Bulk (re)hydrate collections from a JSON dump. Body shape:
```json
{
  "mirror_to_secondary": true,
  "secondary_suffix": "_SECONDARY",
  "collections": {
    "CAR": [ /* docs */ ],
    "CAR_VERSIONED": [ /* docs */ ]
  }
}
```
1. Unversioned docs → upsert by string _id.
2. Versioned docs → insert by composite _id {_id, timestamp}.
3. When mirror_to_secondary=true, data is written to <NAME>_SECONDARY.


### Example CURL

```bash
# Add or update a record
curl -sX POST localhost:8080/add \
  -H 'content-type: application/json' \
  -d '{"type":"CAR","id":"X123","speed":42}'

# Latest read
curl -s 'localhost:8080/get?id=X123&type=CAR'

# Temporal query (5 items before a date)
curl -s 'localhost:8080/temporal-query?id=X123&type=CAR&date=2025-06-26T14:30:00Z&timeframe=BEFORE&count=5'

# Collection fingerprint
curl -s 'localhost:8080/state?type=CAR'

# Dump all non-secondary collections
curl -s 'localhost:8080/dump'
```

### Compile 

Compile everything along with the broker:

```
sudo make (-j) 
```

Compile only the broker target: 

```
sudo cmake --build . --target snds_broker_bin (-j)
```
### Run
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
### Requests
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

## Resilience 

The resilience layer keeps a SECONDARY node in sync with a PRIMARY by exchanging tiny JSON envelopes over NDN. It prefers JSON Patch deltas for speed and falls back to full snapshots when needed. Everything is content-addressed with a deterministic hash so both sides can prove they agree.

### Store (PRIMARY)
Canonical, versioned JSON store with a bounded history. Each change bumps a version, recomputes a canonical hash, and keeps a snapshot for patch generation. Exposes:

- Meta → {version, hash}: a lightweight head descriptor for quick “are we in sync?” checks.
- Patch → {base_version, target_version, target_hash, ops} where ops is RFC6902 JSON Patch ``(nlohmann::json::diff(base,target))``. Apply with ``json.patch(ops)``.
- Snapshot → {version, hash, state}: full state transfer for cold start or when a patch isn’t possible.

### Mirror (SECONDARY)
Local follower that applies Snapshot or Patch. Validates via canonical hash; on mismatch it logs and normalizes locally (the goal is convergence, not crypto).

### ResiliencePoller (SECONDARY)
A tiny scheduler that probes the PRIMARY and pulls updates. It runs two periodic loops and posts all I/O onto the NDN Face’s io_context:

- Alive loop: proves reachability with a nonce echo.
- Meta loop: reads {version, hash, ts} and decides whether to fetch a patch or snapshot.

### Run 

| Arg index | Meaning                 | Default      | Example      |
| --------- | ----------------------- | ------------ | ------------ |
| 1         | NDN prefix              | `SNDS`       | `SNDS`       |
| 2         | peer “is alive” name    | `is_alive_0` | `is_alive_2` |
| 3         | peer “state” name       | `state_0`    | `state_2`    |
| 4         | alive probe period (ms) | `2000`       | `1000`       |
| 5         | meta poll period (ms)   | `2000`       | `3000`       |
| 6         | IPC host                | `127.0.0.1`  | `0.0.0.0`    |
| 7         | IPC port                | `47832`      | `49000`      |

Example with all defaults:
```
./resilience_poller_bin
```
Example specifying everything:
```
./resilience_poller_bin SNDS is_alive_2 state_2 1000 3000 127.0.0.1 49000
```



### Wire protocol (NDN interest)

Names are built under your SNDS prefix with per-peer components:

#### Liveness probe

- Purpose: “Are you up, and did you see my nonce?”
- Interest name: /<SNDS-prefix>/<peer_is_alive_name>/GET_RESILIENCE_IS_ALIVE/<nonce>
- Example: /SNDS/is_alive_2/GET_RESILIENCE_IS_ALIVE/123456789
- Data (JSON):

```json
{ "ack": true, "nonce": "123456789", "ts": "2025-06-26T14:30:00Z" }
```
- Client behavior: Accept only if ack == true and the echoed nonce matches.

#### Head metadata

- Purpose: “What version and hash is your current state?”
- Interest name: /<SNDS-prefix>/<peer_state_name>/GET_RESILIENCE_META
- Example: /SNDS/state_2/GET_RESILIENCE_META
- Data (JSON):
```json
{ "version": 42, "hash": "7c4ff521...md5...", "ts": "2025-06-26T14:31:00Z" }
```
- Client behavior: Compare version to your local version to decide whether you need a patch or a snapshot.

#### Patch (delta) request

- Purpose: “Give me the JSON Patch from my version → your version.”
- Interest name:/<SNDS-prefix>/<peer_state_name>/GET_RESILIENCE_PATCH/<from_version>/<to_version>
- Example: /SNDS/state_2/GET_RESILIENCE_PATCH/40/42
- Success Data (JSON):
```json
{
  "kind": "patch",
  "base_version": 40,
  "target_version": 42,
  "target_hash": "7c4ff521...md5...",
  "ops": [ /* RFC6902 JSON Patch ops */ ]
}
```
- Possible error Data (JSON): (producer guides you to fall back)
```json
{ "error": "patch_unavailable", "advice": "fetch_snapshot", "latest": 42 }
```
- Other error values you might see: ``missing_parameters, invalid_parameters, non_increasing_version, unknown_target_version, store_advanced``.
- Client behavior: Try applying ops to your local state. If apply fails or you get an error, fetch a snapshot of target_version (or of latest).

#### Snapshot (full state) request
- Purpose: “Give me the full state at your head (or at the specified version).”
- Interest name:/<SNDS-prefix>/<peer_state_name>/GET_RESILIENCE_SNAPSHOT/<to_version>
- Example: /SNDS/state_2/GET_RESILIENCE_SNAPSHOT/42
- Success Data (JSON):
```json
{
  "kind": "snapshot",
  "version": 42,
  "hash": "7c4ff521...md5...",
  "state": { /* entire canonical JSON state */ }
}
```
- Possible error Data (JSON):
```json
{ "error": "store_advanced", "advice": "fetch_snapshot", "latest": 43 }
```

(You asked for 42, but the head moved to 43; ask again for 43.)


#### Determinism & hashing

Canonicalization uses nlohmann::ordered_json + compact .dump() so the same logical object hashes identically everywhere.

Hashing is MD5 over the canonical dump. It’s a fast, stable fingerprint for identity checks—not a security guarantee.

#### Client behavior (SECONDARY)

- Boot → send META. With no local version, fetch SNAPSHOT at the advertised version and apply.
- Steady state → on META showing a higher version, request PATCH {local→remote}.
- If PATCH NACKs/times out/fails to apply, fetch SNAPSHOT {remote}.
- Liveness & promotion → on 3 consecutive is_alive misses, the example app self-promotes to PRIMARY, emits an IPC "role":"primary" envelope, and stops the poller.


# Run the MiniNDN


## Install MiniNDN 

Clone the repo: 
```
git clone https://github.com/named-data/mini-ndn
```
Install the MiniNDN: 
```
./install.sh
```

## Make it work with mongo

Install MongoDB locally to not have internet connectivity issues: 

```
# Ubuntu 22.04/24.04 example — installs MongoDB 7.0
wget -qO - https://www.mongodb.org/static/pgp/server-7.0.asc | sudo gpg --dearmor -o /usr/share/keyrings/mongodb-server-7.0.gpg
echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-7.0.gpg ] https://repo.mongodb.org/apt/ubuntu $(. /etc/os-release; echo $UBUNTU_CODENAME)/mongodb-org/7.0 multiverse" | \
  sudo tee /etc/apt/sources.list.d/mongodb-org-7.0.list
```

Install: 

```
sudo apt update
sudo apt install -y mongodb-org
sudo systemctl enable --now mongod
```

Sanity check: 

```
mongosh --eval 'db.runCommand({buildInfo:1}).version'
# and:
ss -lnt | grep 27017
```

A good GUI I use for mongo is Compass: https://www.mongodb.com/try/download/compass


Just download the apt file and install it. Connect to the local cluster ( you can connect to any cluster you want ): "mongodb://127.0.0.1:27017"


## Run 

```
sudo python run.py <specify_topology_file_here>
```


## Test the project effectively 


I spin up three nodes through the ``topology.conf``.


### Requests 

Requests are exactly the same as above. Just make sure to publish from one node and ask from another.

Also make sure you subscribe to other nodes and you get results etc.

### Broker

The ``run.py`` script spins up a broker instance in each Node. You can query the database using the following requests: 

Dump: 
```
mini-ndn> b curl -X GET 'http://10.0.0.2:10000/dump' -H 'Content-Type: application/json' -H 'Accept: application/json'
mini-ndn> a curl -X POST 'http://10.0.0.1:10000/add' -H 'Content-Type: application/json' -H 'Accept: application/json' -d '{"id":"car2","date":"2025-06-28T15:41:50Z","type":"CAR"}'
mini-ndn> a curl -X GET 'http://10.0.0.1:10000/dump' -H 'Content-Type: application/json' -H 'Accept: application/json'
```

With the above ideas you can also use the rest of the requests like add, get etc...



### Resilience

In node D you can set up the resilience by running through the terminal: 

```
./resilience_poller_bin --log-level="DEBUG" --ipc-port="47835"
```

This way D is going to become the `secondary` of A. You can set up A to exit the process after 120s to simulate a crash of A.

```
host.cmdPrint(
  'cd /home/fotis/Github/SeEDS/build && '
  f'env HOME="{host_home}" timeout 120s ./snds_daemon_bin '
  ...
)
```

You should see D merging of all its data with A.