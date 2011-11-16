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

using namespace v8;

static Persistent<String> hedwig_publish_symbol;

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

struct EIOCallback {
  int rc;
  Persistent<Function> callback;
};

class HedwigClient : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;

  static void Init(Handle<Object> target);

protected:
  HedwigClient(const std::string &server);
  ~HedwigClient();

  void publish(const std::string &topic, const std::string &message, Persistent<Function> jsCallback);

  // Constructor
  static Handle<Value> New(const Arguments& args);

  // Methods

  static Handle<Value> Publish(const Arguments& args);

private:
  Hedwig::Client *client;
  Hedwig::Configuration *conf;
};

#endif // NODE_HEDWIG_H_
