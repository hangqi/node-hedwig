/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed with this
 * work for additional information regarding copyright ownership. The ASF
 * licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef NODE_HEDWIG_H_
#define NODE_HEDWIG_H_

#include <v8.h>
#include <node.h>

#include <hedwig/client.h>

#include <string>
#include <map>

#define ADD_PROTOTYPE_METHOD(class, name, method) \
class ## _ ## name ## _symbol = NODE_PSYMBOL(#name); \
NODE_SET_PROTOTYPE_METHOD(constructor_template, #name, method);

#define V8EXC(str) Exception::Error(String::New(str))
#define THREXC(str) ThrowException(Exception::Error(String::New(str)))
#define OBJUNWRAP ObjectWrap::Unwrap

#define REQ_INT_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsInt32()) { \
  return ThrowException(Exception::TypeError( \
  String::New("Argument " #I " must be an integer"))); \
} \
int32_t VAR = args[I]->Int32Value();

#define REQ_STR_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsString()) { \
  return ThrowException(Exception::TypeError( \
  String::New("Argument " #I " must be a string"))); \
} \
String::Utf8Value VAR(args[I]->ToString());

#define REQ_FUN_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsFunction()) { \
  return ThrowException(Exception::TypeError( \
  String::New("Argument " #I " must be a function"))); \
} \
Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_EXT_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsExternal()) { \
  return ThrowException(Exception::TypeError( \
  String::New("Argument " #I " invalid"))); \
} \
Local<External> VAR = Local<External>::Cast(args[I]);

using namespace v8;

static Persistent<String> hedwig_pub_symbol;
static Persistent<String> hedwig_sub_symbol;
static Persistent<String> hedwig_unsub_symbol;
static Persistent<String> hedwig_closesub_symbol;
static Persistent<String> hedwig_startDelivery_symbol;
static Persistent<String> hedwig_stopDelivery_symbol;
static Persistent<String> callback_complete_symbol;
static Persistent<String> callback_failed_symbol;

// Callback Wrapper passed to EIO
struct EIOCallback {
  int rc;
  char * errMsg;
  Persistent<Function> callback;

  EIOCallback():rc(0),errMsg(NULL) {}
  ~EIOCallback() {
    if (errMsg) {
        delete[] errMsg;
        errMsg = NULL;
    } 
  }
};

// Hedwig Callback
class HedwigOperationCallback : public Hedwig::OperationCallback {
public:
  HedwigOperationCallback(Persistent<Function> &jsCallback);
  ~HedwigOperationCallback();

  virtual void operationComplete();
  virtual void operationFailed(const std::exception& exception);
protected:
  void doJsCallback(const char* errorMsg);

  static int EIO_AfterCallback(eio_req *req);
private:
  Persistent<Function> callback;
};

// a Wrapper for JS callback function
class JSCallbackWrapper {
public:
    JSCallbackWrapper(Persistent<Function> &jsCallback);
    virtual ~JSCallbackWrapper();
    Local<Value> Call(Handle<Object> recv, int argc, Handle<Value> argv[]);
    
    inline void inActive();
    inline void active();
protected:
    Persistent<Function> jsCallback_;
    bool active_;
};

typedef boost::shared_ptr<JSCallbackWrapper> JSWPtr;

// Message Handle Wrapper passed to EIO
struct EIOConsumeData {
  JSWPtr callback;
  std::string topic;
  std::string subscriber;
  Hedwig::Message message;
  Hedwig::OperationCallbackPtr consumeCb;
};

// Message Handler Callback
class HedwigMessageHandler : public Hedwig::MessageHandlerCallback {
public:
  HedwigMessageHandler(JSWPtr &jsCallback);
  virtual ~HedwigMessageHandler();

  virtual void consume(const std::string& topic, const std::string& subscriber, const Hedwig::Message& msg, Hedwig::OperationCallbackPtr& callback);

  static int EIO_AfterConsume(eio_req *req);

protected:
  JSWPtr jsCallback;
};

// OperationCallback Wrapper
class OperationCallbackWrapper : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;

  static void Init(Handle<Object> target);

  static Handle<Value> Complete(const Arguments& args);
  static Handle<Value> Failed(const Arguments& args);
protected:
  
  OperationCallbackWrapper(Hedwig::OperationCallbackPtr &callback);
  ~OperationCallbackWrapper();

  // Constructor
  static Handle<Value> New(const Arguments& args);

private:
  Hedwig::OperationCallbackPtr callback;
};

// Hedwig Configuration
class HedwigConfiguration : public Hedwig::Configuration {
public:
  HedwigConfiguration();
  virtual ~HedwigConfiguration();

  virtual int getInt(const std::string& key, int defaultVal) const;

  virtual const std::string get(const std::string& key, const std::string& defaultVal) const;

  virtual bool getBool(const std::string& key, bool defaultVal) const;

  inline void setStr(const std::string& key, const std::string& value);
  inline void setInt(const std::string& key, const int value);
  inline void setBool(const std::string& key, const bool value);
private:
  std::map<std::string, std::string> strMap;
  std::map<std::string, bool> boolMap;
  std::map<std::string, int> intMap;
};

// Hedwig Client
class HedwigClient : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;

  static void Init(Handle<Object> target);

protected:
  HedwigClient(HedwigConfiguration*, const char *);
  ~HedwigClient();

  void publish(const std::string &topic, const std::string &message,
               Persistent<Function> jsCallback);
  void subscribe(const std::string &topic, const std::string &subscriberId,
                 const Hedwig::SubscribeRequest::CreateOrAttach mode,
                 Persistent<Function> jsCallback);
  void unsubscribe(const std::string &topic, const std::string &subscriberId,
                   Persistent<Function> jsCallback);
  void closeSubscription(const std::string &topic, const std::string &subscriberId);
  int startDelivery(const std::string &topic, const std::string &sbuscriberId,
                     Persistent<Function> jsCallback);
  void stopDelivery(const std::string &topic, const std::string &subscriberId);

  // Constructor
  static Handle<Value> New(const Arguments& args);

  // Methods

  static Handle<Value> Publish(const Arguments& args);
  static Handle<Value> Subscribe(const Arguments& args);
  static Handle<Value> Unsubscribe(const Arguments& args);
  static Handle<Value> CloseSubscription(const Arguments& args);
  static Handle<Value> StartDelivery(const Arguments& args);
  static Handle<Value> StopDelivery(const Arguments& args);

private:
  Hedwig::Client *client;
  Hedwig::Configuration *conf;

/** 
 * activedTopicSubscribers -- record all the active subscribers, which called 
 * StartDelivery(topic, subId), but not StopDelivery(topic, subId)
 * when a subscriber called StartDelivery(topic, subId), an new entry will be added into 
 * this map if there does not exist entry(topic, subId), otherwise the StartDelivery 
 * operation will fail when a subscriber called StopDelivery(topic, subId), the 
 * entry(topic, subId) will be removed from the map
 *
 * The reason we need this map is that, when app calls StartDelivery(topic, subId, callback), a new messageHandler instance will be created, 
 * and keep the JS callback in it. When app calls StopDelivery(topic, subId), hedwig c++ client will delete the associated messageHandler, and
 * stop calling consume function in messageHandler. But because the when hedwig c++ client calls the consume function, the messageHandler will send 
 * out an eio opartion to call JS callback, which will result in when the StopDelivery is called, there are some eio operation pending on the eio queue
 * and those eio operation should not be called to JS
 */
  std::map< std::pair<std::string, std::string>, JSWPtr> activedTopicSubscribers;
};

#endif // NODE_HEDWIG_H_
