# Experiment Description 

# Topology 

## Producer 

## Consumer 1

## Consumer 2


# Results and setup

## Populate MongoDB


Note that you need to run Broker bin first: 

```
./snds_broker_bin --mongo.uri mongodb://127.0.0.1:27017
Using URI: mongodb://127.0.0.1:27017
[2025-10-01 15:07:06.954] [info] [Broker] HTTP server listening on 0.0.0.0:10000
```

### car002 @ 09:30
```sh
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "car002",
    "type": "CAR",
    "speed": 64,
    "engine_temp": 25400,
    "battery": 91,
    "timestamp": "2025-09-25T09:30:00Z"
  }'
```

### car001 @ 09:30
```sh
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "car001",
    "type": "CAR",
    "speed": 62,
    "engine_temp": 85.2,
    "battery": 91,
    "timestamp": "2025-09-25T09:30:00Z"
  }'
```
### car001 @ 09:00
```sh
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "car001",
    "type": "CAR",
    "speed": 50,
    "engine_temp": 80.1,
    "battery": 95,
    "timestamp": "2025-09-25T09:00:00Z"
  }'
```

### car001 @ 10:00 (newest)
```sh
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "car001",
    "type": "CAR",
    "speed": 75,
    "engine_temp": 92.4,
    "battery": 87,
    "timestamp": "2025-09-25T10:00:00Z"
  }'
```

### car002 @ 09:40
```sh
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "car002",
    "type": "CAR",
    "speed": 70,
    "engine_temp": 90.1,
    "battery": 78,
    "timestamp": "2025-09-25T09:40:00Z"
  }'
```
### car003 @ 09:50
```sh
curl -X POST http://localhost:10000/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "car003",
    "type": "CAR",
    "speed": 55,
    "engine_temp": 79.4,
    "battery": 95,
    "timestamp": "2025-09-25T09:50:00Z"
  }'
```

## Run necessary services


Producer's MongoDB Service:

```
./snds_broker_bin --mongo.uri mongodb://127.0.0.1:27017
```

Producer SeEDS Service: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 0
```

Consumer .7 SeEDS Service: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 1
```

Consumer .27 SeEDS Service: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 2
```

Note how they all share the same prefix. This is because we need to get the data from the Producer's prefix.



## Publish ID from Producer

Proceed to publish ``car001`` to the NDN network, from within the Producer Node: 

```
scn4ndn@seeds8:~$ curl -v -i -X POST http://127.0.0.1:8080 -d "id=car001"
Note: Unnecessary use of -X or --request, POST is already inferred.
*   Trying 127.0.0.1:8080...
* Connected to 127.0.0.1 (127.0.0.1) port 8080
> POST / HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/8.5.0
> Accept: */*
> Content-Length: 9
> Content-Type: application/x-www-form-urlencoded
> 
< HTTP/1.1 200 OK
HTTP/1.1 200 OK
< Keep-Alive: timeout=5, max=100
Keep-Alive: timeout=5, max=100
< Content-Length: 189
Content-Length: 189
< Content-Type: text/plain
Content-Type: text/plain

< 
{
  "batch_size": 1,
  "messages": [
    {
      "name": "",
      "payload": {
        "raw": "ACK_ID_ADVERTISED\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}
* Connection #0 to host 127.0.0.1 left intact
```


Let's check the Producer's Node logs: 

```log
[2025-10-01 07:50:50.107] [info] [HTTPServer] Received POST request for / with body id=car001
[2025-10-01 07:50:50.107] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car001}
Type: BY_ID_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
[2025-10-01 07:50:50.107] [debug] [HTTPServer] Writing message from POST request to service queue...
Message: {id=car001}
Type: BY_ID_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001

[2025-10-01 07:50:50.108] [debug] [DigitalTwin] Extracted content from message {id=car001}
[2025-10-01 07:50:50.108] [debug] [DigitalTwin] Extracted content length from message 11
[2025-10-01 07:50:50.108] [debug] [DigitalTwin] Extracted type from message: BY_ID_ADVERTISEMENT
[2025-10-01 07:50:50.108] [info] [DigitalTwin] Received by ID get request...
[2025-10-01 07:50:50.108] [debug] [DigitalTwin] Pushed ID car001 to the announced IDs vector...
[2025-10-01 07:50:50.108] [debug] [DigitalTwin] Extracted id Scope from message SNDS_ID_scope_2
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 07:50:50.108] [info] [DigitalTwin] Pushing byID advertisement to the Producer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 07:50:50.108] [debug] [DigitalTwin] Sleeping 2000 ms before sending scope advertisement...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 07:50:50.175] [info] [Producer] Got new ID advertisement request...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 07:50:50.175] [debug] [Producer] Parsed interest Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: postID, Params: [car001] from string
[2025-10-01 07:50:50.175] [info] [Producer] Announcing name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001
[2025-10-01 07:50:50.176] [info] [Producer] Publishing to broker...
[2025-10-01 07:50:50.176] [warning] [Producer] _publish_to_broker: 'type' (string) not provided...
[2025-10-01 07:50:50.176] [info] [Producer] POST /add -> {"id":"car001"}
[2025-10-01 07:50:50.182] [info] [Producer] Published to broker OK: {"status": "success", "inserted_count_versioned": 1, "upserted_or_modified_count": 1}
```


We can see that is successfully published the ``car001`` ID into the NDN network.

## Get ID from Consumers

Let's try to the ID ``car001`` from the SeEDS Service of Consumer .7: 

```sh
scn4ndn@seeds7:~$ curl -i -X GET http://localhost:8080/?"id=car001"
HTTP/1.1 200 OK
Keep-Alive: timeout=5, max=100
Content-Length: 332
Content-Type: text/plain

{
  "batch_size": 1,
  "messages": [
    {
      "name": "",
      "payload": {
        "items": [
          {
            "date": 1759305050180,
            "id": "car001",
            "type": "GENERIC"
          }
        ],
        "mode": "direct"
      },
      "sender": 6,
      "type": "TEMPORAL_QUERY_RESPONSE"
    }
  ]
}
scn4ndn@seeds7:~$ 
```

Nice we got the ID. Let's make sure we actually got it from the Producer Node .8. Let's check the logs of the Producer .8: 

```log
[2025-10-01 08:00:30.383] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 08:00:30.383] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: temporalQuery, Params: [id=car001, type=, date=, count=, timeframe=, filters=]
[2025-10-01 08:00:30.383] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 08:00:30.383] [debug] [Producer] TemporalQuery → id: car001, date: , type: , timeframe: , count: , filters: 
[2025-10-01 08:00:30.383] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car001&type=&count=&date=&timeframe=&filters=
[2025-10-01 08:00:30.386] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 08:00:30.386] [debug] [Producer] Sending data to the NDN…
```

Let's also check the logs of the Consumer .7: 

```log
[2025-10-01 08:00:30.377] [info] [HTTPServer] Received GET request for / with body 
[2025-10-01 08:00:30.377] [info] [HTTPServer] Received GET request for / with target /?id=car001
[2025-10-01 08:00:30.377] [info] [HTTPServer] Received GET request from 127.0.0.1
Message: {id=car001}
Type: GET_BY_ID_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
id = car001
[2025-10-01 08:00:30.377] [info] [Worker] Received content: {id=car001}
[2025-10-01 08:00:30.377] [info] [Worker] Received payload: 
[2025-10-01 08:00:30.377] [info] [Worker] Received type: GET_BY_ID_REQUEST
[2025-10-01 08:00:30.377] [info] [Worker] Inserted ID: car001 into set. Pushing regular getByID request for ID: car001
[2025-10-01 08:00:30.377] [info] [Worker] Reading packet...
[2025-10-01 08:00:30.377] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery:id%3Dcar001&type%3D&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 08:00:30.378] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&Lifetime=6000
[2025-10-01 08:00:30.391] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 08:00:30.392] [info] [Consumer] Data packet payload: [
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759305050180
      },
      "type": "GENERIC"
    }
  }
] in function onData
[2025-10-01 08:00:30.392] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: temporalQuery, Params: [id=car001, type=, date=, count=, timeframe=, filters=] in onData
[2025-10-01 08:00:30.392] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 08:00:30.392] [info] [Worker] Received content: /?id=car001&type=&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 08:00:30.392] [info] [Worker] Received payload: [
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759305050180
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 08:00:30.392] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 08:00:30.392] [info] [Consumer] Reading packet...
[2025-10-01 08:00:30.392] [info] [Worker] Direct mode...
[2025-10-01 08:00:30.392] [info] [Worker] Reading packet...
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
  "batch_size": 1,
  "messages": [
    {
      "name": "",
      "payload": {
        "items": [
          {
            "date": 1759305050180,
            "id": "car001",
            "type": "GENERIC"
          }
        ],
        "mode": "direct"
      },
      "sender": 6,
      "type": "TEMPORAL_QUERY_RESPONSE"
    }
  ]
}
```

@TODO from .27

## Get by Type from Consumers 


Let's publish an ID ``car001`` to the ``CAR`` registry of the Producer: 

```
scn4ndn@seeds8:~$ curl -v -i -X POST http://127.0.0.1:8080 -d "type=CAR&id=car001"
Note: Unnecessary use of -X or --request, POST is already inferred.
*   Trying 127.0.0.1:8080...
* Connected to 127.0.0.1 (127.0.0.1) port 8080
> POST / HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/8.5.0
> Accept: */*
> Content-Length: 18
> Content-Type: application/x-www-form-urlencoded
> 
< HTTP/1.1 200 OK
HTTP/1.1 200 OK
< Keep-Alive: timeout=5, max=100
Keep-Alive: timeout=5, max=100
< Content-Length: 260
Content-Length: 260
< Content-Type: text/plain
Content-Type: text/plain

< 
{
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR",
      "payload": {
        "raw": "ACK_ID_REGISTERED\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}
* Connection #0 to host 127.0.0.1 left intact
```


Let's check the logs of the Producer Node: 

```log
[2025-10-01 08:08:48.245] [info] [HTTPServer] Received POST request for / with body type=CAR&id=car001
[2025-10-01 08:08:48.246] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car001,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
type = CAR
[2025-10-01 08:08:48.246] [debug] [HTTPServer] Writing message from POST request to service queue...
Message: {id=car001,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
type = CAR

[2025-10-01 08:08:48.246] [debug] [DigitalTwin] Extracted content from message {id=car001,type=CAR}
[2025-10-01 08:08:48.246] [debug] [DigitalTwin] Extracted content length from message 20
[2025-10-01 08:08:48.246] [debug] [DigitalTwin] Extracted type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 08:08:48.246] [info] [DigitalTwin] Received by Type get request...
[2025-10-01 08:08:48.246] [debug] [DigitalTwin] The Type is the following: CAR
[2025-10-01 08:08:48.246] [debug] [DigitalTwin] The ID is the following: car001
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 08:08:48.246] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 08:08:48.246] [debug] [DigitalTwin] Sleeping 2000 ms before sending scope advertisement...
[2025-10-01 08:08:48.246] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
[2025-10-01 08:08:48.246] [debug] [Consumer] Extracted content length from message: 71
[2025-10-01 08:08:48.246] [debug] [Consumer] Extracted message type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 08:08:48.246] [debug] [Consumer] Created name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR in func: express_by_id_and_type_advertisement_interest
[2025-10-01 08:08:48.246] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR?CanBePrefix&Lifetime=6000
[2025-10-01 08:08:48.246] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR
[2025-10-01 08:08:48.247] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car001, CAR]
[2025-10-01 08:08:48.247] [info] [Producer] Processing Type Advertisement
[2025-10-01 08:08:48.247] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car001
[2025-10-01 08:08:48.247] [info] [Producer] [event] type_add: CAR -> car001
[2025-10-01 08:08:48.247] [info] [Producer] [resilience] mutate → ver=10 hash=d35ab132e24de8f933603ce28e8e6481
[2025-10-01 08:08:48.247] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(260ec692) 2(70f94e1d) 3(e3bf833b) 4(d82c0ce5) 5(c9609515) 6(1e561ccd) 7(f9042de4) 8(beb95ca5) 9(735e2035) 10(d35ab132) 
[2025-10-01 08:08:48.247] [debug] [Producer] [resilience] ops from v9 → v10:
[
  {
    "op": "add",
    "path": "/types/CAR",
    "value": {
      "ids": {
        "car001": true
      }
    }
  }
]
[2025-10-01 08:08:48.247] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 08:08:48.247] [debug] [Producer] HTTP GET /state?type=CAR
[2025-10-01 08:08:48.249] [info] [Producer] [resilience] mutate → ver=11 hash=35ce77aa2fd7103293a965e62378e09a
[2025-10-01 08:08:48.249] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(260ec692) 2(70f94e1d) 3(e3bf833b) 4(d82c0ce5) 5(c9609515) 6(1e561ccd) 7(f9042de4) 8(beb95ca5) 9(735e2035) 10(d35ab132) 11(35ce77aa) 
[2025-10-01 08:08:48.249] [debug] [Producer] [resilience] ops from v10 → v11:
[
  {
    "op": "add",
    "path": "/broker/collections/CAR",
    "value": {
      "algo": "SHA-256",
      "count": 0,
      "hash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "lastSeen": "2025-10-01T08:08:48Z"
    }
  }
]
[2025-10-01 08:08:48.249] [info] [Producer] [debug][event:type_add CAR->car001] summary → types=1 ids=1 subs(byId)=0 subs(byType)=0 announced=7
[2025-10-01 08:08:48.249] [debug] [Producer] [debug][event:type_add CAR->car001] type_registry:
[2025-10-01 08:08:48.249] [debug] [Producer]   [CAR] car001 
[2025-10-01 08:08:48.249] [debug] [Producer] [debug][event:type_add CAR->car001] subscribed_nodes_for_id:
[2025-10-01 08:08:48.249] [debug] [Producer] [debug][event:type_add CAR->car001] subscribed_nodes_for_type:
[2025-10-01 08:08:48.249] [debug] [Producer] [debug][event:type_add CAR->car001] announcements: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry /ndn/gr/edu/mmlab1/aueb/thomasi/CAR /ndn/gr/edu/mmlab1/aueb/thomasi/state_0 /ndn/gr/edu/mmlab1/aueb/thomasi/is_alive_0 /ndn/gr/edu/mmlab1/aueb/thomasi/car001 /ndn/gr/edu/mmlab1/aueb/thomasi/SNDS_ID_scope_0 /ndn/gr/edu/mmlab1/aueb/thomasi_0 
[2025-10-01 08:08:48.249] [info] [Producer] [debug][event:type_add CAR->car001] broker.journal: entries=1 cursor=0 last_fp=
[2025-10-01 08:08:48.249] [debug] [Producer] [debug][event:type_add CAR->car001] broker.journal (last 1 of 1):
[2025-10-01 08:08:48.249] [debug] [Producer]   [0] ts=2025-10-01T07:50:50Z type=GENERIC id=car001 payload_keys=1
[2025-10-01 08:08:48.249] [debug] [Producer] [overlay][event:type_add CAR->car001] ver=0 hash=d41d8cd98f00b204e9800998ecf8427e keys=0
[2025-10-01 08:08:48.249] [info] [Producer] Registered new ID car001 for type CAR. Sending CONTROL.
[2025-10-01 08:08:48.249] [debug] [Producer] Sending data to the NDN…
[2025-10-01 08:08:48.249] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR in function onData
[2025-10-01 08:08:48.249] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 08:08:48.249] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car001, CAR] in onData
```

We can also publish to ``CAR`` from a completely different node since it depends on the namespace of the producer. Let's do it from Consumer .7:

```
scn4ndn@seeds7:~$ curl -v -i -X POST http://127.0.0.1:8080 -d "type=CAR&id=car002"
Note: Unnecessary use of -X or --request, POST is already inferred.
*   Trying 127.0.0.1:8080...
* Connected to 127.0.0.1 (127.0.0.1) port 8080
> POST / HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/8.5.0
> Accept: */*
> Content-Length: 18
> Content-Type: application/x-www-form-urlencoded
> 
< HTTP/1.1 200 OK
HTTP/1.1 200 OK
< Keep-Alive: timeout=5, max=100
Keep-Alive: timeout=5, max=100
< Content-Length: 260
Content-Length: 260
< Content-Type: text/plain
Content-Type: text/plain

< 
{
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR",
      "payload": {
        "raw": "ACK_ID_REGISTERED\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}
* Connection #0 to host 127.0.0.1 left intact
scn4ndn@seeds7:~$ 
```

Let's check the logs of the Producer Node: 

```log
[2025-10-01 08:12:35.816] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR
[2025-10-01 08:12:35.816] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car002, CAR]
[2025-10-01 08:12:35.816] [info] [Producer] Processing Type Advertisement
[2025-10-01 08:12:35.816] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car002
[2025-10-01 08:12:35.816] [info] [Producer] [event] type_add: CAR -> car002
[2025-10-01 08:12:35.816] [info] [Producer] [resilience] mutate → ver=12 hash=c62806b94b60ee2c75f7962110fc6c32
[2025-10-01 08:12:35.817] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(260ec692) 2(70f94e1d) 3(e3bf833b) 4(d82c0ce5) 5(c9609515) 6(1e561ccd) 7(f9042de4) 8(beb95ca5) 9(735e2035) 10(d35ab132) 11(35ce77aa) 12(c62806b9) 
[2025-10-01 08:12:35.817] [debug] [Producer] [resilience] ops from v11 → v12:
[
  {
    "op": "add",
    "path": "/types/CAR/ids/car002",
    "value": true
  }
]
[2025-10-01 08:12:35.817] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 08:12:35.817] [debug] [Producer] HTTP GET /state?type=CAR
[2025-10-01 08:12:35.821] [info] [Producer] [resilience] mutate → ver=13 hash=05278785cae92313095d73b6cf1a83bc
[2025-10-01 08:12:35.821] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(260ec692) 2(70f94e1d) 3(e3bf833b) 4(d82c0ce5) 5(c9609515) 6(1e561ccd) 7(f9042de4) 8(beb95ca5) 9(735e2035) 10(d35ab132) 11(35ce77aa) 12(c62806b9) 13(05278785) 
[2025-10-01 08:12:35.821] [debug] [Producer] [resilience] ops from v12 → v13:
[
  {
    "op": "replace",
    "path": "/broker/collections/CAR/lastSeen",
    "value": "2025-10-01T08:12:35Z"
  }
]
[2025-10-01 08:12:35.821] [info] [Producer] [debug][event:type_add CAR->car002] summary → types=1 ids=2 subs(byId)=0 subs(byType)=0 announced=7
[2025-10-01 08:12:35.821] [debug] [Producer] [debug][event:type_add CAR->car002] type_registry:
[2025-10-01 08:12:35.821] [debug] [Producer]   [CAR] car002 car001 
[2025-10-01 08:12:35.821] [debug] [Producer] [debug][event:type_add CAR->car002] subscribed_nodes_for_id:
[2025-10-01 08:12:35.821] [debug] [Producer] [debug][event:type_add CAR->car002] subscribed_nodes_for_type:
[2025-10-01 08:12:35.821] [debug] [Producer] [debug][event:type_add CAR->car002] announcements: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry /ndn/gr/edu/mmlab1/aueb/thomasi/CAR /ndn/gr/edu/mmlab1/aueb/thomasi/state_0 /ndn/gr/edu/mmlab1/aueb/thomasi/is_alive_0 /ndn/gr/edu/mmlab1/aueb/thomasi/car001 /ndn/gr/edu/mmlab1/aueb/thomasi/SNDS_ID_scope_0 /ndn/gr/edu/mmlab1/aueb/thomasi_0 
[2025-10-01 08:12:35.821] [info] [Producer] [debug][event:type_add CAR->car002] broker.journal: entries=1 cursor=0 last_fp=
[2025-10-01 08:12:35.821] [debug] [Producer] [debug][event:type_add CAR->car002] broker.journal (last 1 of 1):
[2025-10-01 08:12:35.821] [debug] [Producer]   [0] ts=2025-10-01T07:50:50Z type=GENERIC id=car001 payload_keys=1
[2025-10-01 08:12:35.821] [debug] [Producer] [overlay][event:type_add CAR->car002] ver=0 hash=d41d8cd98f00b204e9800998ecf8427e keys=0
[2025-10-01 08:12:35.821] [info] [Producer] Registered new ID car002 for type CAR. Sending CONTROL.
[2025-10-01 08:12:35.821] [debug] [Producer] Sending data to the NDN…
```

Let's check the logs of the Consumer Node: 

```log
[2025-10-01 08:12:35.813] [info] [HTTPServer] Received POST request for / with body type=CAR&id=car002
[2025-10-01 08:12:35.813] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car002,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car002
type = CAR
[2025-10-01 08:12:35.813] [info] [DigitalTwin] Received by Type get request...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 08:12:35.813] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 08:12:35.813] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR?CanBePrefix&Lifetime=6000
[2025-10-01 08:12:35.824] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR in function onData
[2025-10-01 08:12:35.824] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 08:12:35.824] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car002, CAR] in onData
[2025-10-01 08:12:35.825] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 08:12:35.825] [info] [Consumer] Reading packet...
[2025-10-01 08:12:35.825] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
[2025-10-01 08:12:35.825] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 08:12:35.825] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
[2025-10-01 08:12:35.825] [info] [Worker] Received ACK Message with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR payload: ACK_ID_REGISTERED...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: CONTROL_MESSAGE
Sender: WORKER
Payload: ACK_ID_REGISTERED
```

Let's also try a GET by Type request from Consumer .7:




@TODO from .27