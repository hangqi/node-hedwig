#include "hedwig.h"
#include <iostream>

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
  std::cout << "Opertion complete" << std::endl;
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
  ADD_PROTOTYPE_METHOD(hedwig, publish, Publish);

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
  // client->getPublisher().publish(topic, message);
  // cb->operationComplete();
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


extern "C" {
  static void init(Handle<Object> target) {
    HedwigClient::Init(target);
  }
  NODE_MODULE(hedwig, init);
}
