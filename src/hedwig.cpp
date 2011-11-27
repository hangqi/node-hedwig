#include "hedwig.h"
#include <iostream>

#include <hedwig/exceptions.h>

// Hedwig Configuration
HedwigConfiguration::HedwigConfiguration(const std::string &defaultServer)
  : defaultServer(defaultServer) {
}

HedwigConfiguration::~HedwigConfiguration() {}

int HedwigConfiguration::getInt(const std::string& /* key */, int defaultVal) const {
  return defaultVal;
}

const std::string HedwigConfiguration::get(const std::string &key, const std::string &defaultVal) const {
  if (Configuration::DEFAULT_SERVER == key) {
    return defaultServer;
  } else {
    return defaultVal;
  }
}

bool HedwigConfiguration::getBool(const std::string & /*key*/, bool defaultVal) const {
  return defaultVal;
}

// Hedwig Operation Callback
HedwigOperationCallback::HedwigOperationCallback(Persistent<Function> &jsCallback)
  : callback(jsCallback) {
}

HedwigOperationCallback::~HedwigOperationCallback() {
}

void HedwigOperationCallback::operationComplete() {
  doJsCallback(0);
}

void HedwigOperationCallback::operationFailed(const std::exception& exception) {
  doJsCallback(exception.what());
}

void HedwigOperationCallback::doJsCallback(const char* errorMsg) {
  EIOCallback *cb = new EIOCallback();
  cb->rc = 0 == errorMsg ? 0 : -1;
  cb->callback = callback;

  eio_custom(EIO_Callback, EIO_PRI_DEFAULT, EIO_AfterCallback, cb);
}

void HedwigOperationCallback::EIO_Callback(eio_req *req) {
  // do nothing
}

int HedwigOperationCallback::EIO_AfterCallback(eio_req *req) {
  HandleScope scope;
  EIOCallback *cb = static_cast<EIOCallback *>(req->data);
  Local<Value> argv[1];
  if (cb->rc) {
    argv[0] = V8EXC("Failed");
  } else {
    argv[0] = Local<Value>::New(Null());
  }
  TryCatch try_catch;

  cb->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }

  cb->callback.Dispose();
  delete cb;
  return 0;
}

// Message Handler
HedwigMessageHandler::HedwigMessageHandler(Persistent<Function> &jsCallback)
  : jsCallback(jsCallback) {
}

HedwigMessageHandler::~HedwigMessageHandler() {
  jsCallback.Dispose();
}

void HedwigMessageHandler::consume(const std::string& topic, const std::string& subscriber, const Hedwig::Message& msg, Hedwig::OperationCallbackPtr& callback) {
  EIOConsumeData *consumeData = new EIOConsumeData();
  consumeData->callback = jsCallback;
  consumeData->topic = topic;
  consumeData->subscriber = subscriber;
  consumeData->message = msg.body();
  consumeData->consumeCb = callback;

  eio_custom(EIO_Consume, EIO_PRI_DEFAULT, EIO_AfterConsume, consumeData);
}

void HedwigMessageHandler::EIO_Consume(eio_req *req) {
  // do nothing
}

int HedwigMessageHandler::EIO_AfterConsume(eio_req *req) {
  HandleScope scope;
  EIOConsumeData *consumeData = static_cast<EIOConsumeData *>(req->data);
  Local<Value> argv[4];
  argv[0] = String::New(consumeData->topic.c_str());
  argv[1] = String::New(consumeData->subscriber.c_str());
  argv[2] = String::New(consumeData->message.c_str());
  // wrap callback
  Local<Value> cbArgv[1];
  cbArgv[0] = External::New(&(consumeData->consumeCb));
  Persistent<Object> jsConsumeCallback(
    OperationCallbackWrapper::constructor_template->GetFunction()->NewInstance(1, cbArgv));
  argv[3] = Local<Object>::New(jsConsumeCallback);
  
  TryCatch try_catch;

  consumeData->callback->Call(Context::GetCurrent()->Global(), 4, argv);

  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }

  delete consumeData;
  return 0;
}

// OperationCallback Wrapper
Persistent<FunctionTemplate> OperationCallbackWrapper::constructor_template;

OperationCallbackWrapper::OperationCallbackWrapper(Hedwig::OperationCallbackPtr &callback) : callback(callback) {}

OperationCallbackWrapper::~OperationCallbackWrapper() {}

void OperationCallbackWrapper::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  // Constructor
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("HedwigOperationCallback"));

  Local<ObjectTemplate> instance_template =
    constructor_template->InstanceTemplate();

  // Methods
  ADD_PROTOTYPE_METHOD(callback, complete, Complete);
  ADD_PROTOTYPE_METHOD(callback, failed, Failed);

  // target->Set(String::NewSymbol("HedwigOperationCallback"), constructor_template->GetFunction());
}

/**
 * Create new operation callback object
 *
 * @constructor
 */
Handle<Value> OperationCallbackWrapper::New(const Arguments& args) {
  HandleScope scope;

  REQ_EXT_ARG(0, callback); 
  Hedwig::OperationCallbackPtr callbackPtr =
    *(static_cast<Hedwig::OperationCallbackPtr*>(callback->Value()));

  OperationCallbackWrapper *wrapper = new OperationCallbackWrapper(callbackPtr); 
  wrapper->Wrap(args.This());

  return args.This();
}

Handle<Value> OperationCallbackWrapper::Complete(const Arguments& args) {
  OperationCallbackWrapper *wrapper = OBJUNWRAP<OperationCallbackWrapper>(args.This());
  wrapper->callback->operationComplete();
  return Undefined();
}

Handle<Value> OperationCallbackWrapper::Failed(const Arguments& args) {
  OperationCallbackWrapper *wrapper = OBJUNWRAP<OperationCallbackWrapper>(args.This());
  // TODO: hard code exception
  wrapper->callback->operationFailed(Hedwig::ClientException());
  return Undefined();
}

// Hedwig

Persistent<FunctionTemplate> HedwigClient::constructor_template;

void HedwigClient::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  // Constructor
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Hedwig"));

  Local<ObjectTemplate> instance_template =
    constructor_template->InstanceTemplate();

  // Methods
  ADD_PROTOTYPE_METHOD(hedwig, pub, Publish);
  ADD_PROTOTYPE_METHOD(hedwig, sub, Subscribe);
  ADD_PROTOTYPE_METHOD(hedwig, unsub, Unsubscribe);
  ADD_PROTOTYPE_METHOD(hedwig, closesub, CloseSubscription);
  ADD_PROTOTYPE_METHOD(hedwig, startDelivery, StartDelivery);
  ADD_PROTOTYPE_METHOD(hedwig, stopDelivery, StopDelivery);

  // Make it visible in Javascript
  target->Set(String::NewSymbol("Hedwig"),
              constructor_template->GetFunction());
}

HedwigClient::HedwigClient(const std::string &server) : ObjectWrap() {
  conf = new HedwigConfiguration(server);
  client = new Hedwig::Client(*conf);
}

HedwigClient::~HedwigClient() {
  delete client;
  delete conf;
}

void HedwigClient::publish(const std::string &topic, const std::string &message, Persistent<Function> jsCallback) {
  Hedwig::OperationCallbackPtr cb(new HedwigOperationCallback(jsCallback));
  client->getPublisher().asyncPublish(topic, message, cb);
}

void HedwigClient::subscribe(const std::string &topic, const std::string &subscriberId, 
                              const Hedwig::SubscribeRequest::CreateOrAttach mode,
                              Persistent<Function> jsCallback) {
  Hedwig::OperationCallbackPtr cb(new HedwigOperationCallback(jsCallback));
  client->getSubscriber().asyncSubscribe(topic, subscriberId, mode, cb);
}

void HedwigClient::unsubscribe(const std::string &topic, const std::string &subscriberId,
                               Persistent<Function> jsCallback) {
  Hedwig::OperationCallbackPtr cb(new HedwigOperationCallback(jsCallback));
  client->getSubscriber().asyncUnsubscribe(topic, subscriberId, cb);
}

void HedwigClient::closeSubscription(const std::string &topic, const std::string &subscriberId) {
  client->getSubscriber().closeSubscription(topic, subscriberId);
}

void HedwigClient::startDelivery(const std::string &topic, const std::string &subscriberId,
                                 Persistent<Function> jsCallback) {
  Hedwig::MessageHandlerCallbackPtr mcb(new HedwigMessageHandler(jsCallback));
  client->getSubscriber().startDelivery(topic, subscriberId, mcb);
}

void HedwigClient::stopDelivery(const std::string &topic, const std::string &subscriberId) {
  client->getSubscriber().stopDelivery(topic, subscriberId);
}

/**
 * Create new Hedwig object
 *
 * @constructor
 */
Handle<Value> HedwigClient::New(const Arguments& args) {
  HandleScope scope;

  std::string server;
  if (args[0]->IsString()) {
    String::Utf8Value host(args[0]->ToString());
    server = std::string(*host);
  } else {
    return THREXC("No hostname defined");
  }

  HedwigClient *hedwig = new HedwigClient(server);
  hedwig->Wrap(args.This());

  return args.This();
}

/**
 * Publish a message to hedwig client
 *
 * @param {String} topic
 * @param {String} message
 * @param {Function(error, success)} callback
 */
Handle<Value> HedwigClient::Publish(const Arguments& args) {
  REQ_STR_ARG(0, topic);
  REQ_STR_ARG(1, message);
  REQ_FUN_ARG(2, callback);

  HedwigClient *hedwig = OBJUNWRAP<HedwigClient>(args.This());

  Persistent<Function> jsCallback = Persistent<Function>::New(callback);

  hedwig->publish(*topic, *message, jsCallback);

  return Undefined();
}

/**
 * Subscribe a topic as a subscriber
 *
 * @param {String} topic
 * @param {String} subscriber id
 * @param {Integer} create mode
 * @param {Function(error, success)} callback
 */
Handle<Value> HedwigClient::Subscribe(const Arguments& args) {
  REQ_STR_ARG(0, topic);
  REQ_STR_ARG(1, subscriberId);
  REQ_INT_ARG(2, createModeValue);
  REQ_FUN_ARG(3, callback);

  Hedwig::SubscribeRequest::CreateOrAttach mode =
    Hedwig::SubscribeRequest::CREATE;
  if (1 == createModeValue) {
    mode = Hedwig::SubscribeRequest::ATTACH;
  } else if (2 == createModeValue) {
    mode = Hedwig::SubscribeRequest::CREATE_OR_ATTACH;
  }

  HedwigClient *hedwig = OBJUNWRAP<HedwigClient>(args.This());

  Persistent<Function> jsCallback = Persistent<Function>::New(callback);

  hedwig->subscribe(*topic, *subscriberId, mode, jsCallback);

  return Undefined();
}

/**
 * Unsubscribe a topic as a subscriber
 *
 * @param {String} topic
 * @param {String} subscriber id
 * @param {Function(error, success)} callback
 */
Handle<Value> HedwigClient::Unsubscribe(const Arguments& args) {
  REQ_STR_ARG(0, topic);
  REQ_STR_ARG(1, subscriberId);
  REQ_FUN_ARG(2, callback);

  HedwigClient *hedwig = OBJUNWRAP<HedwigClient>(args.This());

  Persistent<Function> jsCallback = Persistent<Function>::New(callback);

  hedwig->unsubscribe(*topic, *subscriberId, jsCallback);

  return Undefined();
}

/**
 * Close Subscription
 *
 * @param {String} topic
 * @param {String} subscriber id
 */
Handle<Value> HedwigClient::CloseSubscription(const Arguments& args) {
  REQ_STR_ARG(0, topic);
  REQ_STR_ARG(1, subscriberId);

  HedwigClient *hedwig = OBJUNWRAP<HedwigClient>(args.This());

  hedwig->closeSubscription(*topic, *subscriberId);

  return Undefined();
}

/**
 * Start delivery handler
 *
 * @param {String} topic
 * @param {String} subscriber id
 * @param {Function(topic, subId, message, cbFunc)} message handle
 */
Handle<Value> HedwigClient::StartDelivery(const Arguments& args) {
  REQ_STR_ARG(0, topic);
  REQ_STR_ARG(1, subscriberId);
  REQ_FUN_ARG(2, msgHandler);

  HedwigClient *hedwig = OBJUNWRAP<HedwigClient>(args.This());
  Persistent<Function> jsMsgHandler= Persistent<Function>::New(msgHandler);
  hedwig->startDelivery(*topic, *subscriberId, jsMsgHandler);
  return Undefined();
}

/**
 * Stop message deliver handler
 *
 * @param {String} topic
 * @param {String} subscriber id
 */
Handle<Value> HedwigClient::StopDelivery(const Arguments& args) {
  REQ_STR_ARG(0, topic);
  REQ_STR_ARG(1, subscriberId);

  HedwigClient *hedwig = OBJUNWRAP<HedwigClient>(args.This());
  hedwig->stopDelivery(*topic, *subscriberId);
  return Undefined();
}

extern "C" {
  static void init(Handle<Object> target) {
    OperationCallbackWrapper::Init(target);
    HedwigClient::Init(target);
  }
  NODE_MODULE(hedwig, init);
}
