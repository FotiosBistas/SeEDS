# Experiment Description 

# Topology 

## Producer 

## Consumer 1

## Consumer 2


# Results and setup

## Populate MongoDB

You can empty the database: 

```sh
mongosh SeEDS --eval "db.dropDatabase()"
```

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


Producer's NFD: 

```
reset && nfd-stop ; sleep 2 && nfd-start && sleep 2 && nfdc face create udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-1.mmlab.edu.gr
```

Consumer's .7 NFD: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-2.mmlab.edu.gr
```

Consumer's .27 NFD: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://titan.cs.memphis.edu && nfdc route add / udp://titan.cs.memphis.edu && nfdc route add /localhop/nfd udp://titan.cs.memphis.edu
```

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
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 1 --producer.registry-type="MOTO"
```

Consumer .27 SeEDS Service: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 2 --producer.registry-type="SENSOR"
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

## Publish to Type 


Let's publish a IDs ``car001`` and ``car002`` to the ``CAR`` registry of the Producer: 

```sh
scn4ndn@seeds8:~$ curl -v -i -X POST http://127.0.0.1:8080 -d "id=car002&type=CAR"
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
scn4ndn@seeds8:~$ curl -v -i -X POST http://127.0.0.1:8080 -d "id=car001&type=CAR"
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
< Content-Length: 283
Content-Length: 283
< Content-Type: text/plain
Content-Type: text/plain

< 
{
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/scopeNotification:car002&CAR&SNDS_ID_scope_0",
      "payload": {
        "raw": "ACK_SUBSCRIPTION_NO_SUBSCRIBERS\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}
* Connection #0 to host 127.0.0.1 left intact
scn4ndn@seeds8:~$ 
```


Let's check the Producer Node's .8 logs for ```car001``: 

```log
[2025-10-01 11:23:02.563] [info] [HTTPServer] Received POST request for / with body id=car001&type=CAR
[2025-10-01 11:23:02.563] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car001,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
type = CAR
[2025-10-01 11:23:02.563] [info] [DigitalTwin] Received by Type get request...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 11:23:02.563] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 11:23:02.563] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 11:23:02.565] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR
[2025-10-01 11:23:02.565] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car001, CAR]
[2025-10-01 11:23:02.565] [info] [Producer] Processing Type Advertisement
[2025-10-01 11:23:02.565] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car001
[2025-10-01 11:23:02.565] [info] [Producer] [event] type_add: CAR -> car001
[2025-10-01 11:23:02.565] [info] [Producer] [resilience] mutate → ver=8 hash=16f33110a1c1a5bf47bcebe72ce1385d
[2025-10-01 11:23:02.565] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 11:23:02.568] [info] [Producer] [resilience] mutate → ver=9 hash=9a8f23204701bc3a1c8c32972dbd678a
[2025-10-01 11:23:02.568] [info] [Producer] [debug][event:type_add CAR->car001] summary → types=1 ids=1 subs(byId)=0 subs(byType)=0 announced=6
[2025-10-01 11:23:02.568] [info] [Producer] [debug][event:type_add CAR->car001] broker.journal: entries=0 cursor=0 last_fp=
[2025-10-01 11:23:02.568] [info] [Producer] Registered new ID car001 for type CAR. Sending CONTROL.
[2025-10-01 11:23:02.571] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR in function onData
[2025-10-01 11:23:02.571] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 11:23:02.571] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car001, CAR] in onData
[2025-10-01 11:23:02.571] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 11:23:02.571] [info] [Consumer] Reading packet...
[2025-10-01 11:23:02.571] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
[2025-10-01 11:23:02.571] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 11:23:02.571] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
```

Let's check the Producer Node's .8 logs for ```car002``: 

```log
[2025-10-01 11:23:49.987] [info] [HTTPServer] Received POST request for / with body id=car002&type=CAR
[2025-10-01 11:23:49.988] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car002,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car002
type = CAR
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/scopeNotification:car001&CAR&SNDS_ID_scope_0",
      "payload": {
        "raw": "ACK_SUBSCRIPTION_NO_SUBSCRIBERS\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 11:23:49.988] [info] [DigitalTwin] Received by Type get request...
[2025-10-01 11:23:49.988] [info] [HTTPServer] Read Message from queue:
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/scopeNotification:car001&CAR&SNDS_ID_scope_0",
      "payload": {
        "raw": "ACK_SUBSCRIPTION_NO_SUBSCRIBERS\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:

Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 11:23:49.988] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 11:23:49.989] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 11:23:49.989] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR
[2025-10-01 11:23:49.989] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car002, CAR]
[2025-10-01 11:23:49.989] [info] [Producer] Processing Type Advertisement
[2025-10-01 11:23:49.989] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car002
[2025-10-01 11:23:49.989] [info] [Producer] [event] type_add: CAR -> car002
[2025-10-01 11:23:49.989] [info] [Producer] [resilience] mutate → ver=10 hash=49c48570966545a9f4342171864f7147
[2025-10-01 11:23:49.990] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 11:23:49.993] [info] [Producer] [resilience] mutate → ver=11 hash=7d94b74db913b456c7e1dabf89179466
[2025-10-01 11:23:49.993] [info] [Producer] [debug][event:type_add CAR->car002] summary → types=1 ids=2 subs(byId)=0 subs(byType)=0 announced=6
[2025-10-01 11:23:49.993] [info] [Producer] [debug][event:type_add CAR->car002] broker.journal: entries=0 cursor=0 last_fp=
[2025-10-01 11:23:49.993] [info] [Producer] Registered new ID car002 for type CAR. Sending CONTROL.
[2025-10-01 11:23:49.994] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR in function onData
[2025-10-01 11:23:49.994] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 11:23:49.994] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car002, CAR] in onData
[2025-10-01 11:23:49.994] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 11:23:49.994] [info] [Consumer] Reading packet...
[2025-10-01 11:23:49.994] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
[2025-10-01 11:23:49.994] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 11:23:49.994] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
```


We can also publish to a Node's Registry from another Node entirely. Let's publish to Producer's .8 ``CAR`` registry through Consumer .7: 

```sh
scn4ndn@seeds7:~$ curl -v -i -X POST http://127.0.0.1:8080 -d "id=car003&type=CAR"
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
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR",
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

Let's check th logs of the Producer .8: 

```
[2025-10-01 11:26:29.237] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar003%26CAR
[2025-10-01 11:26:29.237] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car003, CAR]
[2025-10-01 11:26:29.237] [info] [Producer] Processing Type Advertisement
[2025-10-01 11:26:29.237] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car003
[2025-10-01 11:26:29.237] [info] [Producer] [event] type_add: CAR -> car003
[2025-10-01 11:26:29.238] [info] [Producer] [resilience] mutate → ver=12 hash=c01344c3df5cff5e09c743a3b9935dfc
[2025-10-01 11:26:29.238] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 11:26:29.240] [info] [Producer] [resilience] mutate → ver=13 hash=7f2ffa6e595b0de13f39b7f8bd5cd7e8
[2025-10-01 11:26:29.240] [info] [Producer] [debug][event:type_add CAR->car003] summary → types=1 ids=3 subs(byId)=0 subs(byType)=0 announced=6
[2025-10-01 11:26:29.240] [info] [Producer] [debug][event:type_add CAR->car003] broker.journal: entries=0 cursor=0 last_fp=
[2025-10-01 11:26:29.240] [info] [Producer] Registered new ID car003 for type CAR. Sending CONTROL.
```

Let's check the logs of the Consumer .7: 

```
[2025-10-01 11:26:29.235] [info] [HTTPServer] Received POST request for / with body id=car003&type=CAR
[2025-10-01 11:26:29.235] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car003,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car003
type = CAR
[2025-10-01 11:26:29.235] [info] [DigitalTwin] Received by Type get request...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 11:26:29.235] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 11:26:29.235] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar003%26CAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 11:26:29.243] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar003%26CAR in function onData
[2025-10-01 11:26:29.244] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 11:26:29.244] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car003, CAR] in onData
[2025-10-01 11:26:29.244] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 11:26:29.244] [info] [Consumer] Reading packet...
[2025-10-01 11:26:29.244] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
[2025-10-01 11:26:29.244] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 11:26:29.244] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
```


## Get by Type from Consumer

Let's try a GET by Type request from Consumer .7:

```sh
scn4ndn@seeds7:~$ curl -i -X GET http://localhost:8080/?"type=CAR"
HTTP/1.1 200 OK
Keep-Alive: timeout=5, max=100
Content-Length: 283
Content-Type: text/plain

{
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/scopeNotification:car003&CAR&SNDS_ID_scope_1",
      "payload": {
        "raw": "ACK_SUBSCRIPTION_NO_SUBSCRIBERS\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}
```

Let's check the logs of the Producer: 

```log
[2025-10-01 11:28:02.649] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR
[2025-10-01 11:28:02.649] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR_registry, Command: getByType, Params: [CAR]
[2025-10-01 11:28:02.649] [info] [Producer] Sending registry file payload for type CAR.
[2025-10-01 11:28:02.653] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 11:28:02.654] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car003, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 11:28:02.655] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 11:28:02.660] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 11:28:02.660] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car002, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 11:28:02.662] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 11:28:02.668] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 11:28:02.668] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car001, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 11:28:02.670] [info] [Producer] Successfully queried /temporal-query
```


Let's check the logs of the Consumer: 

```log
[2025-10-01 11:28:02.648] [info] [HTTPServer] Received GET request for / with target /?type=CAR
[2025-10-01 11:28:02.648] [info] [HTTPServer] Received GET request from 127.0.0.1
Message: {type=CAR}
Type: GET_BY_TYPE_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
type = CAR
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/scopeNotification:car003&CAR&SNDS_ID_scope_1",
      "payload": {
        "raw": "ACK_SUBSCRIPTION_NO_SUBSCRIBERS\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 11:28:02.648] [info] [HTTPServer] Read Message from queue:
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
  "batch_size": 1,
  "messages": [
    {
      "name": "/ndn/gr/edu/mmlab1/aueb/thomasi/CAR/scopeNotification:car003&CAR&SNDS_ID_scope_1",
      "payload": {
        "raw": "ACK_SUBSCRIPTION_NO_SUBSCRIBERS\n"
      },
      "sender": 6,
      "type": "CONTROL_MESSAGE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 11:28:02.648] [info] [Worker] Received content: {type=CAR}
[2025-10-01 11:28:02.648] [info] [Worker] Received payload: 
[2025-10-01 11:28:02.648] [info] [Worker] Received type: GET_BY_TYPE_REQUEST
[2025-10-01 11:28:02.648] [info] [Worker] Pushing getByType request for type: CAR_registry
[2025-10-01 11:28:02.648] [info] [Worker] Reading packet...
[2025-10-01 11:28:02.648] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 11:28:02.652] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR in function onData
[2025-10-01 11:28:02.652] [info] [Consumer] Data packet payload: Rcar003car002car001 in function onData
[2025-10-01 11:28:02.652] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR_registry, Command: getByType, Params: [CAR] in onData
[2025-10-01 11:28:02.652] [info] [Consumer] Received Registry File
[2025-10-01 11:28:02.652] [info] [Consumer] In Function: onData sending name CAR to worker Rcar003car002car001
[2025-10-01 11:28:02.652] [info] [Worker] Received content: CAR
[2025-10-01 11:28:02.652] [info] [Worker] Received payload: Rcar003car002car001
[2025-10-01 11:28:02.652] [info] [Worker] Received type: REGISTRY_PAYLOAD
[2025-10-01 11:28:02.652] [info] [Worker] Parsed ID #1: car003
[2025-10-01 11:28:02.652] [info] [Worker] Parsed ID #2: car002
[2025-10-01 11:28:02.652] [info] [Worker] Parsed ID #3: car001
[2025-10-01 11:28:02.652] [info] [Worker] Size of ids in registry file: 3
[2025-10-01 11:28:02.652] [info] [Worker] Pushing request to the consumer for id car003
[2025-10-01 11:28:02.652] [info] [Worker] Pushing request to the consumer for id car002
[2025-10-01 11:28:02.652] [info] [Worker] Pushing request to the consumer for id car001
[2025-10-01 11:28:02.652] [info] [Worker] Reading packet...
[2025-10-01 11:28:02.652] [info] [Consumer] Reading packet...
[2025-10-01 11:28:02.652] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar003&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 11:28:02.652] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 11:28:02.658] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 11:28:02.658] [info] [Consumer] Data packet payload: [
  {
    "": {
      "_id": "car003",
      "battery": 95,
      "engine_temp": 79.4,
      "id": "car003",
      "speed": 55,
      "timestamp": {
        "$date": 1758793800000
      },
      "type": "CAR"
    }
  }
] in function onData
[2025-10-01 11:28:02.658] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car003, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 11:28:02.658] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 11:28:02.658] [info] [Worker] Received content: /?id=car003&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 11:28:02.658] [info] [Worker] Received payload: [
  {
    "": {
      "_id": "car003",
      "battery": 95,
      "engine_temp": 79.4,
      "id": "car003",
      "speed": 55,
      "timestamp": {
        "$date": 1758793800000
      },
      "type": "CAR"
    }
  }
]
[2025-10-01 11:28:02.658] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 11:28:02.658] [info] [Worker] Direct mode...
[2025-10-01 11:28:02.658] [info] [Consumer] Reading packet...
[2025-10-01 11:28:02.659] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar002&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 11:28:02.659] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 11:28:02.659] [info] [Worker] Reading packet...
[2025-10-01 11:28:02.666] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 11:28:02.666] [info] [Consumer] Data packet payload: [
  {
    "": {
      "_id": "car002",
      "battery": 78,
      "engine_temp": 90.1,
      "id": "car002",
      "speed": 70,
      "timestamp": {
        "$date": 1758793200000
      },
      "type": "CAR"
    }
  }
] in function onData
[2025-10-01 11:28:02.666] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car002, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 11:28:02.666] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 11:28:02.666] [info] [Worker] Received content: /?id=car002&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 11:28:02.666] [info] [Worker] Received payload: [
  {
    "": {
      "_id": "car002",
      "battery": 78,
      "engine_temp": 90.1,
      "id": "car002",
      "speed": 70,
      "timestamp": {
        "$date": 1758793200000
      },
      "type": "CAR"
    }
  }
]
[2025-10-01 11:28:02.666] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 11:28:02.666] [info] [Consumer] Reading packet...
[2025-10-01 11:28:02.667] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar001&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 11:28:02.666] [info] [Worker] Direct mode...
[2025-10-01 11:28:02.667] [info] [Worker] Reading packet...
[2025-10-01 11:28:02.667] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 11:28:02.674] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 11:28:02.674] [info] [Consumer] Data packet payload: [
  {
    "": {
      "_id": "car001",
      "battery": 87,
      "engine_temp": 92.4,
      "id": "car001",
      "speed": 75,
      "timestamp": {
        "$date": 1758794400000
      },
      "type": "CAR"
    }
  }
] in function onData
[2025-10-01 11:28:02.674] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car001, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 11:28:02.674] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 11:28:02.674] [info] [Consumer] Reading packet...
[2025-10-01 11:28:02.675] [info] [Worker] Received content: /?id=car001&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 11:28:02.675] [info] [Worker] Received payload: [
  {
    "": {
      "_id": "car001",
      "battery": 87,
      "engine_temp": 92.4,
      "id": "car001",
      "speed": 75,
      "timestamp": {
        "$date": 1758794400000
      },
      "type": "CAR"
    }
  }
]
[2025-10-01 11:28:02.675] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 11:28:02.675] [info] [Worker] Direct mode...
[2025-10-01 11:28:02.675] [info] [Worker] Reading packet...
```


@TODO from .27