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

#include "hedwig.h"
#include <iostream>
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include <hedwig/exceptions.h>

static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("hedwig."__FILE__));

#define NODE_DEFINE_CONSTANT_BY_NAME(target, name, constant)       \
    (target)->Set(v8::String::NewSymbol(name),  \
            v8::Integer::New(constant),                 \
            static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete))

namespace HedwigMessage {

    const char K_SRCREGION[] = "srcRegion";
    const char K_BODY[] = "body";
    const char K_MSGID[] = "msgId";
    const char K_LOCALCOMPONENT[] = "localComponent";
    const char K_REMOTECOMPONENTS[] = "remoteComponents";
    const char K_REGION[] = "region";
    const char K_SEQID[] = "seqId";

#define STR_LH(var) \
    String::New(var) 

#define NUM_LH(var) \
    Number::New(var)

    // Helper to convert Hedwig::Message (protobuf message) to JS Object 
    static Local<Object> ToJSObject(Hedwig::Message& message) 
    {
        HandleScope scope;
        Local<Object> ret = Object::New();
        if (message.has_srcregion()) {
            ret->Set(STR_LH(K_SRCREGION), STR_LH(message.srcregion().c_str())); 
        }
        if (message.has_body()) {
            ret->Set(STR_LH(K_BODY), STR_LH(message.body().c_str()));
        }
        if (message.has_msgid()) {
            Local<Object> msgid = Object::New();

            Hedwig::MessageSeqId id = message.msgid();
            if (id.has_localcomponent()) {
                msgid->Set(STR_LH(K_LOCALCOMPONENT), NUM_LH(id.localcomponent()));
            } 
            int remote_size = id.remotecomponents_size();
            if (remote_size > 0) {
                Local<Array> array = Array::New(remote_size);
                for (int i = 0; i < remote_size; ++i) {
                    Hedwig::RegionSpecificSeqId rid = id.remotecomponents(i);     
                    Local<Object> ridObj = Object::New();
                    if (rid.has_region()) {
                        ridObj->Set(STR_LH(K_REGION), STR_LH(rid.region().c_str()));
                    }
                    if (rid.has_seqid()) {
                        ridObj->Set(STR_LH(K_SEQID), NUM_LH(rid.seqid()));    
                    }
                    array->Set(i, ridObj);
                }
                msgid->Set(STR_LH(K_REMOTECOMPONENTS), array);
            }
            ret->Set(STR_LH(K_MSGID), msgid);
        }
        return scope.Close(ret);
    }
#undef STR_LH 
#undef NUM_LH
};

// Hedwig Configuration
    HedwigConfiguration::HedwigConfiguration()
: strMap(std::map<std::string, std::string>()),
    boolMap(std::map<std::string, bool>()),
    intMap(std::map<std::string, int>())
{}

HedwigConfiguration::~HedwigConfiguration() {
    strMap.clear();
    boolMap.clear();
    intMap.clear();
}

int HedwigConfiguration::getInt(const std::string& key, int defaultVal) const {
    std::map<std::string, int>::const_iterator it = intMap.find(key);
    if (it != intMap.end()) {
        return it->second;
    }
    return defaultVal;
}

const std::string HedwigConfiguration::get(const std::string &key, const std::string &defaultVal) const {
    std::map<std::string, std::string>::const_iterator it = strMap.find(key);
    if (it != strMap.end()) {
        return it->second;
    }
    return defaultVal;
}

bool HedwigConfiguration::getBool(const std::string & key, bool defaultVal) const {
    std::map<std::string, bool>::const_iterator it = boolMap.find(key);
    if (it != boolMap.end()) {
        return it->second;
    }
    return defaultVal;
}

inline void HedwigConfiguration::setStr(const std::string &key, const std::string &val) {
    strMap.insert(std::pair<std::string, std::string>(key, val));
}

inline void HedwigConfiguration::setInt(const std::string &key, int val) {
    intMap.insert(std::pair<std::string, int>(key, val));
}

inline void HedwigConfiguration::setBool(const std::string &key, bool val) {
    boolMap.insert(std::pair<std::string, bool>(key, val));
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
    cb->errMsg = errorMsg ? strdup(errorMsg) : NULL;
    cb->callback = callback;

    eio_nop(EIO_PRI_DEFAULT, EIO_AfterCallback, cb);
}

int HedwigOperationCallback::EIO_AfterCallback(eio_req *req) {
    HandleScope scope;
    EIOCallback *cb = static_cast<EIOCallback *>(req->data);
    ev_unref(EV_DEFAULT_UC);
    Local<Value> argv[1];
    if (cb->rc) {
        argv[0] = V8EXC(cb->errMsg);
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

JSCallbackWrapper::JSCallbackWrapper(Persistent<Function> &jsCallback)
    : jsCallback_(jsCallback),active_(true) {
    }

JSCallbackWrapper::~JSCallbackWrapper() {
    jsCallback_.Dispose();
}

inline void JSCallbackWrapper::inActive() {
    active_ = false;
}

inline void JSCallbackWrapper::active() {
    active_ = true;
}

Local<Value> JSCallbackWrapper::Call(Handle<Object> recv, int argc, Handle<Value> argv[]) {
    HandleScope scope;
    if (active_) 
        return jsCallback_->Call(recv, argc, argv);

    return scope.Close(Undefined());
}

// Message Handler
HedwigMessageHandler::HedwigMessageHandler(JSWPtr &jsCallback)
    : jsCallback(jsCallback) {
    }

HedwigMessageHandler::~HedwigMessageHandler() {
}

std::queue<EIOConsumeData*> HedwigMessageHandler::req_msg_queue = std::queue<EIOConsumeData*>();
pthread_mutex_t HedwigMessageHandler::queue_mutex = PTHREAD_MUTEX_INITIALIZER;
ev_async HedwigMessageHandler::ev_hedwig_consume_notifier;

void HedwigMessageHandler::Init() {
    ev_async_init(&ev_hedwig_consume_notifier, HedwigMessageHandler::AfterConsume);
    ev_async_start(EV_DEFAULT_UC_ &ev_hedwig_consume_notifier);
    ev_unref(EV_DEFAULT_UC);
}

void HedwigMessageHandler::consume(const std::string& topic, const std::string& subscriber, const Hedwig::Message& msg, Hedwig::OperationCallbackPtr& callback) {
    EIOConsumeData *consumeData = new EIOConsumeData();
    consumeData->callback = jsCallback;
    consumeData->topic = topic;
    consumeData->subscriber = subscriber;
    consumeData->message = msg;
    consumeData->consumeCb = callback;

    LOG4CXX_INFO(logger, "consume called, push to queue, and notify ev_loop: " << consumeData->topic << " " << consumeData->subscriber << " " << consumeData->message.msgid().localcomponent());

    pthread_mutex_lock(&queue_mutex);
    req_msg_queue.push(consumeData);
    pthread_mutex_unlock(&queue_mutex);

    // notify v8 main thread that there is a new message on the queue.
    ev_ref(EV_DEFAULT_UC);
    ev_async_send(EV_DEFAULT_UC_ &ev_hedwig_consume_notifier);
}

void HedwigMessageHandler::AfterConsume(EV_P_ ev_async *watcher, int revents) {
    HandleScope scope;
    assert(watcher == &ev_hedwig_consume_notifier);
    assert(revents == EV_ASYNC);

    // using while here because the ev_async_send is level trigger
    while (true) {
        EIOConsumeData *consumeData = NULL;
        pthread_mutex_lock(&queue_mutex);
        if (req_msg_queue.size() > 0) {
            consumeData = req_msg_queue.front();
            req_msg_queue.pop();
        }
        pthread_mutex_unlock(&queue_mutex);

        if (! consumeData) 
            return;

        ev_unref(EV_DEFAULT_UC);
        LOG4CXX_INFO(logger, "about consume callback to JS: " << consumeData->topic << " " << consumeData->subscriber << " " << consumeData->message.msgid().localcomponent());
        Local<Value> argv[4];
        argv[0] = String::New(consumeData->topic.c_str());
        argv[1] = String::New(consumeData->subscriber.c_str());
        argv[2] = HedwigMessage::ToJSObject(consumeData->message);
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
    }
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

    // Exports CONSTANT
    NODE_DEFINE_CONSTANT_BY_NAME(target, "CREATE", Hedwig::SubscribeRequest::CREATE);
    NODE_DEFINE_CONSTANT_BY_NAME(target, "ATTACH", Hedwig::SubscribeRequest::ATTACH);
    NODE_DEFINE_CONSTANT_BY_NAME(target, "CREATE_OR_ATTACH", Hedwig::SubscribeRequest::CREATE_OR_ATTACH);

    // Make it visible in Javascript
    target->Set(String::NewSymbol("Hedwig"),
            constructor_template->GetFunction());
}

HedwigClient::HedwigClient(HedwigConfiguration* config, const char *log4cxxCfg) : ObjectWrap() {
    conf = config;
    client = new Hedwig::Client(*conf);
    log4cxx::PropertyConfigurator::configure(log4cxxCfg);
}

HedwigClient::~HedwigClient() {
    delete client;
    delete conf;
}

void HedwigClient::publish(const std::string &topic, const std::string &message, Persistent<Function> jsCallback) {
    Hedwig::OperationCallbackPtr cb(new HedwigOperationCallback(jsCallback));
    client->getPublisher().asyncPublish(topic, message, cb);
    ev_ref(EV_DEFAULT_UC);
}

void HedwigClient::subscribe(const std::string &topic, const std::string &subscriberId, 
        const Hedwig::SubscribeRequest::CreateOrAttach mode,
        Persistent<Function> jsCallback) {
    Hedwig::OperationCallbackPtr cb(new HedwigOperationCallback(jsCallback));
    client->getSubscriber().asyncSubscribe(topic, subscriberId, mode, cb);
    ev_ref(EV_DEFAULT_UC);
}

void HedwigClient::unsubscribe(const std::string &topic, const std::string &subscriberId,
        Persistent<Function> jsCallback) {
    Hedwig::OperationCallbackPtr cb(new HedwigOperationCallback(jsCallback));
    client->getSubscriber().asyncUnsubscribe(topic, subscriberId, cb);
    ev_ref(EV_DEFAULT_UC);
}

void HedwigClient::closeSubscription(const std::string &topic, const std::string &subscriberId) {
    client->getSubscriber().closeSubscription(topic, subscriberId);
}

int HedwigClient::startDelivery(const std::string &topic, const std::string &subscriberId,
        Persistent<Function> jsCallback) {

    std::map< std::pair<std::string, std::string>, JSWPtr >::iterator it = activedTopicSubscribers.find(std::pair<std::string, std::string>(topic, subscriberId));
    if (it != activedTopicSubscribers.end()) {
        return -1;
    }
    JSWPtr jcw(new JSCallbackWrapper(jsCallback));
    Hedwig::MessageHandlerCallbackPtr mcb(new HedwigMessageHandler(jcw));
    client->getSubscriber().startDelivery(topic, subscriberId, mcb);
    ev_ref(EV_DEFAULT_UC);
    activedTopicSubscribers[std::pair<std::string, std::string>(topic, subscriberId)] = jcw;
    return 0;
}

void HedwigClient::stopDelivery(const std::string &topic, const std::string &subscriberId) {
    client->getSubscriber().stopDelivery(topic, subscriberId);
    ev_unref(EV_DEFAULT_UC);
    std::map< std::pair<std::string, std::string>, JSWPtr>::iterator it = activedTopicSubscribers.find(std::pair<std::string, std::string>(topic, subscriberId));
    if (it != activedTopicSubscribers.end()) {
        it->second->inActive();
        activedTopicSubscribers.erase(it);
    }
}

/**
 * Create new Hedwig object
 *
 * @constructor
 */
Handle<Value> HedwigClient::New(const Arguments& args) {
    HandleScope scope;

    HedwigConfiguration * config; 
    REQ_STR_ARG(1, log4xxCfg);
    if (args[0]->IsObject()) {
        config = new HedwigConfiguration();
        Local<Object> obj = args[0]->ToObject();
        Local<Array> properties = obj->GetPropertyNames();
        for (int i = 0, length = properties->Length(); i < length; ++i)
        {
            Local<Value> key = properties->Get(0);
            Local<Value> val = obj->Get(key);
            String::AsciiValue keyStr(key->ToString());
            if (val->IsString()) {
                String::Utf8Value valStr(val->ToString());
                config->setStr(*keyStr, *valStr);
            } else if (val->IsBoolean()) {
                config->setBool(*keyStr, val->BooleanValue()); 
            } else if (val->IsInt32()) {
                config->setInt(*keyStr, val->Int32Value());
            }
        }
    } else {
        return THREXC("parameter must be an Object");
    }

    HedwigClient *hedwig = new HedwigClient(config, *log4xxCfg);
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
    int rc = hedwig->startDelivery(*topic, *subscriberId, jsMsgHandler);

    if (rc) {
        Local<Value> argv[1];
        argv[0] = V8EXC("already startDelivery");
        TryCatch try_catch;

        msgHandler->Call(Context::GetCurrent()->Global(), 1, argv);

        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }
    }
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
        HedwigMessageHandler::Init();
    }
    NODE_MODULE(hedwig, init);
}
