================
Compilation
================

snds_service.cpp depends on ndn-cxx (and NFD). It must be placed in the "ndn-cxx/examples" directory and compiled via waf (see ndn-cxx instructions).

Command: $ reset && waf && sudo ./waf && sudo ldconfig


================
Execution Steps (for local deployment - ***all entities in the same node/localhost***):
================

1. Start nfd using the command

$ nfd-start

2. Start snds service (at DIR ndn-cxx/build/examples

$ ./snds_service

3. Run the publisher indicating both id and type, e.g., send a POST HTTP message with body "id=car1&type=CAR". Can be done via browser (in mozilla: dev tools->network->edit and resend (packet))  or curl

$ curl -v -i -X POST http://127.0.0.1:8080 -d id="car1&type=CAR"

or loop for multiple ids, e.g., 10 ids car1 to car10
$ for i in {1..10}; do curl -v -i -X POST http://127.0.0.1:8080 -d id="car${i}&type=CAR"; sleep 0.5; done

4. Run the subscriber indicating either id or typee.g., send a GET HTTP message with URL param "?id=car1" or "?type=CAR". Can be done via browser or curl

$ curl -i -X GET http://localhost:8080/?id=car1

For subscriptions append the "isSub=true&suburl=MYURL" at the url. Add quotes to all variables e.g., /?"id=car1&isSub=true&suburl=MYURL"


==================
Bugs
==================

SeqFAULT: Request byType without prior publication leads to seq-fault when making the registry file.
Unhandled packet: Acks for subscriptions are not handled.
