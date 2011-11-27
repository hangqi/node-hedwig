#ifndef NODE_HEDWIG_H_
#define NODE_HEDWIG_H_

#include <v8.h>
#include <node.h>

#include <hedwig/client.h>

#include <string>

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
  Persistent<Function> callback;
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

  static void EIO_Callback(eio_req *req);
  static int EIO_AfterCallback(eio_req *req);
private:
  Persistent<Function> callback;
};

// Message Handle Wrapper passed to EIO
struct EIOConsumeData {
  Persistent<Function> callback;
  std::string topic;
  std::string subscriber;
  std::string message;
  Hedwig::OperationCallbackPtr consumeCb;
};

// Message Handler Callback
class HedwigMessageHandler : public Hedwig::MessageHandlerCallback {
public:
  HedwigMessageHandler(Persistent<Function> &jsCallback);
  virtual ~HedwigMessageHandler();

  virtual void consume(const std::string& topic, const std::string& subscriber, const Hedwig::Message& msg, Hedwig::OperationCallbackPtr& callback);

  static void EIO_Consume(eio_req *req);
  static int EIO_AfterConsume(eio_req *req);

protected:
  Persistent<Function> jsCallback;
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
  HedwigConfiguration(const std::string &defaultServer);
  virtual ~HedwigConfiguration();

  virtual int getInt(const std::string& key, int defaultVal) const;

  virtual const std::string get(const std::string& key, const std::string& defaultVal) const;

  virtual bool getBool(const std::string& /*key*/, bool defaultVal) const;

private:
  const std::string defaultServer;
};

// Hedwig Client
class HedwigClient : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;

  static void Init(Handle<Object> target);

protected:
  HedwigClient(const std::string &server);
  ~HedwigClient();

  void publish(const std::string &topic, const std::string &message,
               Persistent<Function> jsCallback);
  void subscribe(const std::string &topic, const std::string &subscriberId,
                 const Hedwig::SubscribeRequest::CreateOrAttach mode,
                 Persistent<Function> jsCallback);
  void unsubscribe(const std::string &topic, const std::string &subscriberId,
                   Persistent<Function> jsCallback);
  void closeSubscription(const std::string &topic, const std::string &subscriberId);
  void startDelivery(const std::string &topic, const std::string &sbuscriberId,
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
};

#endif // NODE_HEDWIG_H_
