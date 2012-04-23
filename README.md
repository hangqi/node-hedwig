A nodejs client for Hedwig, which is a large scale pub/sub system built on topc of ZooKeeper and BookKeeper.

# node-hedwig

## Description

A nodejs client for [Hedwig](http://zookeeper.apache.org/bookkeeper/), which is a large scale pub/sub system built on top of ZooKeeper and BookKeeper.

## API Usage:

Create a hedwig client: Hedwig(options, log4j_properties_file_path)
- param: JSON Object. Configuration settings used to construct a hedwig client.
- param: String. Log4cxx properties file path.
<pre>
    var client = new hedwig.Hedwig(options, './log4cxx.properties');
</pre>
Construct an operation callback: function(error)
- param: String. Error message, if operation succeed, error is null. otherwise, it returns the error message.

    var pubCb = function(error) {
        if (error) {
            console.log('pub failed : ' + error);
        } else {
            console.log('pub succeed !');
        }
    };

Publish messages: Hedwig#pub(topic, message, callback)
- param: String. Topic name.
- param: String. Message to publish.
- param: Function. Callback after message published.

    client.pub(topic, "msg", pubCb);

Subscribe topics: Hedwig#sub(topic, subscriber_id, mode, callback)
- param: String. Topic name.
- param: String. Subscriber id.
- param: Constant. Subscription mode. Available is CREATE, ATTACH, CREATE_OR_ATTACH.
- param: Function. Callback after subscribe operation executed.

    client.sub(topic, subId, CREATE_OR_ATTACH, subCb);

Unsubscribe topics: Hedwig#unsub(topic, subscriber_id)
- param: String. Topic name.
- param: String. Subscriber id.

    client.unsub(topic, subId);

Close subscription: Hedwig#closesub(topic, subscriber_id) (which is different with #unsub, it doesn't remove subscription in server side, just clean client state.)
- param: String. Topic name.
- param: String. Subscriber id.

    client.closesub(topic, subId);

Construct a message handler to process received messages: function(topic, subscriber_id, message, consume_callback)
- param: String. Topic name.
- param: String. Subscriber id.
- param: Object. Message object.
- param: Callback. Callback to tell server consume this message after client processed it.

    var msgHandler = function(thisTopic, thisSub, message, consumeCb) {
        console.log('Received message : ' + JSON.stringify(message));
        consumeCb.complete();
    };

Start to receive messages: Hedwig#startDelivery(topic, subscriber_id, message_handler)
- param: String. Topic name.
- param: String. Subscriber id.
- param: Function. Message handler used to process recevied messages.

    client.startDelivery(topic, subId, msgHandler);

Stop to receive messages: Hedwig#stopDelivery(topic, subscriber_id)
- param: String. Topic name.
- param: String. Subscriber id.

    client.stopDelivery(topic, subId);
    
## Building

