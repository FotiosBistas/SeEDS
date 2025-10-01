# NDN Testbed

# New Testbed Node

## Generate certificates 


# Existing nodes


Our current setup has these nodes:

```
ssh scn4ndn@195.251.234.27 ( currently consumer )
ssh scn4ndn@195.251.234.7 ( currently consumer )
ssh scn4ndn@195.251.234.8 ( currently producer )
```

You need to ask for passwords by an administrator in order to use them.


## Producer


Make sure NFD is not running: 

```
nfd-stop
```

Then run NFD: 

```
nfd-start
```

Then you need to attach to the appropriate face/testbed-node: 

```
nfdc face create udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-1.mmlab.edu.gr
```

Provide certificate (if needed, download the tool from: https://github.com/yoursunny/ndn6-tools): 

```
ndn6-serve-certs --inter certs/*.cert
```

Here's a bundle that does all of the above commands in one go: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-1.mmlab.edu.gr
```



## Consumer 1 (.7)

Make sure NFD is not running: 

```
nfd-stop
```

Then run NFD: 

```
nfd-start
```

Then you need to attach to the appropriate face/testbed-node: 

```
nfdc face create udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-2.mmlab.edu.gr
```

Here's a bundle that does all of the above commands in one go: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-2.mmlab.edu.gr
```

## Consumer 2 (.27)

Make sure NFD is not running: 

```
nfd-stop
```

Then run NFD: 

```
nfd-start
```

Then you need to attach to the appropriate face/testbed-node: 

```
nfdc face create udp://titan.cs.memphis.edu && nfdc route add / udp://titan.cs.memphis.edu && nfdc route add /localhop/nfd udp://titan.cs.memphis.edu
```

Here's a bundle that does all of the above commands in one go: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://titan.cs.memphis.edu && nfdc route add / udp://titan.cs.memphis.edu && nfdc route add /localhop/nfd udp://titan.cs.memphis.edu
```


# Experiment - KPI

```
>> KPI 6 – Validation of the full prototype over the worldwide NDN testbed

Target: Report showing that we ran experiments including at least 3 nodes in 3 continents by October 9, with participation of UMemphis.
```

## Installing Mongo Locally

## Executing the SeEDS Service

### Producer Node (.8)

You will need 5 consoles to run:

Restart NFD and attach to the appropriate face/testbed-node: 

```
reset && nfd-stop ; sleep 2 && nfd-start && sleep 2 && nfdc face create udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-1.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-1.mmlab.edu.gr
```

Use the certificate the be able to publish content: 

```
ndn6-serve-certs --inter certs/*.cert
```


Run the Broker Service: 

```
./snds_broker_bin --mongo.uri mongodb://127.0.0.1:27017
```

Run the SeEDS Service daemon: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 0
```


Post an ID:``car1`` to the service: 

```
curl -v -i -X POST http://127.0.0.1:8080 -d id=car1
```

Post an ID: ``car1`` to the registry ``CAR``: 

```
curl -v -i -X POST http://127.0.0.1:8080 -d "id=car1&type=CAR"
```


### Consumer Node (.7, .27)

You will need e consoles to run:

Restart NFD and attach to the appropriate face/testbed-node: 

For consumer node .7 run: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add / udp://mmlab-aueb-2.mmlab.edu.gr && nfdc route add /localhop/nfd udp://mmlab-aueb-2.mmlab.edu.gr
```

For consumer node .27 run: 

```
reset && nfd-stop ; sleep 2; nfd-start && sleep 2 && nfdc face create udp://titan.cs.memphis.edu && nfdc route add / udp://titan.cs.memphis.edu && nfdc route add /localhop/nfd udp://titan.cs.memphis.edu
```


For consumer node .7 you can run: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 1 --producer.registry-type MOTO
```

For consumer node .27 you can run: 

```
./snds_daemon_bin --snds-prefix ndn/gr/edu/mmlab1/aueb/thomasi --num-services 3 --snds-id 2 --producer.registry-type SENSOR
```

The point here is that you want to differentite the registry type of each node, so you can post to the appropriate node/registry each time. Also beware that you must set the ID and the number of services appropriately starting from a 0 index.


Now, after posting to the producer node, you can run: 


```
curl -i -X GET http://localhost:8080/?id=car1
```

```
curl -i -X GET http://localhost:8080/?type=CAR
```

You should see these, causing actions inside the producer node.
