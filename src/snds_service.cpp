/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2022 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/validator-config.hpp>

#include <iostream>
#include <string>
#include <pthread.h>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <exception>

//#include "snds_util.hpp"

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

// Enclosing code in ndn simplifies coding (can also use `using namespace ndn`)
namespace ndn {
// Additional nested namespaces should be used to prevent/limit name conflicts
namespace examples {

/**
 * Message Format for thread communication
 * */	
enum SNDS_COMPONENTS {PROXY, PRODUCER, CONSUMER, DT, SERVICE, HTTP_SRV, ON_INTEREST, TEST};	//TODO: remove TEST
enum SNDS_INTRAMSG_TYPE {GENERIC, BYID_RESPONSE, BYTYPE_RESPONSE};	

struct SNDS_Intra_Msg{
	SNDS_COMPONENTS sender_component; // who sent the message
	char* message; // message
	unsigned message_len;
	char* name; //NDN name of the correlated item; optional! 
	unsigned name_len;
	SNDS_INTRAMSG_TYPE type; //

	
	SNDS_Intra_Msg(const SNDS_COMPONENTS &comp, const char* msg, const unsigned &msg_len){
		sender_component = comp;
		message_len = msg_len;
		message = new char [message_len];
		memcpy(message, msg, message_len);
		name = NULL;
		name_len = 0;
		type = GENERIC;
		}
	
	SNDS_Intra_Msg(SNDS_Intra_Msg* msg){
		sender_component = msg->sender_component;
		message_len = msg->message_len;
		message = new char [message_len];
		//std::cout<<"message_len: "<<message_len<<std::endl;
		memcpy(message, msg->message, msg->message_len);
		
		name=NULL; name_len=0;
		if (msg->name != NULL){
			memcpy(name, msg->name, msg->name_len);
			name_len = msg->name_len;
			}
		type=msg->type;
		}
		
	~SNDS_Intra_Msg(){
		free(message);
		if (name != NULL)
			free (name);
		}
	
	void setName(const char * _name, const unsigned& _name_len){
		if (_name != NULL){
			name = new char [_name_len];
			memcpy(name, _name, _name_len);
			name_len = _name_len;
		}else{ 
			std::cout<<"ERROR: Got null pointer for name. Exiting.\n";
			exit(1);
		}
	}
	
	void print(){
		std::cout<< "SNDS component: "<< sender_component<<" message: "<< message;
		if (name != NULL){
			std::cout<< "name: "<<name;
		}
		std::cout<<std::endl;}
	};

class SNDS_SERVICE {
public:
  SNDS_SERVICE() {
    producer_validator.load("trust-schema.conf");
    consumer_validator.load("trust-schema.conf");
    
  }

	void init(){
			std::vector<std::string> *tmpvec = new std::vector<std::string>();
			myTypesRegistryMap["/CAR"] = tmpvec;
		}

  void start_threads() {
	  /***
	   * Initialize
	   * */
	  //First create the threads for the internal snds_srv components
	  pthread_t proxy_thread, dt_thread, consumer_thread, producer_thread, service_thread, httpserv_thread;
	  pthread_create (&proxy_thread, NULL, static_proxy, this); 
	  pthread_create (&dt_thread, NULL, static_digital_twin, this); 
	  pthread_create (&consumer_thread, NULL, static_consumer, this); 
	  pthread_create (&producer_thread, NULL, static_producer, this); 
	  pthread_create (&service_thread, NULL, static_service, this); 
	  pthread_create (&httpserv_thread, NULL, static_httpserv, this); 
		//sleep(5);
		//log(__FUNCTION__, "writting packet");
		//SNDS_Intra_Msg* msg = new SNDS_Intra_Msg(TEST, "test_message", 12);
		//writeIntraSNDSMessage(v_to_proxy, m_to_proxy, c_to_proxy, msg);
		//log(__FUNCTION__, "wrote packet");


	  (void) pthread_join(dt_thread, NULL);
	  (void) pthread_join(proxy_thread, NULL);
	  (void) pthread_join(consumer_thread, NULL);
	  (void) pthread_join(producer_thread, NULL);
	  (void) pthread_join(service_thread, NULL);
	  (void) pthread_join(httpserv_thread, NULL);
  }

private:

	uint8_t NUMBER_OF_SNDS_SRVS = 1;
	
 	const std::string SNDS_PREFIX="/SNDS";
	const std::string MY_SNDS_ID="0"; //unique SDNS_service ID; incremental int counter starting from 0.	
	const std::string MY_SNDS_ID_NAME=SNDS_PREFIX+"_"+MY_SNDS_ID;
	const std::string ID_SCOPE_PREFIX = "/SNDS_ID_scope_";
	const std::string MY_SNDS_ID_SCOPE_NAME = SNDS_PREFIX+ID_SCOPE_PREFIX+MY_SNDS_ID;
	
	const std::string REGISTRY_FILE_POSTFIX = "_registry";

	std::map<std::string, std::vector<std::string>* > myTypesRegistryMap; //maps Type names to vectors of IDs 
	std::map<std::string, std::vector<std::string>* > pendingIDsToTypesMap; //maps IDs expected for getByType requests; use vector cause one ID may be in multiple getByType requests
	std::map<std::string, std::vector<std::string>* > pendingIDsForTypeMap; //maps type to the expected IDs
	
	
	std::map<std::string, std::vector<std::string>* > subscribedBYID_users; // <id, vector<urls>> maps subscribedIds to User URLs
	std::map<std::string, std::vector<std::string>* > subscribedBYTYPE_users; // <type, vector<urls>> maps subscribed types to User URLs
	
	std::map<std::string, std::vector<std::string>* > subscribedBYID_SNDSnodes; // <id, vector<urls>> maps subscribedIds to User URLs
	std::map<std::string, std::vector<std::string>* > subscribedBYTYPE_SNDSnodes; // <type, vector<urls>> maps subscribed types to User URLs
	
	std::vector<std::string> announcedIDsVec; //stores local annnounced ids
	std::vector<std::string> pendingGetByIDreqsVec; //stores the IDs of pending getByID requests
	std::vector<std::string> pendingGetByTYPEreqsVec; //stores the IDs of pending getByTYPE requests
	std::map<std::string, std::string> locallyAnnouncedIDsMap; //stores the IDs of locally advertised items and theit data
	//tags used in HTTP POST/GET to indicate id and type
	std::string PARAM_ID_TAG = "id" ;
	std::string PARAM_TYPE_TAG = "type" ;
	
	std::string ACK_TAG = "THIS_IS_AN_SNDS_ACK";

   //vector like message queues
   std::vector<SNDS_Intra_Msg*>  v_to_proxy;
   std::vector<SNDS_Intra_Msg*>  v_to_dt;
   std::vector<SNDS_Intra_Msg*>  v_to_producer;
   std::vector<SNDS_Intra_Msg*>  v_to_consumer;
   std::vector<SNDS_Intra_Msg*>  v_to_service;

   //mutexes for thread communication/sychronization
   pthread_mutex_t m_to_proxy = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_t m_to_dt = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_t m_to_producer = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_t m_to_consumer = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_t m_to_service = PTHREAD_MUTEX_INITIALIZER;
   
   //condition variables
   pthread_cond_t c_to_proxy = PTHREAD_COND_INITIALIZER;
   pthread_cond_t c_to_dt = PTHREAD_COND_INITIALIZER;
   pthread_cond_t c_to_producer = PTHREAD_COND_INITIALIZER;
   pthread_cond_t c_to_consumer = PTHREAD_COND_INITIALIZER;
   pthread_cond_t c_to_service = PTHREAD_COND_INITIALIZER;
   
   /***
    * work around to call no static function from the pthread_Create from member function/code
    * */
   static void* static_proxy(void* context){ return ((SNDS_SERVICE*)context)->func_proxy(NULL);}
   static void* static_digital_twin(void* context){ return ((SNDS_SERVICE*)context)->func_digital_twin(NULL);}
   static void* static_consumer(void* context){ return ((SNDS_SERVICE*)context)->func_consumer(NULL);}
   static void* static_producer(void* context){ return ((SNDS_SERVICE*)context)->func_producer(NULL);}
   static void* static_service(void* context){ return ((SNDS_SERVICE*)context)->func_service(NULL);}
   static void* static_httpserv(void* context){ return ((SNDS_SERVICE*)context)->func_httpserv(NULL);}
	   
	/**
	 * Reads intra-snds message from the specified queue (thread safe)
	 * */
    SNDS_Intra_Msg* readIntraSNDSMessage(std::vector<SNDS_Intra_Msg*>  &v, pthread_mutex_t &m,  pthread_cond_t &c){
	   pthread_mutex_lock (&m);
	   int rc=0;
	   while (v.empty() && rc==0){
		 	rc = pthread_cond_wait (&c, &m);
	   }
	   SNDS_Intra_Msg* msg = v.front();
	   v.erase(v.begin());
	   pthread_mutex_unlock (&m);
	   return msg;
	   }  
	   	   
	/**
	 * Writes intra-snds message to the specified queue (thread safe)
	 * */	   
    void writeIntraSNDSMessage(std::vector<SNDS_Intra_Msg*> &v, pthread_mutex_t &m,  pthread_cond_t &c,  SNDS_Intra_Msg* msg){
	  	pthread_mutex_lock (&m);
		v.push_back(msg);  
	    //std::cout<<v.back()<<std::endl;;
		pthread_cond_signal(&c);
		pthread_mutex_unlock (&m);
		//log(__FUNCTION__, "Wrote packet and notified receiver.");
		return;
	   }   
	   
	  /**
	   * TODO: Turns out I do not include proxy in operation.. can be removed..
	   * */
   void* func_proxy(void* params){
	    log (__FUNCTION__, "Started.");

	  while (true){
		  log (__FUNCTION__, "Reading packet..");
		  SNDS_Intra_Msg* msg = readIntraSNDSMessage(v_to_proxy, m_to_proxy, c_to_proxy);
  		  log (__FUNCTION__, "Got packet..");
  		  SNDS_COMPONENTS src = msg->sender_component;
  		  
  		  switch (src){
			  case TEST:
			    log(__FUNCTION__, "Forwarding packet to the consumer.");
			    msg->sender_component=PROXY;
				writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg);
				break;
			  case HTTP_SRV:
			    //std::string outmsg="Received packet from http srv. Payload: " + (msg->message);
			    log(__FUNCTION__, msg->message );
			    log(__FUNCTION__, "Forwarding packet to the consumer.");
				msg->sender_component=PROXY;
				writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg);
				break;
			  default:
				log(__FUNCTION__, "Unknown sender, dropping packet.");
				delete msg;
			  }
		}
	  return 0;
	  }
	   
   void* func_digital_twin(void* params){
	   log (__FUNCTION__, "Started.");

       while (true){
		  log (__FUNCTION__, "Reading packet..");
		  SNDS_Intra_Msg* msg = readIntraSNDSMessage(v_to_dt, m_to_dt, c_to_dt);
  		  log (__FUNCTION__, "Got new packet. Payload: "+std::string(msg->message));
		  log (__FUNCTION__, "Creating the dt state..");
	      ///TODO: do what the dt has to to do
  		  //parse id and Type from url
	      std::string id = getfromGETRequest(std::string(msg->message), "id");
		  /***
		   * 1. Announce id
		   * */
  		  if (strcmp(id.c_str(), "")==0){
			  log (__FUNCTION__, "WARNING:Unknown ID: \""+ id+"\"");
		  }else{
			  std::string itemName = "/"+id;
  		      // update local state for announced content
			  log(__FUNCTION__, "Added new id (\""+ itemName+"\") to announced IDs vector.");
			  announcedIDsVec.push_back(itemName);
			  // forward request to the producer threads			
			  SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(DT, itemName.c_str(), itemName.length());
			  log (__FUNCTION__, "Pushing byID advertisement to the producer..");
			  writeIntraSNDSMessage(v_to_producer, m_to_producer, c_to_producer, msg1);
		  }
		  /**
		   * 2. Send interest to register id for that type
		   * */
  		  std::string type = getfromGETRequest(std::string(msg->message), "type");
  		  if (strcmp(type.c_str(), "")==0){
			  log (__FUNCTION__, "WARNING:Unknown TYPE: \""+ type+"\"");
  		  }else{
			  log (__FUNCTION__, "Pushing byTYPE (\""+type+"\") advertisement to the consumer..");
			  std::string itemName = "/"+type+"/"+id;
			  SNDS_Intra_Msg* msg2 = new SNDS_Intra_Msg(DT, itemName.c_str(), itemName.length());
			  writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg2);
		  }
		  /**
		   * 3. Send interest to subscribe id under the correlated id scope
		   * */
  		  if (strcmp(id.c_str(), "")==0){
			  log (__FUNCTION__, "WARNING:Unknown ID: \""+ id+"\"");
		  }else{
			  log (__FUNCTION__, "Pushing byTYPE advertisement to the consumer..");
			  std::string idscope = getSNDSNodeScopeNameForSubscription(id);
			  std::string itemName = idscope+"/"+id;
			  log (__FUNCTION__, itemName.c_str());
			  SNDS_Intra_Msg* msg3 = new SNDS_Intra_Msg(DT, itemName.c_str(), itemName.length());
			  writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg3);
		  }
	  }
      
      return 0;
      }
	   
	   /**
	    * "Stateless" (in the context of SNDS app) function that creates and sends Interest packets
	    * */
   void* func_consumer(void* params){
  		  log (__FUNCTION__, "Started.");

	   while (true){
			log (__FUNCTION__, "Reading packet..");
			SNDS_Intra_Msg* msg = readIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer);
					  
			std::string intNameStr = SNDS_PREFIX;
			intNameStr.append(msg->message, msg->message_len);
			Name interestName(intNameStr);
			std::cout<<intNameStr<<std::endl;
			interestName.appendVersion();

			Interest interest(interestName);
			interest.setMustBeFresh(true);
			interest.setInterestLifetime(6_s); // The default is 4 seconds

			log(__FUNCTION__, "Sending Interest: "+std::string(msg->message, msg->message_len));
			std::cout<<interest<<std::endl;
			//log(__FUNCTION__, interest.c_str());
			consumer_face.expressInterest(interest,
								   std::bind(&SNDS_SERVICE::onData, this,  _1, _2),
								   std::bind(&SNDS_SERVICE::onNack, this, _1, _2),
								   std::bind(&SNDS_SERVICE::onTimeout, this, _1));

			// processEvents will block until the requested data is received or a timeout occurs
			consumer_face.processEvents();
			delete msg;
		  
		}
      
      return 0;
	   }
	   
	   
	/**
	 * Producer thread makes the initial advertisements for the SNDS service
	 * */
   void* func_producer(void* params){
 		 log (__FUNCTION__, "Started.");
 		  
	     std::string tmp_name = MY_SNDS_ID_NAME;
		 log (__FUNCTION__, "Announcing name: "+tmp_name);
	     producer_face.setInterestFilter(tmp_name,
                             std::bind(&SNDS_SERVICE::onInterest, this, _2),
                             nullptr, // RegisterPrefixSuccessCallback is optional
                             std::bind(&SNDS_SERVICE::onRegisterFailed, this, _1, _2));

		tmp_name = MY_SNDS_ID_SCOPE_NAME;
		log (__FUNCTION__, "Announcing name: "+tmp_name);
		producer_face.setInterestFilter(tmp_name,
								 std::bind(&SNDS_SERVICE::onInterest, this, _2),
								 nullptr, // RegisterPrefixSuccessCallback is optional
								 std::bind(&SNDS_SERVICE::onRegisterFailed, this, _1, _2));
		//ADVERTISE items for byTYPE requests						 
	  for (const auto &myPair : myTypesRegistryMap){
		  tmp_name = SNDS_PREFIX+myPair.first;
		   log (__FUNCTION__, "Announcing name: "+tmp_name);
		  producer_face.setInterestFilter(tmp_name,
								 std::bind(&SNDS_SERVICE::onInterest, this, _2),
								 nullptr, // RegisterPrefixSuccessCallback is optional
								 std::bind(&SNDS_SERVICE::onRegisterFailed, this, _1, _2));
								 
		 //registry file
		   tmp_name = tmp_name+REGISTRY_FILE_POSTFIX;
		   log (__FUNCTION__, "Announcing name: "+tmp_name);
		   producer_face.setInterestFilter(tmp_name,
								 std::bind(&SNDS_SERVICE::onInterest, this, _2),
								 nullptr, // RegisterPrefixSuccessCallback is optional
								 std::bind(&SNDS_SERVICE::onRegisterFailed, this, _1, _2));
		  }

		auto cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
		m_certServeHandle = producer_face.setInterestFilter(security::extractIdentityFromCertName(cert.getName()),
													 [this, cert] (auto&&...) {producer_face.put(cert); },
													 std::bind(&SNDS_SERVICE::onRegisterFailed, this, _1, _2));
		
	   	producer_face.processEvents(time::milliseconds(-1));
	   		

	    while (true){
			producer_face.processEvents(time::milliseconds(1000)); //every 1s check for incoming reqs from proxy
			if (v_to_producer.size()>0){
				SNDS_Intra_Msg* msg = readIntraSNDSMessage(v_to_producer, m_to_producer, c_to_producer);
				log (__FUNCTION__, "Got new adv request. Message: "+std::string(msg->message, msg->message_len));

				tmp_name = SNDS_PREFIX;
				tmp_name.append(msg->message, msg->message_len);
				log (__FUNCTION__, "Announcing name: "+tmp_name);
				locallyAnnouncedIDsMap[std::string(msg->message, msg->message_len)] = "SNDS_dummy_data";

				producer_face.setInterestFilter(tmp_name,
								 std::bind(&SNDS_SERVICE::onInterest, this, _2),
								 nullptr, // RegisterPrefixSuccessCallback is optional
								 std::bind(&SNDS_SERVICE::onRegisterFailed, this, _1, _2));
				delete msg;
			}
				
			//producer_face.processEvents();
		  //log (__FUNCTION__, "Reading packet..");
		  //SNDS_Intra_Msg* msg = readIntraSNDSMessage(v_to_producer, m_to_producer, c_to_producer);
		}
      
      return 0;
	   }
	   
	   /**
	    * Manages IP end-user getByTYPE and getByID requests 
	    * */
   void* func_service(void* params){
	    log (__FUNCTION__, "Started.");

	    while (true){
		  log (__FUNCTION__, "Reading packet..");
		  SNDS_Intra_Msg* msg = readIntraSNDSMessage(v_to_service, m_to_service, c_to_service);
		  /**
		   * handle message from CONSUMER -- Data messages that are responses to Interest messages
		   * */
		  if (msg->sender_component == CONSUMER){
			  std::string name (msg->name, msg->name_len);
 			  prepareName(name);

			  log(__FUNCTION__, "Got Data packet for CONSUMER with name: " +name);
			  bool handled = false;
			  ///check if it is a registry file
			  if (name.find(REGISTRY_FILE_POSTFIX)!=std::string::npos){
				  handled = true;
				  log(__FUNCTION__, "Got Registry file.");	 
				  std::vector<std::string> _v;
				  getIDsFromRegistryFilePayload(msg->message, _v);
				  log (__FUNCTION__, "Size of ids in registry file: "+_v.size());
				  //for each id in the registry file
				  for (unsigned i=0; i< _v.size(); i++){
					  prepareName(_v[i]);
					  if (pendingIDsToTypesMap.find(_v[i]) == pendingIDsToTypesMap.end()){
						  std::vector<std::string> *tmpv = new std::vector<std::string>();;
						  tmpv->push_back(name);
						  pendingIDsToTypesMap[_v[i]] = tmpv;
						  }else{
							  pendingIDsToTypesMap[_v[i]]->push_back(name);
						  }
						  //send request for ID
						  SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(SERVICE, _v[i].c_str(), _v[i].length());
						  log (__FUNCTION__, "Pushing request to the consumer.");
						  writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg1);
					  }
				  //add ids to pendingIDsToTypesMap
				  std::vector<std::string> *v_ids = new std::vector<std::string>(); 
				  v_ids->assign(_v.begin(), _v.end());
				  pendingIDsForTypeMap[name] = v_ids;
				  }
			   ///this either a response to getById or getByTYPE or BOTH!
			   	auto iter = std::find(pendingGetByIDreqsVec.begin(), pendingGetByIDreqsVec.end(), name);
				if (iter!=pendingGetByIDreqsVec.end()){
					handled = true;
					log(__FUNCTION__, "Got successfully data for previous requestByID. Removing state");
					pendingGetByIDreqsVec.erase(iter);
				}
				auto iter2  = pendingIDsToTypesMap.find(name); // name is id, e.g., car1
				if (iter2 != pendingIDsToTypesMap.end()){
					handled = true;
					log(__FUNCTION__, "Got successfully data for previous requestByTYPE. Removing state.");
					std::vector<std::string> *vtypes = iter2->second;
					//for every TYPE that this id is included
					for (unsigned k=0;k<vtypes->size(); k++){
						std::string _type = vtypes->at(k);
						//verify state consistency (i.e., that id is stored in the vector of this type)
						std::vector<std::string>* _vids = pendingIDsForTypeMap.find(_type)->second;
						auto it = std::find(_vids->begin(), _vids->end(), name);
						if (it == _vids->end()){
							log(__FUNCTION__, "Could not find id "+name+" for type "+_type);
							log(__FUNCTION__, "FATAL::State inconsistency. Exiting.");
							exit(1);
						}
						_vids->erase(it); // erase id for this type
						//check if this was the last id for this type
						if (_vids->empty()){
							delete _vids;
							pendingIDsForTypeMap.erase(_type); //erase type since all ids have been received
							vtypes->erase(vtypes->begin() + k); // erase type for this id too.
							k=0; //reset counter to avoid weird loop behavior
						}
					}
					//if no types are pending remove entry completely
					if (vtypes->empty()){
						pendingIDsToTypesMap.erase(iter2);
						}
				}
				if (!handled) {
					log(__FUNCTION__, "WARNING: packet was not handled. Debug it.");
					}
 			   delete msg;
		  }  
		  /**
		   *  hanlde message from PROXY -- request from IP end-user
		   * */
		  else if (msg->sender_component == HTTP_SRV){
			  //first check if its a subscription (or regular request)
			  bool isSub = isSubscriptionRequest(msg->message);
			  std::string subUrl;
			  if (isSub){
				log(__FUNCTION__, "Got a subscription type getByID request.");
				subUrl = getURLfromSubscription(msg->message);
				if (subUrl.compare("")==0){
					log(__FUNCTION__, "Error:: Got empty/no URL for response. Treating request as regular (no subscription).");
					isSub = false;
				}
			  }
			  		
			  std::string _id = getfromGETRequest (msg->message, PARAM_ID_TAG);
			  std::string _type = getfromGETRequest (msg->message, PARAM_TYPE_TAG);
			  
			  if (isSub == true){
				// If this is an ByID request  
				if (strcmp(_id.c_str(), "")!=0){
					//add user subscription state, ignore message if state already exists	
					bool subExists = false;
					auto itmap = subscribedBYID_users.find(_id);
					if (itmap == subscribedBYID_users.end()){
						//adding first sub for this id
						std::vector<std::string>* newVec = new std::vector<std::string>();
						newVec->push_back(subUrl);
						subscribedBYID_users[_id] = newVec;
					}else{
						//check if url is already in the vector
						std::vector<std::string>* urlVec = itmap->second;
						auto itvec = std::find(urlVec->begin(), urlVec->end(), _id);
						if (itvec == urlVec->end()){
							//URL is not in the vec
							urlVec->push_back(subUrl);
							log(__FUNCTION__, "Stored new getByID subscription.");
						}else {
							//do notthing						
							log(__FUNCTION__, "Subscription was previously stored. Ignoring (duplicate) request.");
							}
						}
					if (!subExists)	{
						log(__FUNCTION__, "Stored new getByID subscription.");
						//send interest to responsible (for this ID) SNDS node
						std::string prepared_id = _id; //TODO: make sure all names are "prepared" at all times..
						prepareName(prepared_id); 
						std::string int_name = getSNDSNodeScopeNameForSubscription(prepared_id)+prepared_id+ID_SCOPE_PREFIX+MY_SNDS_ID;
						SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(SERVICE, int_name.c_str(), int_name.length());
						log (__FUNCTION__, "Pushing byID  (ID: \""+_id+"\") request to SNDS node.");
						writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg1);
					}
				}
			  } 
				  //check if this a requestByID case
				  if (strcmp(_id.c_str(), "")!=0){
					  std::string itemName = std::string("/"+_id);
					  // update local state for received getByID requests
					  log(__FUNCTION__, "Got getByID request (ID: \""+_id+"\")");
					  // avoid duplicate reqeusts
					  if(std::find(pendingGetByIDreqsVec.begin(), pendingGetByIDreqsVec.end(), _id)!=pendingGetByIDreqsVec.end()){
						log(__FUNCTION__, "Found pending request for ID. Skipping request..");				  
					  }else{
						  pendingGetByIDreqsVec.push_back("/"+_id);
						  // forward request to the consumer threads			
						  SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(SERVICE, itemName.c_str(), itemName.length());
						  log (__FUNCTION__, "Pushing byID  (ID: \""+_id+"\") request to the consumer.");
						  writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg1);
						}
				  }else{
					  //check if this a requestByType case
					  if (strcmp(_type.c_str(), "")==0){
						  log (__FUNCTION__, "Could not find type..");
					  }else{
						  log (__FUNCTION__, "Got getByType request.");
						  //check if there is already a pending request for this type
						  auto it = std::find(pendingGetByTYPEreqsVec.begin(), pendingGetByTYPEreqsVec.end(),  _type);
						  if (it != pendingGetByTYPEreqsVec.end()){
							  log (__FUNCTION__, "Request for this type is pending. Aggregating requests (ignoring current request)."); //TODO: perhaps add a timer, in case process stalls..
						  } else {
							  pendingGetByTYPEreqsVec.push_back(_type);
							  std::string itemName = std::string("/"+_type+REGISTRY_FILE_POSTFIX);
							  // forward request to the consumer thread			
							  SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(SERVICE, itemName.c_str(), itemName.length());
							  log (__FUNCTION__, "Pushing request for registry file to the consumer..");
							  writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg1);
						  }
					  }
				  }
			  
			  delete msg;	
			  continue;
			}	  
		}
      return 0;   
	   }
	   
	   
   void* func_httpserv(void* params){
  		log (__FUNCTION__, "Started.");
		while (true) {
			///TODO: socket has to be reopened from scratch each packet, otherwise I can only receive post or get packets.. Need to investigate it..
			boost::asio::io_context io_context;
			tcp::acceptor acceptor(io_context, {tcp::v4(), 8080});
			tcp::socket socket(io_context);
			acceptor.accept(socket);

			// Read the HTTP request
			boost::beast::flat_buffer buffer;
			http::request<http::string_body> request;
	
				try{
					boost::beast::http::read(socket, buffer, request);
				}catch (std::exception& e){
					std::cout<<"Caught exception at read(socket) "<<e.what()<<std::endl;
					socket.shutdown(tcp::socket::shutdown_send);	
					continue;
				}
		
			// Handle the request
			//// Prepare the response message
			http::response<http::string_body> response;
			response.version(request.version());
			response.result(http::status::ok);
			response.set(http::field::server, "My HTTP Server");
			response.set(http::field::content_type, "text/plain");
			response.body() = "SNDS_ACK!\n";
			response.prepare_payload();

			//// Send the response to the client
			boost::beast::http::write(socket, response);
			//forward request internally
			
			std::string method_name = std::string(std::string_view(request.method_string().data(),request.method_string().size()));
			
			if (strcmp(method_name.c_str(), "GET")==0){
				log(__FUNCTION__,"Got HTTP GET request from: "+socket.remote_endpoint().address().to_string());
				//forward GET to DT
				std::string target_str = boost_string_view_to_string(request.target()); 
				SNDS_Intra_Msg *msg = new SNDS_Intra_Msg(HTTP_SRV, target_str.c_str(), (unsigned) target_str.length());
				writeIntraSNDSMessage(v_to_service, m_to_service, c_to_service, msg);
			}else if (strcmp(method_name.c_str(), "POST")==0){
				log(__FUNCTION__,"Got HTTP POST request from: "+socket.remote_endpoint().address().to_string());
				log(__FUNCTION__,"Request body: \""+request.body()+"\"");
				//forward POST to SERVICE	
				SNDS_Intra_Msg *msg = new SNDS_Intra_Msg(HTTP_SRV, request.body().c_str(), (unsigned) request.body().length());
				writeIntraSNDSMessage(v_to_dt, m_to_dt, c_to_dt, msg);
			}else {
				log(__FUNCTION__, "Unkown method type. Ignoring request");
				}					
				 socket.shutdown(tcp::socket::shutdown_send);	
		}
        // Close the socket
       
        return 0;   
	   }
   
  Face producer_face, consumer_face;
  KeyChain m_keyChain;
  ValidatorConfig producer_validator{producer_face};
  ValidatorConfig consumer_validator{consumer_face};
  ScopedRegisteredPrefixHandle m_certServeHandle;
    
    
    /**
     * Callback for incoming Interests for the producer thread. 
     * */
   void onInterest(const Interest& interest){
		//std::cout << ">> I: " << interest << std::endl;
		//first check if the Interest prefix is an advertised scope for getByTYPE requests; etc. /CAR/car1
		//if so, add the post fix (eg., car1) to the registry for that type (eg., CAR_registry)
		Name name = interest.getName();
		log(__FUNCTION__, name.toUri().c_str());
		if (name.size() > 2) {
			/**
			 * Handle interest indicating new adv for type -- prefix will be a predefined TYPE
			 * */		
			std::string typenamestr = name.at(1).toUri();
			//log(__FUNCTION__, typenamestr.c_str());
			auto iter = myTypesRegistryMap.find("/"+typenamestr);
			if (iter!=myTypesRegistryMap.end()){
				std::string id_for_type = name.at(2).toUri();

				log(__FUNCTION__, "Got interest indicating new adv for type (\""+typenamestr+"\"). Adding user to registry (ID: \""+id_for_type+"\").");
				//std::cout<<typenamestr<<std::endl;
				std::vector<std::string> *tmp_v = iter->second;
				//if type is already registered
				if (std::find(tmp_v->begin(), tmp_v->end(), id_for_type)!= tmp_v->end()){
					log(__FUNCTION__, "WARNING:;ID already registered for type. Skipping..");
					sendData(interest.getName(), "WARNING:;ID already registered for type.");
					return;
					}
				tmp_v->push_back(id_for_type);
				//for (uint8_t i=0; i<tmp_v->size(); i++){
					//log(__FUNCTION__, tmp_v->at(i).c_str());
					//}
				log(__FUNCTION__, "Sending back ACK");
				sendData(interest.getName(), ACK_TAG);
				return;
			}
			/**
			 * Handle interest indicating a new ID advertised for async pub/sub -- prefix will be a predefined SNDS_ID_scope_X
			 * */		
			std::string prefix = "/"+name.at(1).toUri();
			if (MY_SNDS_ID_SCOPE_NAME.compare(SNDS_PREFIX+prefix)==0){
				//there are two cases:
				// //case1:
				if (name.size() == 5){
					log(__FUNCTION__, "Got interest indicating new SUBSCRIPTION getByid for \""+name.at(2).toUri()+"\" from SNDS_NODE \""+name.at(3).toUri()+"\"");
					log(__FUNCTION__, "Storing subscription and sending back ACK..");
					std::string prepared_name = name.at(2).toUri();
					prepareName(prepared_name);
					std::string prepared_sndsId = name.at(3).toUri();
					prepareName(prepared_sndsId);
					auto itmap = subscribedBYID_SNDSnodes.find(prepared_name);
					if (itmap == subscribedBYID_SNDSnodes.end()){
						//add new entry
						std::vector<std::string> * sdndsidvec = new std::vector<std::string>();
						subscribedBYID_SNDSnodes[prepared_name] = sdndsidvec;
						sdndsidvec->push_back(prepared_sndsId);
					}else{
						//subscriptions for that name already exist
						std::vector<std::string> * sdndsidvec = itmap->second;
						auto itvec = std::find(sdndsidvec->begin(), sdndsidvec->end(),  prepared_sndsId);
						if (itvec == sdndsidvec->end()){
							sdndsidvec->push_back(prepared_sndsId);
						}
					}
					sendData(interest.getName(), ACK_TAG+" to_new_subscription");

				}else if (name.size() == 4){
					log(__FUNCTION__, "Got interest/notification indicating new adv for \""+name.at(2).toUri()+"\" ");
					log(__FUNCTION__, "Checking for stored subscribers to this name/id..");
					std::string prepared_name = name.at(2).toUri();
					prepareName(prepared_name);
					auto itmap = subscribedBYID_SNDSnodes.find(prepared_name);
					if (itmap != subscribedBYID_SNDSnodes.end()){
						std::vector<std::string> * sdndsidvec = itmap->second;
						for (unsigned t=0; t<sdndsidvec->size(); t++){
							log(__FUNCTION__, "Need to notify SNDS node: "+ sdndsidvec->at(t));
							std::string itemName = sdndsidvec->at(t)+prepared_name;
							SNDS_Intra_Msg* msg3 = new SNDS_Intra_Msg(ON_INTEREST, itemName.c_str(), itemName.length());
							writeIntraSNDSMessage(v_to_consumer, m_to_consumer, c_to_consumer, msg3);
						}
					}else{
						log(__FUNCTION__, "No SNDS node to notify for \""+ prepared_name	+"\"" );
						}
					//send ack -- not necessary
					sendData(interest.getName(), ACK_TAG+" to_notification_for_new_subscription");
					}
					return;
			}
			/**
			 * Handle Interest for registry file
			 * */		
			if (name.at(1).toUri().find(REGISTRY_FILE_POSTFIX)!=std::string::npos){
				std::string _nm = name.at(1).toUri();
				log(__FUNCTION__, "Got an Interest for a registry file (\""+_nm+"\").");
				_nm = _nm.substr(0, _nm.find(REGISTRY_FILE_POSTFIX));
				auto _iter = myTypesRegistryMap.find("/"+_nm);
				if (_iter == myTypesRegistryMap.end()){
					log(__FUNCTION__, "WARNING: Unknown resigstry failed.");
					return;
					}
				std::string payload = getRegistryFilePayload(_iter->second);
				log(__FUNCTION__, "Send Data with registry file.");
				sendData(interest.getName(), payload);
				return;
			}
			/**
			 * Handle Interest indicating a new getById request, e.g., /car1
			 * */		
			std::string _id = "/"+name.at(1).toUri();
			auto iter2 = locallyAnnouncedIDsMap.find(_id);
			if (iter2!=locallyAnnouncedIDsMap.end()){
				log(__FUNCTION__, "Got Interest for locally announced ID (\""+_id+"\"). Responding with Data packet.");
				sendData(interest.getName(), iter2->second);
				return;
			}else{
				log(__FUNCTION__, "WARNING::Got interest for ID (\""+_id+"\") NOT locally announced. Locally (byID) announced items:");
				std::string outmsg="";
				for (auto it = locallyAnnouncedIDsMap.cbegin(); it !=locallyAnnouncedIDsMap.cend(); ++it){
					outmsg=outmsg+" "+it->first;
					}
				log(__FUNCTION__, "Locally announced ids: "+outmsg);
				return;
			}
		}
		log(__FUNCTION__, "Interest left unprocessed. Did not know what to do with it..");
	}
		
		
	/**
	 * Serializes the contents of vector to string format for responding to registry file requests.
	 * Format: numOfIDs | id1_len | ID1 | id2_len | ID2 |... 
	 * */
	char* getRegistryFilePayload (std::vector<std::string>* _v){
		unsigned len = 1;
		for (unsigned i=0; i < _v->size(); i++){
			len++; //1 byte sizeOfId
			len+=_v->at(i).length();
			}
			
		char* payload = new char[len];
		unsigned counter=0;
		payload[counter++] = _v->size();
		//std::cout<<"numberOfIDs "<<(unsigned)_v->size() <<std::endl;
		for (unsigned i=0; i < _v->size(); i++){
			payload[counter++] = _v->at(i).length();
			memcpy (payload+counter, _v->at(i).c_str(), _v->at(i).length());
			counter+= _v->at(i).length();
			//std::cout<<"id i"<<(unsigned)i << " -> "<<_v->at(i) <<std::endl;
		}
		return payload;
		}
		
		
	/**
	 * Parses the registry file (1st param) and adds the ids in the vector ids (2nd param)
	 * */
	void getIDsFromRegistryFilePayload ( const char* _payload, std::vector<std::string>& ids ){
		unsigned counter = 2;
		uint8_t numberOfIDs=_payload[counter++];
		//std::cout<<"numberOfIDs "<<(unsigned)numberOfIDs <<std::endl;
		for (uint8_t i=0; i<numberOfIDs; i++){
			uint8_t sizeOfID = _payload[counter++];
			//std::cout<<"sizeOfID "<<(unsigned)sizeOfID <<std::endl;
			std::string _s (_payload+counter, sizeOfID);
			//std::cout<<"_s "<< _s <<std::endl;
			counter+=sizeOfID; 
			ids.push_back(_s);
			}
		
		return;
		}
  
  /**
   * Simply sends a Data packet with the specified _name and payload.
   * */
  bool sendData(const Name &_name, const std::string &payload){
	  auto data = std::make_shared<Data>();
		data->setName(_name);
		data->setFreshnessPeriod(10_s);
		data->setContent(payload);
		// Sign Data packet with default identity
		m_keyChain.sign(*data);
		// m_keyChain.sign(*data, signingByIdentity(<identityName>));
		// m_keyChain.sign(*data, signingByKey(<keyName>));
		// m_keyChain.sign(*data, signingByCertificate(<certName>));
		// m_keyChain.sign(*data, signingWithSha256());
		// Return Data packet to the requester
		//std::cout << "<< D: " << *data << std::endl;
		producer_face.put(*data);
	  return true; 
	  }
  
  void onRegisterFailed(const Name& prefix, const std::string& reason){
		std::cerr << "ERROR: Failed to register prefix '" << prefix
				  << "' with the local forwarder (" << reason << ")\n";
		producer_face.shutdown();
  }
  
  /**
   * Handle incoming Data packets from NDN network.
   * Call-back function.
   * */
   void onData(const Interest&, const Data& data) {
		log(__FUNCTION__, "Received Data with name: "+data.getName().toUri() );
		std::string payload = std::string((const char*)(data.getContent().data()) );
		log(__FUNCTION__, "Data packet payload: \"" +payload+"\"");
		//if this is response to previous getByID request
		if (data.getName().size()<2){
			log(__FUNCTION__, "WARNINGI::Got unexpectedly small content name. Skipping.." );
			return;
			}
		std::size_t isAnACKit = payload.find(ACK_TAG);
		if (isAnACKit != std::string::npos){ 
			log(__FUNCTION__, "Got ACK. Skipping.." );
			return;
		}
		
		std::string _nm = data.getName().at(1).toUri();
		SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(CONSUMER, (const char*)data.getContent().data(), data.getContent().size());
		msg1->setName(_nm.c_str(), _nm.length());
		//msg1->type = BYID_RESPONSE;
		
 		writeIntraSNDSMessage(v_to_service, m_to_service, c_to_service, msg1);
		return;
		
  }

  void onNack(const Interest &, const lp::Nack& nack) const  {
		std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
		
		//log(__FUNCTION__, "Received NACK for Interest: "+interest.getName().toUri() );			
		//std::string _nm = interest.getName().at(1).toUri();
		//SNDS_Intra_Msg* msg1 = new SNDS_Intra_Msg(NACK, (const char*)data.getContent().data(), data.getContent().size());
		//msg1->setName(_nm.c_str(), _nm.length());
		////msg1->type = BYID_RESPONSE;
		//writeIntraSNDSMessage(v_to_service, m_to_service, c_to_service, msg1);
  }

  void  onTimeout(const Interest& interest) const{
		std::cout << "Timeout for " << interest << std::endl;
  }
  
  pthread_mutex_t m_log = PTHREAD_MUTEX_INITIALIZER;
  
  void log(const char* function_name, const char* message){
	pthread_mutex_lock(&m_log);
	std::cout <<function_name<< "::" << message << std::endl;	
	pthread_mutex_unlock(&m_log);
	}

   void log(const char* function_name, std::string message){
	log(function_name, message.c_str());
  }
	
	//util funct to convert boost::string_view to std::string
	std::string boost_string_view_to_string(const boost::string_view &bsv){
		return std::string(std::string_view(bsv.data(),bsv.size()));	
	}
	
	std::string GETdelimiter = "&";
	std::string GETAssignSymbol = "=";
	/**
	 * Assuming URL format "/tag=value&tag=value&tag=value&.."
	 * Returns the value of the "tag"
	 * */
	std::string getfromGETRequest(const std::string &url, const std::string &tag){
		//get tag
		std::size_t pos = url.find(tag);
		if (pos == std::string::npos){
			//std::cout<<"did not find "<<tag<<" in url "<<url<<std::endl;
			return "";
			} 
		std::string token = url.substr(pos, url.length());
		//get value
		token = token.substr(0, token.find(GETdelimiter));
		return token.substr(token.find(GETAssignSymbol)+1, token.length());
		
		}
		
	/**
	 * Assuming GET url variable format "tag=value&tag=value&tag=value&.."
	 * Returns the value of the "tag"
	 * */
	std::string getfromPOSTRequest(const std::string &messageBody, const std::string &tag){
		return getfromGETRequest(messageBody, tag);
		
		}
		
	std::string SUBtag = "isSub";
	std::string SUBURLtag = "suburl";
	/**
	 * Assuming Subscriptions include "SUBtag=true"
	 * */
	bool isSubscriptionRequest(const std::string &url){
		if (getfromGETRequest(url, SUBtag).compare("true")==0)
			return true;
		else
			return false;
		
		} 
	
	std::string getURLfromSubscription(const std::string &url){
		return getfromGETRequest(url, SUBURLtag);
	}
	
	// returns the scope name of the responsible SNDS node for the subscribed name 	
	std::string getSNDSNodeScopeNameForSubscription(const std::string& _id){
		return ID_SCOPE_PREFIX+std::to_string(getSNDSNodeForSubscription(_id));
		}
		
	// returns the ID of the responsible SNDS node for the subscribed name 	
	unsigned getSNDSNodeForSubscription(std::string name){
		std::hash<std::string> hash_fn;
		size_t hash_value = hash_fn(name);
		return hash_value % NUMBER_OF_SNDS_SRVS;
			
		}

	//simply adds the "/" initial char if needed
	void prepareName(std::string & nm){
		if (nm.at(0) != '/'){
			nm = "/"+nm;}
		}

};
} // namespace examples
} // namespace ndn


int main(int argc, char** argv){

    
    ndn::examples::SNDS_SERVICE snds_serv;
    snds_serv.init();
	snds_serv.start_threads();
	
    std::cout<<"Main::Exiting.\n";

    return 0;

}
