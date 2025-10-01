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

Producer's certs: 

```
ndn6-serve-certs --inter certs/*.cert
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
[2025-10-01 13:49:06.864] [info] [HTTPServer] Received POST request for / with body id=car001
[2025-10-01 13:49:06.864] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car001}
Type: BY_ID_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
[2025-10-01 13:49:06.864] [debug] [HTTPServer] Writing message from POST request to service queue...
Message: {id=car001}
Type: BY_ID_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001

[2025-10-01 13:49:06.864] [debug] [DigitalTwin] Extracted content from message {id=car001}
[2025-10-01 13:49:06.864] [debug] [DigitalTwin] Extracted content length from message 11
[2025-10-01 13:49:06.864] [debug] [DigitalTwin] Extracted type from message: BY_ID_ADVERTISEMENT
[2025-10-01 13:49:06.864] [info] [DigitalTwin] Received by ID get request...
[2025-10-01 13:49:06.864] [debug] [DigitalTwin] Pushed ID car001 to the announced IDs vector...
[2025-10-01 13:49:06.864] [debug] [DigitalTwin] Extracted id Scope from message SNDS_ID_scope_2
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:49:06.864] [info] [DigitalTwin] Pushing byID advertisement to the Producer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:49:06.864] [debug] [DigitalTwin] Sleeping 2000 ms before sending scope advertisement...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:49:06.893] [info] [Producer] Got new ID advertisement request...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/postID:car001
Type: BY_ID_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:49:06.893] [debug] [Producer] Parsed interest Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: postID, Params: [car001] from string
[2025-10-01 13:49:06.893] [info] [Producer] Announcing name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001
[2025-10-01 13:49:06.894] [info] [Producer] Publishing to broker...
[2025-10-01 13:49:06.894] [warning] [Producer] _publish_to_broker: 'type' (string) not provided...
[2025-10-01 13:49:06.894] [info] [Producer] POST /add -> {"id":"car001"}
[2025-10-01 13:49:07.082] [info] [Producer] Published to broker OK: {"status": "success", "inserted_count_versioned": 1, "upserted_or_modified_count": 1}

[2025-10-01 13:49:07.082] [debug] [Producer] Appending body to broker journal: {"id":"car001"}
[2025-10-01 13:49:07.082] [info] [Producer] [resilience] mutate → ver=8 hash=a96596e77b8ca3a9df6378a6ef9a4362
[2025-10-01 13:49:07.082] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(5b784e73) 2(851bb2ce) 3(f2dc8068) 4(1dbec4b9) 5(92323da8) 6(a8c74cc9) 7(11cc7dd5) 8(a96596e7) 
[2025-10-01 13:49:07.082] [debug] [Producer] [resilience] ops from v7 → v8:
[
  {
    "op": "add",
    "path": "/broker/journal",
    "value": [
      {
        "id": "car001",
        "payload": {
          "id": "car001"
        },
        "ts": "2025-10-01T13:49:07Z",
        "type": "GENERIC"
      }
    ]
  }
]
[2025-10-01 13:49:07.083] [info] [Worker] Received content: 
[2025-10-01 13:49:07.083] [info] [Worker] Received payload: ACK_ID_ADVERTISED
[2025-10-01 13:49:07.083] [info] [Worker] Received type: BY_ID_ADVERTISEMENT
[2025-10-01 13:49:07.083] [info] [Worker] Received ACK Message with name:  payload: ACK_ID_ADVERTISED...
Message: 
Type: CONTROL_MESSAGE
Sender: WORKER
Payload: ACK_ID_ADVERTISED

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:49:07.083] [debug] [Worker] Writing message to service queue:
Message: 
Type: CONTROL_MESSAGE
Sender: WORKER
Payload: ACK_ID_ADVERTISED

HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:49:07.083] [info] [Worker] Reading packet...
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
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

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:49:07.083] [info] [HTTPServer] Read Message from queue:
Message: BATCH
Type: GENERIC
Sender: WORKER
Payload: {
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

HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:49:07.087] [info] [Producer] Successfully registered prefix: /ndn/gr/edu/mmlab1/aueb/thomasi/car001
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
            "date": 1759326546992,
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

Nice we got the ID. Let's make sure we actually got it from the Producer Node .8. Let's check the logs of the Producer .8: 

```log
[2025-10-01 13:50:41.971] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:50:41.971] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: temporalQuery, Params: [id=car001, type=, date=, count=, timeframe=, filters=]
[2025-10-01 13:50:41.971] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:50:41.971] [debug] [Producer] TemporalQuery → id: car001, date: , type: , timeframe: , count: , filters: 
[2025-10-01 13:50:41.971] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car001&type=&count=&date=&timeframe=&filters=
[2025-10-01 13:50:41.972] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:50:41.972] [debug] [Producer] Sending data to the NDN…
```

Let's also check the logs of the Consumer .7: 

```log
[2025-10-01 13:50:41.969] [info] [HTTPServer] Received GET request for / with body 
[2025-10-01 13:50:41.969] [info] [HTTPServer] Received GET request for / with target /?id=car001
[2025-10-01 13:50:41.969] [info] [HTTPServer] Received GET request from 127.0.0.1
Message: {id=car001}
Type: GET_BY_ID_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
id = car001
[2025-10-01 13:50:41.969] [debug] [HTTPServer] Writing message from GET request to service queue...
Message: {id=car001}
Type: GET_BY_ID_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
id = car001

[2025-10-01 13:50:41.969] [info] [Worker] Received content: {id=car001}
[2025-10-01 13:50:41.969] [info] [Worker] Received payload: 
[2025-10-01 13:50:41.969] [info] [Worker] Received type: GET_BY_ID_REQUEST
[2025-10-01 13:50:41.969] [debug] [Worker] Server intra message...
[2025-10-01 13:50:41.969] [info] [Worker] Inserted ID: car001 into set. Pushing regular getByID request for ID: car001
[2025-10-01 13:50:41.969] [info] [Worker] Reading packet...
[2025-10-01 13:50:41.969] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/getByID:car001
[2025-10-01 13:50:41.969] [debug] [Consumer] Extracted content length from message: 53
[2025-10-01 13:50:41.969] [debug] [Consumer] Extracted message type from message: GET_BY_ID_REQUEST
[2025-10-01 13:50:41.969] [debug] [Consumer] Query.type: 
[2025-10-01 13:50:41.969] [debug] [Consumer] Query.id: car001
[2025-10-01 13:50:41.969] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery:id%3Dcar001&type%3D&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:50:41.969] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 13:50:41.977] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:50:41.977] [info] [Consumer] Data packet payload: [
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
] in function onData
[2025-10-01 13:50:41.977] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: temporalQuery, Params: [id=car001, type=, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:50:41.977] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:50:41.977] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:50:41.977] [debug] [Consumer] Candidate JSON slice:
[
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 13:50:41.977] [debug] [Consumer] Sanitized payload to:
[
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 13:50:41.977] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:50:41.977] [debug] [Consumer] Got ID and type in direct: id=car001,type=
[2025-10-01 13:50:41.978] [info] [Worker] Received content: /?id=car001&type=&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:50:41.978] [info] [Worker] Received payload: [
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 13:50:41.978] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:50:41.978] [debug] [Worker] Consumer intra message...
[2025-10-01 13:50:41.978] [info] [Worker] Direct mode...
[2025-10-01 13:50:41.978] [info] [Consumer] Reading packet...
[2025-10-01 13:50:41.978] [debug] [Worker] Cleared pending_get_by_id_requests for 'car001': erased=1
[2025-10-01 13:50:41.978] [debug] [Worker] Cleared pending_get_by_type_requests for 'GENERIC': erased=0
[2025-10-01 13:50:41.978] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:50:41.978] [info] [Worker] Reading packet...
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
            "date": 1759326546992,
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

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:50:41.978] [info] [HTTPServer] Read Message from queue:
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
            "date": 1759326546992,
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

HTTP Type: HTTP_EMPTY
Params:
```

Let's try and get it from .27:

```sh
scn4ndn@seeds23:~$ curl -i -X GET http://localhost:8080/?"id=car001"
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
            "date": 1759326546992,
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
scn4ndn@seeds23:~$ 
```


Let's check the producer's .8 logs: 


```log
[2025-10-01 13:53:00.664] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:53:00.664] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: temporalQuery, Params: [id=car001, type=, date=, count=, timeframe=, filters=]
[2025-10-01 13:53:00.664] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:53:00.664] [debug] [Producer] TemporalQuery → id: car001, date: , type: , timeframe: , count: , filters: 
[2025-10-01 13:53:00.664] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car001&type=&count=&date=&timeframe=&filters=
[2025-10-01 13:53:00.668] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:53:00.669] [debug] [Producer] Sending data to the NDN…
```

Let's check the consumer's .27 logs: 

```log
[2025-10-01 13:53:00.501] [info] [HTTPServer] Received GET request for / with body 
[2025-10-01 13:53:00.501] [info] [HTTPServer] Received GET request for / with target /?id=car001
[2025-10-01 13:53:00.501] [info] [HTTPServer] Received GET request from 127.0.0.1
Message: {id=car001}
Type: GET_BY_ID_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
id = car001
[2025-10-01 13:53:00.501] [debug] [HTTPServer] Writing message from GET request to service queue...
Message: {id=car001}
Type: GET_BY_ID_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
id = car001

[2025-10-01 13:53:00.501] [info] [Worker] Received content: {id=car001}
[2025-10-01 13:53:00.501] [info] [Worker] Received payload: 
[2025-10-01 13:53:00.501] [info] [Worker] Received type: GET_BY_ID_REQUEST
[2025-10-01 13:53:00.501] [debug] [Worker] Server intra message...
[2025-10-01 13:53:00.501] [info] [Worker] Inserted ID: car001 into set. Pushing regular getByID request for ID: car001
[2025-10-01 13:53:00.501] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/getByID:car001
[2025-10-01 13:53:00.501] [info] [Worker] Reading packet...
[2025-10-01 13:53:00.501] [debug] [Consumer] Extracted content length from message: 53
[2025-10-01 13:53:00.501] [debug] [Consumer] Extracted message type from message: GET_BY_ID_REQUEST
[2025-10-01 13:53:00.501] [debug] [Consumer] Query.type: 
[2025-10-01 13:53:00.501] [debug] [Consumer] Query.id: car001
[2025-10-01 13:53:00.501] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery:id%3Dcar001&type%3D&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:53:00.501] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:53:00.833] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/car001/temporalQuery%3Aid%3Dcar001%26type%3D%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:53:00.833] [info] [Consumer] Data packet payload: [
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
] in function onData
[2025-10-01 13:53:00.834] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: car001, Command: temporalQuery, Params: [id=car001, type=, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:53:00.834] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:53:00.834] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:53:00.834] [debug] [Consumer] Candidate JSON slice:
[
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 13:53:00.834] [debug] [Consumer] Sanitized payload to:
[
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 13:53:00.834] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:53:00.834] [debug] [Consumer] Got ID and type in direct: id=car001,type=
[2025-10-01 13:53:00.834] [info] [Consumer] Reading packet...
[2025-10-01 13:53:00.834] [info] [Worker] Received content: /?id=car001&type=&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:53:00.834] [info] [Worker] Received payload: [
  {
    "": {
      "_id": "car001",
      "id": "car001",
      "timestamp": {
        "$date": 1759326546992
      },
      "type": "GENERIC"
    }
  }
]
[2025-10-01 13:53:00.834] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:53:00.834] [debug] [Worker] Consumer intra message...
[2025-10-01 13:53:00.834] [info] [Worker] Direct mode...
[2025-10-01 13:53:00.834] [debug] [Worker] Cleared pending_get_by_id_requests for 'car001': erased=1
[2025-10-01 13:53:00.835] [debug] [Worker] Cleared pending_get_by_type_requests for 'GENERIC': erased=0
[2025-10-01 13:53:00.835] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:53:00.835] [info] [Worker] Reading packet...
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
            "date": 1759326546992,
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

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:53:00.835] [info] [HTTPServer] Read Message from queue:
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
            "date": 1759326546992,
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

HTTP Type: HTTP_EMPTY
Params:
```

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
[2025-10-01 13:37:32.755] [info] [HTTPServer] Received POST request for / with body id=car001&type=CAR
[2025-10-01 13:37:32.755] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car001,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
type = CAR
[2025-10-01 13:37:32.755] [debug] [HTTPServer] Writing message from POST request to service queue...
Message: {id=car001,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car001
type = CAR

[2025-10-01 13:37:32.755] [debug] [DigitalTwin] Extracted content from message {id=car001,type=CAR}
[2025-10-01 13:37:32.755] [debug] [DigitalTwin] Extracted content length from message 20
[2025-10-01 13:37:32.755] [debug] [DigitalTwin] Extracted type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 13:37:32.755] [info] [DigitalTwin] Received by Type get request...
[2025-10-01 13:37:32.755] [debug] [DigitalTwin] The Type is the following: CAR
[2025-10-01 13:37:32.755] [debug] [DigitalTwin] The ID is the following: car001
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:37:32.755] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:37:32.755] [debug] [DigitalTwin] Sleeping 2000 ms before sending scope advertisement...
[2025-10-01 13:37:32.755] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
[2025-10-01 13:37:32.755] [debug] [Consumer] Extracted content length from message: 71
[2025-10-01 13:37:32.755] [debug] [Consumer] Extracted message type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 13:37:32.755] [debug] [Consumer] Created name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR in func: express_by_id_and_type_advertisement_interest
[2025-10-01 13:37:32.755] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:37:32.756] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR
[2025-10-01 13:37:32.756] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car001, CAR]
[2025-10-01 13:37:32.756] [info] [Producer] Processing Type Advertisement
[2025-10-01 13:37:32.756] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car001
[2025-10-01 13:37:32.756] [info] [Producer] [event] type_add: CAR -> car001
[2025-10-01 13:37:32.756] [info] [Producer] [resilience] mutate → ver=8 hash=0c1d2fdbfecac4ed4a5374ed83f8a7d8
[2025-10-01 13:37:32.756] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(630cd269) 2(2bcd1ae9) 3(c9dc1836) 4(9a5a300d) 5(c872b4e2) 6(f1eb6fbc) 7(b5972495) 8(0c1d2fdb) 
[2025-10-01 13:37:32.756] [debug] [Producer] [resilience] ops from v7 → v8:
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
[2025-10-01 13:37:32.756] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 13:37:32.756] [debug] [Producer] HTTP GET /state?type=CAR
[2025-10-01 13:37:32.760] [info] [Producer] [resilience] mutate → ver=9 hash=231555f1734d0c307e5320479fcc0c9f
[2025-10-01 13:37:32.760] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(630cd269) 2(2bcd1ae9) 3(c9dc1836) 4(9a5a300d) 5(c872b4e2) 6(f1eb6fbc) 7(b5972495) 8(0c1d2fdb) 9(231555f1) 
[2025-10-01 13:37:32.760] [debug] [Producer] [resilience] ops from v8 → v9:
[
  {
    "op": "add",
    "path": "/broker/collections/CAR",
    "value": {
      "algo": "SHA-256",
      "count": 3,
      "hash": "b66cd183738b81929b9632a47631dace78dd98715418434d4f92be99a6ec75c1",
      "lastSeen": "2025-10-01T13:37:32Z"
    }
  }
]
[2025-10-01 13:37:32.761] [info] [Producer] [debug][event:type_add CAR->car001] summary → types=1 ids=1 subs(byId)=0 subs(byType)=0 announced=6
[2025-10-01 13:37:32.761] [debug] [Producer] [debug][event:type_add CAR->car001] type_registry:
[2025-10-01 13:37:32.761] [debug] [Producer]   [CAR] car001 
[2025-10-01 13:37:32.761] [debug] [Producer] [debug][event:type_add CAR->car001] subscribed_nodes_for_id:
[2025-10-01 13:37:32.761] [debug] [Producer] [debug][event:type_add CAR->car001] subscribed_nodes_for_type:
[2025-10-01 13:37:32.761] [debug] [Producer] [debug][event:type_add CAR->car001] announcements: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry /ndn/gr/edu/mmlab1/aueb/thomasi/CAR /ndn/gr/edu/mmlab1/aueb/thomasi/state_0 /ndn/gr/edu/mmlab1/aueb/thomasi/is_alive_0 /ndn/gr/edu/mmlab1/aueb/thomasi/SNDS_ID_scope_0 /ndn/gr/edu/mmlab1/aueb/thomasi_0 
[2025-10-01 13:37:32.761] [info] [Producer] [debug][event:type_add CAR->car001] broker.journal: entries=0 cursor=0 last_fp=
[2025-10-01 13:37:32.761] [debug] [Producer] [overlay][event:type_add CAR->car001] ver=0 hash=d41d8cd98f00b204e9800998ecf8427e keys=0
[2025-10-01 13:37:32.761] [info] [Producer] Registered new ID car001 for type CAR. Sending CONTROL.
[2025-10-01 13:37:32.761] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:37:32.764] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar001%26CAR in function onData
[2025-10-01 13:37:32.764] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 13:37:32.764] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car001, CAR] in onData
[2025-10-01 13:37:32.764] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 13:37:32.764] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car001&CAR
[2025-10-01 13:37:32.764] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 13:37:32.765] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
```

Let's check the Producer Node's .8 logs for ```car002``: 

```log
[2025-10-01 13:38:23.395] [info] [HTTPServer] Received POST request for / with body id=car002&type=CAR
[2025-10-01 13:38:23.395] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car002,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car002
type = CAR
[2025-10-01 13:38:23.395] [debug] [HTTPServer] Writing message from POST request to service queue...
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
[2025-10-01 13:38:23.395] [debug] [DigitalTwin] Extracted content from message {id=car002,type=CAR}
[2025-10-01 13:38:23.395] [debug] [DigitalTwin] Extracted content length from message 20
[2025-10-01 13:38:23.395] [debug] [DigitalTwin] Extracted type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 13:38:23.395] [info] [DigitalTwin] Received by Type get request...
[2025-10-01 13:38:23.395] [info] [HTTPServer] Read Message from queue:
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

[2025-10-01 13:38:23.395] [debug] [DigitalTwin] The Type is the following: CAR
[2025-10-01 13:38:23.395] [debug] [DigitalTwin] The ID is the following: car002
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:38:23.395] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:38:23.396] [debug] [DigitalTwin] Sleeping 2000 ms before sending scope advertisement...
[2025-10-01 13:38:23.396] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
[2025-10-01 13:38:23.396] [debug] [Consumer] Extracted content length from message: 71
[2025-10-01 13:38:23.396] [debug] [Consumer] Extracted message type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 13:38:23.396] [debug] [Consumer] Created name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR in func: express_by_id_and_type_advertisement_interest
[2025-10-01 13:38:23.396] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:38:23.396] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR
[2025-10-01 13:38:23.396] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car002, CAR]
[2025-10-01 13:38:23.396] [info] [Producer] Processing Type Advertisement
[2025-10-01 13:38:23.396] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car002
[2025-10-01 13:38:23.396] [info] [Producer] [event] type_add: CAR -> car002
[2025-10-01 13:38:23.397] [info] [Producer] [resilience] mutate → ver=10 hash=5de9c8ab762e109fd4b0a985de005a49
[2025-10-01 13:38:23.397] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(630cd269) 2(2bcd1ae9) 3(c9dc1836) 4(9a5a300d) 5(c872b4e2) 6(f1eb6fbc) 7(b5972495) 8(0c1d2fdb) 9(231555f1) 10(5de9c8ab) 
[2025-10-01 13:38:23.397] [debug] [Producer] [resilience] ops from v9 → v10:
[
  {
    "op": "add",
    "path": "/types/CAR/ids/car002",
    "value": true
  }
]
[2025-10-01 13:38:23.397] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 13:38:23.397] [debug] [Producer] HTTP GET /state?type=CAR
[2025-10-01 13:38:23.399] [info] [Producer] [resilience] mutate → ver=11 hash=c84debb2b5768c8bb334fa056dd1a216
[2025-10-01 13:38:23.400] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(630cd269) 2(2bcd1ae9) 3(c9dc1836) 4(9a5a300d) 5(c872b4e2) 6(f1eb6fbc) 7(b5972495) 8(0c1d2fdb) 9(231555f1) 10(5de9c8ab) 11(c84debb2) 
[2025-10-01 13:38:23.400] [debug] [Producer] [resilience] ops from v10 → v11:
[
  {
    "op": "replace",
    "path": "/broker/collections/CAR/lastSeen",
    "value": "2025-10-01T13:38:23Z"
  }
]
[2025-10-01 13:38:23.400] [info] [Producer] [debug][event:type_add CAR->car002] summary → types=1 ids=2 subs(byId)=0 subs(byType)=0 announced=6
[2025-10-01 13:38:23.400] [debug] [Producer] [debug][event:type_add CAR->car002] type_registry:
[2025-10-01 13:38:23.400] [debug] [Producer]   [CAR] car002 car001 
[2025-10-01 13:38:23.400] [debug] [Producer] [debug][event:type_add CAR->car002] subscribed_nodes_for_id:
[2025-10-01 13:38:23.400] [debug] [Producer] [debug][event:type_add CAR->car002] subscribed_nodes_for_type:
[2025-10-01 13:38:23.400] [debug] [Producer] [debug][event:type_add CAR->car002] announcements: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry /ndn/gr/edu/mmlab1/aueb/thomasi/CAR /ndn/gr/edu/mmlab1/aueb/thomasi/state_0 /ndn/gr/edu/mmlab1/aueb/thomasi/is_alive_0 /ndn/gr/edu/mmlab1/aueb/thomasi/SNDS_ID_scope_0 /ndn/gr/edu/mmlab1/aueb/thomasi_0 
[2025-10-01 13:38:23.400] [info] [Producer] [debug][event:type_add CAR->car002] broker.journal: entries=0 cursor=0 last_fp=
[2025-10-01 13:38:23.400] [debug] [Producer] [overlay][event:type_add CAR->car002] ver=0 hash=d41d8cd98f00b204e9800998ecf8427e keys=0
[2025-10-01 13:38:23.400] [info] [Producer] Registered new ID car002 for type CAR. Sending CONTROL.
[2025-10-01 13:38:23.400] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:38:23.401] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar002%26CAR in function onData
[2025-10-01 13:38:23.401] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 13:38:23.401] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car002, CAR] in onData
[2025-10-01 13:38:23.401] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 13:38:23.401] [info] [Consumer] Reading packet...
[2025-10-01 13:38:23.401] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car002&CAR
[2025-10-01 13:38:23.401] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 13:38:23.401] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
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

Let's check the logs of the Producer .8: 

```log
[2025-10-01 13:41:27.555] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar003%26CAR
[2025-10-01 13:41:27.555] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car003, CAR]
[2025-10-01 13:41:27.555] [info] [Producer] Processing Type Advertisement
[2025-10-01 13:41:27.555] [info] [Producer] Type advertisement: name(registry) = CAR, ID = car003
[2025-10-01 13:41:27.555] [info] [Producer] [event] type_add: CAR -> car003
[2025-10-01 13:41:27.555] [info] [Producer] [resilience] mutate → ver=12 hash=a029fd95434064b0a2deb814c82214c0
[2025-10-01 13:41:27.555] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(630cd269) 2(2bcd1ae9) 3(c9dc1836) 4(9a5a300d) 5(c872b4e2) 6(f1eb6fbc) 7(b5972495) 8(0c1d2fdb) 9(231555f1) 10(5de9c8ab) 11(c84debb2) 12(a029fd95) 
[2025-10-01 13:41:27.555] [debug] [Producer] [resilience] ops from v11 → v12:
[
  {
    "op": "add",
    "path": "/types/CAR/ids/car003",
    "value": true
  }
]
[2025-10-01 13:41:27.555] [info] [Producer] Getting from IP: 127.0.0.1 and port: 10000
[2025-10-01 13:41:27.555] [debug] [Producer] HTTP GET /state?type=CAR
[2025-10-01 13:41:27.558] [info] [Producer] [resilience] mutate → ver=13 hash=e6602199b9cb6dd9a1fb37adcffa2c1d
[2025-10-01 13:41:27.558] [debug] [Producer] [resilience] history versions: 0(37a6259c) 1(630cd269) 2(2bcd1ae9) 3(c9dc1836) 4(9a5a300d) 5(c872b4e2) 6(f1eb6fbc) 7(b5972495) 8(0c1d2fdb) 9(231555f1) 10(5de9c8ab) 11(c84debb2) 12(a029fd95) 13(e6602199) 
[2025-10-01 13:41:27.558] [debug] [Producer] [resilience] ops from v12 → v13:
[
  {
    "op": "replace",
    "path": "/broker/collections/CAR/lastSeen",
    "value": "2025-10-01T13:41:27Z"
  }
]
[2025-10-01 13:41:27.559] [info] [Producer] [debug][event:type_add CAR->car003] summary → types=1 ids=3 subs(byId)=0 subs(byType)=0 announced=6
[2025-10-01 13:41:27.559] [debug] [Producer] [debug][event:type_add CAR->car003] type_registry:
[2025-10-01 13:41:27.559] [debug] [Producer]   [CAR] car003 car002 car001 
[2025-10-01 13:41:27.559] [debug] [Producer] [debug][event:type_add CAR->car003] subscribed_nodes_for_id:
[2025-10-01 13:41:27.559] [debug] [Producer] [debug][event:type_add CAR->car003] subscribed_nodes_for_type:
[2025-10-01 13:41:27.559] [debug] [Producer] [debug][event:type_add CAR->car003] announcements: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry /ndn/gr/edu/mmlab1/aueb/thomasi/CAR /ndn/gr/edu/mmlab1/aueb/thomasi/state_0 /ndn/gr/edu/mmlab1/aueb/thomasi/is_alive_0 /ndn/gr/edu/mmlab1/aueb/thomasi/SNDS_ID_scope_0 /ndn/gr/edu/mmlab1/aueb/thomasi_0 
[2025-10-01 13:41:27.559] [info] [Producer] [debug][event:type_add CAR->car003] broker.journal: entries=0 cursor=0 last_fp=
[2025-10-01 13:41:27.559] [debug] [Producer] [overlay][event:type_add CAR->car003] ver=0 hash=d41d8cd98f00b204e9800998ecf8427e keys=0
[2025-10-01 13:41:27.559] [info] [Producer] Registered new ID car003 for type CAR. Sending CONTROL.
[2025-10-01 13:41:27.559] [debug] [Producer] Sending data to the NDN…
```

Let's check the logs of the Consumer .7: 

```log
[2025-10-01 13:41:27.551] [info] [HTTPServer] Received POST request for / with body id=car003&type=CAR
[2025-10-01 13:41:27.551] [info] [HTTPServer] Received POST request from 127.0.0.1
Message: {id=car003,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car003
type = CAR
[2025-10-01 13:41:27.551] [debug] [HTTPServer] Writing message from POST request to service queue...
Message: {id=car003,type=CAR}
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_POST
Params:
id = car003
type = CAR

[2025-10-01 13:41:27.551] [debug] [DigitalTwin] Extracted content from message {id=car003,type=CAR}
[2025-10-01 13:41:27.551] [debug] [DigitalTwin] Extracted content length from message 20
[2025-10-01 13:41:27.551] [debug] [DigitalTwin] Extracted type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 13:41:27.551] [info] [DigitalTwin] Received by Type get request...
[2025-10-01 13:41:27.551] [debug] [DigitalTwin] The Type is the following: CAR
[2025-10-01 13:41:27.551] [debug] [DigitalTwin] The ID is the following: car003
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:41:27.551] [info] [DigitalTwin] Pushing byType advertisement to the Consumer...
Message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
Type: BY_ID_AND_TYPE_ADVERTISEMENT
Sender: DIGITAL_TWIN
Payload: 
HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:41:27.551] [debug] [DigitalTwin] Sleeping 2000 ms before sending scope advertisement...
[2025-10-01 13:41:27.551] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
[2025-10-01 13:41:27.551] [debug] [Consumer] Extracted content length from message: 71
[2025-10-01 13:41:27.551] [debug] [Consumer] Extracted message type from message: BY_ID_AND_TYPE_ADVERTISEMENT
[2025-10-01 13:41:27.551] [debug] [Consumer] Created name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR in func: express_by_id_and_type_advertisement_interest
[2025-10-01 13:41:27.551] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar003%26CAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:41:27.562] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement%3Acar003%26CAR in function onData
[2025-10-01 13:41:27.562] [info] [Consumer] Data packet payload: ACK_ID_REGISTERED in function onData
[2025-10-01 13:41:27.562] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: byIDAndTypeAdvertisement, Params: [car003, CAR] in onData
[2025-10-01 13:41:27.562] [info] [Consumer] Got ACK ACK_ID_REGISTERED. Skipping...
[2025-10-01 13:41:27.562] [info] [Consumer] Reading packet...
[2025-10-01 13:41:27.562] [info] [Worker] Received content: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/byIDAndTypeAdvertisement:car003&CAR
[2025-10-01 13:41:27.562] [info] [Worker] Received payload: ACK_ID_REGISTERED
[2025-10-01 13:41:27.562] [info] [Worker] Received type: GET_BY_ID_FOR_TYPE_RESPONSE
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
[2025-10-01 13:43:29.560] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR
[2025-10-01 13:43:29.560] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR_registry, Command: getByType, Params: [CAR]
[2025-10-01 13:43:29.560] [info] [Producer] Sending registry file payload for type CAR.
[2025-10-01 13:43:29.560] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:43:29.567] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:43:29.567] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car003, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 13:43:29.567] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:43:29.567] [debug] [Producer] TemporalQuery → id: car003, date: , type: CAR, timeframe: , count: , filters: 
[2025-10-01 13:43:29.567] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car003&type=CAR&count=&date=&timeframe=&filters=
[2025-10-01 13:43:29.571] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:43:29.571] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:43:29.579] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:43:29.579] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car002, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 13:43:29.579] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:43:29.579] [debug] [Producer] TemporalQuery → id: car002, date: , type: CAR, timeframe: , count: , filters: 
[2025-10-01 13:43:29.579] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car002&type=CAR&count=&date=&timeframe=&filters=
[2025-10-01 13:43:29.580] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:43:29.580] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:43:29.584] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:43:29.584] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car001, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 13:43:29.584] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:43:29.584] [debug] [Producer] TemporalQuery → id: car001, date: , type: CAR, timeframe: , count: , filters: 
[2025-10-01 13:43:29.584] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car001&type=CAR&count=&date=&timeframe=&filters=
[2025-10-01 13:43:29.585] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:43:29.585] [debug] [Producer] Sending data to the NDN…
```


Let's check the logs of the Consumer: 

```log
[2025-10-01 13:43:29.557] [info] [HTTPServer] Received GET request for / with target /?type=CAR
[2025-10-01 13:43:29.557] [info] [HTTPServer] Received GET request from 127.0.0.1
Message: {type=CAR}
Type: GET_BY_TYPE_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
type = CAR
[2025-10-01 13:43:29.557] [debug] [HTTPServer] Writing message from GET request to service queue...
Message: {type=CAR}
Type: GET_BY_TYPE_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
type = CAR

[2025-10-01 13:43:29.557] [info] [Worker] Received content: {type=CAR}
[2025-10-01 13:43:29.557] [info] [Worker] Received payload: 
[2025-10-01 13:43:29.557] [info] [Worker] Received type: GET_BY_TYPE_REQUEST
[2025-10-01 13:43:29.557] [debug] [Worker] Server intra message...
[2025-10-01 13:43:29.557] [info] [Worker] Pushing getByType request for type: CAR_registry
[2025-10-01 13:43:29.557] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType:CAR
[2025-10-01 13:43:29.557] [info] [Worker] Reading packet...
[2025-10-01 13:43:29.557] [debug] [Consumer] Extracted content length from message: 58
[2025-10-01 13:43:29.557] [debug] [Consumer] Extracted message type from message: GET_BY_TYPE_REQUEST
[2025-10-01 13:43:29.557] [debug] [Consumer] Created name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType:CAR in func: express_get_by_type_interest
[2025-10-01 13:43:29.557] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 13:43:29.563] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR in function onData
[2025-10-01 13:43:29.563] [info] [Consumer] Data packet payload: Rcar003car002car001 in function onData
[2025-10-01 13:43:29.563] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR_registry, Command: getByType, Params: [CAR] in onData
[2025-10-01 13:43:29.563] [info] [Consumer] Received Registry File
[2025-10-01 13:43:29.564] [info] [Consumer] In Function: onData sending name CAR to worker Rcar003car002car001
[2025-10-01 13:43:29.564] [info] [Worker] Received content: CAR
[2025-10-01 13:43:29.564] [info] [Worker] Received payload: Rcar003car002car001
[2025-10-01 13:43:29.564] [info] [Worker] Received type: REGISTRY_PAYLOAD
[2025-10-01 13:43:29.564] [info] [Consumer] Reading packet...
[2025-10-01 13:43:29.564] [debug] [Worker] Consumer intra message...
[2025-10-01 13:43:29.564] [debug] [Worker] Number of IDs in payload: 3
[2025-10-01 13:43:29.564] [debug] [Worker] Reading ID #1 with size 6
[2025-10-01 13:43:29.564] [info] [Worker] Parsed ID #1: car003
[2025-10-01 13:43:29.564] [debug] [Worker] Reading ID #2 with size 6
[2025-10-01 13:43:29.564] [info] [Worker] Parsed ID #2: car002
[2025-10-01 13:43:29.564] [debug] [Worker] Reading ID #3 with size 6
[2025-10-01 13:43:29.564] [info] [Worker] Parsed ID #3: car001
[2025-10-01 13:43:29.564] [info] [Worker] Size of ids in registry file: 3
[2025-10-01 13:43:29.564] [debug] [Worker] Processing registry id: car003
[2025-10-01 13:43:29.564] [info] [Worker] Pushing request to the consumer for id car003
[2025-10-01 13:43:29.564] [debug] [Worker] Processing registry id: car002
[2025-10-01 13:43:29.564] [info] [Worker] Pushing request to the consumer for id car002
[2025-10-01 13:43:29.564] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/getByIDForType:car003&CAR
[2025-10-01 13:43:29.564] [debug] [Consumer] Extracted content length from message: 61
[2025-10-01 13:43:29.564] [debug] [Consumer] Extracted message type from message: GET_BY_ID_FOR_TYPE_REQUEST
[2025-10-01 13:43:29.564] [debug] [Consumer] Query.type: CAR
[2025-10-01 13:43:29.564] [debug] [Consumer] Query.id: car003
[2025-10-01 13:43:29.564] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar003&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:43:29.564] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 13:43:29.564] [debug] [Worker] Processing registry id: car001
[2025-10-01 13:43:29.565] [info] [Worker] Pushing request to the consumer for id car001
[2025-10-01 13:43:29.565] [info] [Worker] Reading packet...
[2025-10-01 13:43:29.576] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:43:29.576] [info] [Consumer] Data packet payload: [
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
[2025-10-01 13:43:29.576] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car003, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:43:29.576] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:43:29.576] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:43:29.576] [debug] [Consumer] Candidate JSON slice:
[
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
[2025-10-01 13:43:29.576] [debug] [Consumer] Sanitized payload to:
[
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
[2025-10-01 13:43:29.576] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:43:29.576] [debug] [Consumer] Got ID and type in direct: id=car003,type=CAR
[2025-10-01 13:43:29.576] [info] [Worker] Received content: /?id=car003&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:43:29.576] [info] [Worker] Received payload: [
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
[2025-10-01 13:43:29.577] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:43:29.577] [debug] [Worker] Consumer intra message...
[2025-10-01 13:43:29.577] [info] [Worker] Direct mode...
[2025-10-01 13:43:29.577] [debug] [Worker] Cleared pending_get_by_id_requests for 'car003': erased=0
[2025-10-01 13:43:29.577] [info] [Consumer] Reading packet...
[2025-10-01 13:43:29.577] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/getByIDForType:car002&CAR
[2025-10-01 13:43:29.577] [debug] [Worker] Cleared pending_get_by_type_requests for 'CAR': erased=1
[2025-10-01 13:43:29.577] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:43:29.577] [info] [Worker] Reading packet...
[2025-10-01 13:43:29.577] [debug] [Consumer] Extracted content length from message: 61
[2025-10-01 13:43:29.577] [debug] [Consumer] Extracted message type from message: GET_BY_ID_FOR_TYPE_REQUEST
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
            "date": 1758793800000,
            "id": "car003",
            "type": "CAR"
          }
        ],
        "mode": "direct"
      },
      "sender": 6,
      "type": "TEMPORAL_QUERY_RESPONSE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:43:29.577] [debug] [Consumer] Query.type: CAR
[2025-10-01 13:43:29.577] [info] [HTTPServer] Read Message from queue:
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
            "date": 1758793800000,
            "id": "car003",
            "type": "CAR"
          }
        ],
        "mode": "direct"
      },
      "sender": 6,
      "type": "TEMPORAL_QUERY_RESPONSE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:43:29.577] [debug] [Consumer] Query.id: car002
[2025-10-01 13:43:29.577] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar002&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:43:29.577] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 13:43:29.583] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:43:29.583] [info] [Consumer] Data packet payload: [
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
[2025-10-01 13:43:29.583] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car002, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:43:29.583] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:43:29.583] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:43:29.583] [debug] [Consumer] Candidate JSON slice:
[
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
[2025-10-01 13:43:29.583] [debug] [Consumer] Sanitized payload to:
[
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
[2025-10-01 13:43:29.583] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:43:29.583] [debug] [Consumer] Got ID and type in direct: id=car002,type=CAR
[2025-10-01 13:43:29.583] [info] [Worker] Received content: /?id=car002&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:43:29.583] [info] [Worker] Received payload: [
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
[2025-10-01 13:43:29.583] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:43:29.583] [debug] [Worker] Consumer intra message...
[2025-10-01 13:43:29.583] [info] [Worker] Direct mode...
[2025-10-01 13:43:29.583] [info] [Consumer] Reading packet...
[2025-10-01 13:43:29.583] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/getByIDForType:car001&CAR
[2025-10-01 13:43:29.583] [debug] [Consumer] Extracted content length from message: 61
[2025-10-01 13:43:29.583] [debug] [Consumer] Extracted message type from message: GET_BY_ID_FOR_TYPE_REQUEST
[2025-10-01 13:43:29.583] [debug] [Consumer] Query.type: CAR
[2025-10-01 13:43:29.583] [debug] [Worker] Cleared pending_get_by_id_requests for 'car002': erased=0
[2025-10-01 13:43:29.583] [debug] [Worker] Cleared pending_get_by_type_requests for 'CAR': erased=0
[2025-10-01 13:43:29.583] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:43:29.583] [info] [Worker] Reading packet...
[2025-10-01 13:43:29.583] [debug] [Consumer] Query.id: car001
[2025-10-01 13:43:29.583] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar001&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:43:29.583] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=200
[2025-10-01 13:43:29.587] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:43:29.587] [info] [Consumer] Data packet payload: [
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
[2025-10-01 13:43:29.588] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car001, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:43:29.588] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:43:29.588] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:43:29.588] [debug] [Consumer] Candidate JSON slice:
[
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
[2025-10-01 13:43:29.588] [debug] [Consumer] Sanitized payload to:
[
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
[2025-10-01 13:43:29.588] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:43:29.588] [debug] [Consumer] Got ID and type in direct: id=car001,type=CAR
[2025-10-01 13:43:29.588] [info] [Consumer] Reading packet...
[2025-10-01 13:43:29.588] [info] [Worker] Received content: /?id=car001&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:43:29.588] [info] [Worker] Received payload: [
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
[2025-10-01 13:43:29.588] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:43:29.588] [debug] [Worker] Consumer intra message...
[2025-10-01 13:43:29.588] [info] [Worker] Direct mode...
[2025-10-01 13:43:29.588] [debug] [Worker] Cleared pending_get_by_id_requests for 'car001': erased=0
[2025-10-01 13:43:29.588] [debug] [Worker] Cleared pending_get_by_type_requests for 'CAR': erased=0
[2025-10-01 13:43:29.588] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:43:29.588] [info] [Worker] Reading packet...
```

Let's try a GET by Type request from Consumer .27:

```sh
scn4ndn@seeds23:~$ curl -i -X GET http://localhost:8080/?"type=CAR"
HTTP/1.1 200 OK
Keep-Alive: timeout=5, max=100
Content-Length: 328
Content-Type: text/plain

{
  "batch_size": 1,
  "messages": [
    {
      "name": "",
      "payload": {
        "items": [
          {
            "date": 1758793200000,
            "id": "car002",
            "type": "CAR"
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

Let's check the Producer's .8 logs: 

```log
[2025-10-01 13:45:36.170] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR
[2025-10-01 13:45:36.171] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR_registry, Command: getByType, Params: [CAR]
[2025-10-01 13:45:36.171] [info] [Producer] Sending registry file payload for type CAR.
[2025-10-01 13:45:36.171] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:45:36.499] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:45:36.499] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car003, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 13:45:36.499] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:45:36.499] [debug] [Producer] TemporalQuery → id: car003, date: , type: CAR, timeframe: , count: , filters: 
[2025-10-01 13:45:36.499] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car003&type=CAR&count=&date=&timeframe=&filters=
[2025-10-01 13:45:36.503] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:45:36.503] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:45:36.832] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:45:36.832] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car002, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 13:45:36.832] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:45:36.832] [debug] [Producer] TemporalQuery → id: car002, date: , type: CAR, timeframe: , count: , filters: 
[2025-10-01 13:45:36.832] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car002&type=CAR&count=&date=&timeframe=&filters=
[2025-10-01 13:45:36.835] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:45:36.835] [debug] [Producer] Sending data to the NDN…
[2025-10-01 13:45:37.168] [info] [Producer] Received interest in function: onInterest, name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D
[2025-10-01 13:45:37.168] [info] [Producer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car001, type=CAR, date=, count=, timeframe=, filters=]
[2025-10-01 13:45:37.168] [debug] [Producer] Created interest response: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in: process_temporal_query_interest
[2025-10-01 13:45:37.168] [debug] [Producer] TemporalQuery → id: car001, date: , type: CAR, timeframe: , count: , filters: 
[2025-10-01 13:45:37.168] [debug] [Producer] Requesting to endpoint: /temporal-query?id=car001&type=CAR&count=&date=&timeframe=&filters=
[2025-10-01 13:45:37.171] [info] [Producer] Successfully queried /temporal-query
[2025-10-01 13:45:37.171] [debug] [Producer] Sending data to the NDN…
```

Let's check the Consumer's .27 logs: 

```log
[2025-10-01 13:45:36.006] [info] [HTTPServer] Received GET request for / with body 
[2025-10-01 13:45:36.006] [info] [HTTPServer] Received GET request for / with target /?type=CAR
[2025-10-01 13:45:36.006] [info] [HTTPServer] Received GET request from 127.0.0.1
Message: {type=CAR}
Type: GET_BY_TYPE_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
type = CAR
[2025-10-01 13:45:36.006] [debug] [HTTPServer] Writing message from GET request to service queue...
Message: {type=CAR}
Type: GET_BY_TYPE_REQUEST
Sender: HTTP_SRV
Payload: 
HTTP Type: HTTP_GET
Params:
type = CAR

[2025-10-01 13:45:36.006] [info] [Worker] Received content: {type=CAR}
[2025-10-01 13:45:36.006] [info] [Worker] Received payload: 
[2025-10-01 13:45:36.006] [info] [Worker] Received type: GET_BY_TYPE_REQUEST
[2025-10-01 13:45:36.006] [debug] [Worker] Server intra message...
[2025-10-01 13:45:36.006] [info] [Worker] Pushing getByType request for type: CAR_registry
[2025-10-01 13:45:36.006] [info] [Worker] Reading packet...
[2025-10-01 13:45:36.006] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType:CAR
[2025-10-01 13:45:36.006] [debug] [Consumer] Extracted content length from message: 58
[2025-10-01 13:45:36.006] [debug] [Consumer] Extracted message type from message: GET_BY_TYPE_REQUEST
[2025-10-01 13:45:36.006] [debug] [Consumer] Created name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType:CAR in func: express_get_by_type_interest
[2025-10-01 13:45:36.006] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:45:36.336] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR_registry/getByType%3ACAR in function onData
[2025-10-01 13:45:36.336] [info] [Consumer] Data packet payload: Rcar003car002car001 in function onData
[2025-10-01 13:45:36.336] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR_registry, Command: getByType, Params: [CAR] in onData
[2025-10-01 13:45:36.336] [info] [Consumer] Received Registry File
[2025-10-01 13:45:36.336] [info] [Consumer] In Function: onData sending name CAR to worker Rcar003car002car001
[2025-10-01 13:45:36.336] [info] [Consumer] Reading packet...
[2025-10-01 13:45:36.336] [info] [Worker] Received content: CAR
[2025-10-01 13:45:36.336] [info] [Worker] Received payload: Rcar003car002car001
[2025-10-01 13:45:36.336] [info] [Worker] Received type: REGISTRY_PAYLOAD
[2025-10-01 13:45:36.336] [debug] [Worker] Consumer intra message...
[2025-10-01 13:45:36.336] [debug] [Worker] Number of IDs in payload: 3
[2025-10-01 13:45:36.336] [debug] [Worker] Reading ID #1 with size 6
[2025-10-01 13:45:36.336] [info] [Worker] Parsed ID #1: car003
[2025-10-01 13:45:36.336] [debug] [Worker] Reading ID #2 with size 6
[2025-10-01 13:45:36.336] [info] [Worker] Parsed ID #2: car002
[2025-10-01 13:45:36.336] [debug] [Worker] Reading ID #3 with size 6
[2025-10-01 13:45:36.336] [info] [Worker] Parsed ID #3: car001
[2025-10-01 13:45:36.336] [info] [Worker] Size of ids in registry file: 3
[2025-10-01 13:45:36.336] [debug] [Worker] Processing registry id: car003
[2025-10-01 13:45:36.337] [info] [Worker] Pushing request to the consumer for id car003
[2025-10-01 13:45:36.337] [debug] [Worker] Processing registry id: car002
[2025-10-01 13:45:36.337] [info] [Worker] Pushing request to the consumer for id car002
[2025-10-01 13:45:36.337] [debug] [Worker] Processing registry id: car001
[2025-10-01 13:45:36.337] [info] [Worker] Pushing request to the consumer for id car001
[2025-10-01 13:45:36.337] [info] [Worker] Reading packet...
[2025-10-01 13:45:36.337] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/getByIDForType:car003&CAR
[2025-10-01 13:45:36.337] [debug] [Consumer] Extracted content length from message: 61
[2025-10-01 13:45:36.337] [debug] [Consumer] Extracted message type from message: GET_BY_ID_FOR_TYPE_REQUEST
[2025-10-01 13:45:36.337] [debug] [Consumer] Query.type: CAR
[2025-10-01 13:45:36.337] [debug] [Consumer] Query.id: car003
[2025-10-01 13:45:36.337] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar003&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:45:36.337] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:45:36.668] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar003%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:45:36.668] [info] [Consumer] Data packet payload: [
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
[2025-10-01 13:45:36.668] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car003, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:45:36.668] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:45:36.669] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:45:36.669] [debug] [Consumer] Candidate JSON slice:
[
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
[2025-10-01 13:45:36.669] [debug] [Consumer] Sanitized payload to:
[
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
[2025-10-01 13:45:36.669] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:45:36.669] [debug] [Consumer] Got ID and type in direct: id=car003,type=CAR
[2025-10-01 13:45:36.669] [info] [Consumer] Reading packet...
[2025-10-01 13:45:36.669] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/getByIDForType:car002&CAR
[2025-10-01 13:45:36.669] [debug] [Consumer] Extracted content length from message: 61
[2025-10-01 13:45:36.669] [debug] [Consumer] Extracted message type from message: GET_BY_ID_FOR_TYPE_REQUEST
[2025-10-01 13:45:36.669] [debug] [Consumer] Query.type: CAR
[2025-10-01 13:45:36.669] [debug] [Consumer] Query.id: car002
[2025-10-01 13:45:36.669] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar002&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:45:36.669] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:45:36.669] [info] [Worker] Received content: /?id=car003&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:45:36.669] [info] [Worker] Received payload: [
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
[2025-10-01 13:45:36.669] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:45:36.669] [debug] [Worker] Consumer intra message...
[2025-10-01 13:45:36.669] [info] [Worker] Direct mode...
[2025-10-01 13:45:36.670] [debug] [Worker] Cleared pending_get_by_id_requests for 'car003': erased=0
[2025-10-01 13:45:36.670] [debug] [Worker] Cleared pending_get_by_type_requests for 'CAR': erased=1
[2025-10-01 13:45:36.670] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:45:36.670] [info] [Worker] Reading packet...
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
            "date": 1758793800000,
            "id": "car003",
            "type": "CAR"
          }
        ],
        "mode": "direct"
      },
      "sender": 6,
      "type": "TEMPORAL_QUERY_RESPONSE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:
[2025-10-01 13:45:36.671] [info] [HTTPServer] Read Message from queue:
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
            "date": 1758793800000,
            "id": "car003",
            "type": "CAR"
          }
        ],
        "mode": "direct"
      },
      "sender": 6,
      "type": "TEMPORAL_QUERY_RESPONSE"
    }
  ]
}

HTTP Type: HTTP_EMPTY
Params:

[2025-10-01 13:45:37.000] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar002%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:45:37.001] [info] [Consumer] Data packet payload: [
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
[2025-10-01 13:45:37.001] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car002, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:45:37.001] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:45:37.001] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:45:37.001] [debug] [Consumer] Candidate JSON slice:
[
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
[2025-10-01 13:45:37.002] [debug] [Consumer] Sanitized payload to:
[
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
[2025-10-01 13:45:37.002] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:45:37.002] [debug] [Consumer] Got ID and type in direct: id=car002,type=CAR
[2025-10-01 13:45:37.002] [info] [Worker] Received content: /?id=car002&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:45:37.004] [info] [Worker] Received payload: [
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
[2025-10-01 13:45:37.005] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:45:37.005] [debug] [Worker] Consumer intra message...
[2025-10-01 13:45:37.005] [info] [Worker] Direct mode...
[2025-10-01 13:45:37.002] [info] [Consumer] Reading packet...
[2025-10-01 13:45:37.005] [debug] [Worker] Cleared pending_get_by_id_requests for 'car002': erased=0
[2025-10-01 13:45:37.005] [debug] [Consumer] Extracted content from message: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/getByIDForType:car001&CAR
[2025-10-01 13:45:37.005] [debug] [Consumer] Extracted content length from message: 61
[2025-10-01 13:45:37.005] [debug] [Consumer] Extracted message type from message: GET_BY_ID_FOR_TYPE_REQUEST
[2025-10-01 13:45:37.005] [debug] [Consumer] Query.type: CAR
[2025-10-01 13:45:37.005] [debug] [Consumer] Query.id: car001
[2025-10-01 13:45:37.005] [debug] [Worker] Cleared pending_get_by_type_requests for 'CAR': erased=0
[2025-10-01 13:45:37.005] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:45:37.005] [info] [Consumer] Created name /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery:id%3Dcar001&type%3DCAR&date%3D&count%3D&timeframe%3D&filters%3D
[2025-10-01 13:45:37.006] [info] [Worker] Reading packet...
[2025-10-01 13:45:37.006] [info] [Consumer] Sending actual Interest /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D?CanBePrefix&MustBeFresh&Lifetime=6000
[2025-10-01 13:45:37.336] [info] [Consumer] Received Data with name: /ndn/gr/edu/mmlab1/aueb/thomasi/CAR/temporalQuery%3Aid%3Dcar001%26type%3DCAR%26date%3D%26count%3D%26timeframe%3D%26filters%3D in function onData
[2025-10-01 13:45:37.336] [info] [Consumer] Data packet payload: [
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
[2025-10-01 13:45:37.336] [info] [Consumer] Parsed interest: Prefix: ndn/gr/edu/mmlab1/aueb/thomasi, Name: CAR, Command: temporalQuery, Params: [id=car001, type=CAR, date=, count=, timeframe=, filters=] in onData
[2025-10-01 13:45:37.336] [info] [Consumer] Temporal Query Reponse...
[2025-10-01 13:45:37.336] [debug] [Consumer] Removed extra characters from payload (first 32 bytes hex): 5b 0a 20 20 7b 0a 20 20 20 20 22 22 3a 20 7b 0a 20 20 20 20 20 20 22 5f 69 64 22 3a 20 22 63 61 
[2025-10-01 13:45:37.336] [debug] [Consumer] Candidate JSON slice:
[
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
[2025-10-01 13:45:37.336] [debug] [Consumer] Sanitized payload to:
[
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
[2025-10-01 13:45:37.337] [debug] [Consumer] Direct mode (single id in bucket).
[2025-10-01 13:45:37.337] [debug] [Consumer] Got ID and type in direct: id=car001,type=CAR
[2025-10-01 13:45:37.337] [info] [Consumer] Reading packet...
[2025-10-01 13:45:37.337] [info] [Worker] Received content: /?id=car001&type=CAR&date=&metafile=false&parentQueryHash=&lastInterest=true&mode=direct
[2025-10-01 13:45:37.337] [info] [Worker] Received payload: [
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
[2025-10-01 13:45:37.337] [info] [Worker] Received type: VERSION_RESPONSE
[2025-10-01 13:45:37.337] [debug] [Worker] Consumer intra message...
[2025-10-01 13:45:37.337] [info] [Worker] Direct mode...
[2025-10-01 13:45:37.337] [debug] [Worker] Cleared pending_get_by_id_requests for 'car001': erased=0
[2025-10-01 13:45:37.337] [debug] [Worker] Cleared pending_get_by_type_requests for 'CAR': erased=0
[2025-10-01 13:45:37.337] [debug] [Worker] Direct mode: no filters discovered; returning full item
[2025-10-01 13:45:37.337] [info] [Worker] Reading packet...
```