var hedwig = require('./build/Release/hedwig.node');

var client = new hedwig.Hedwig("localhost:4081:9876");

var topic = 'mytopic';
var subId = 'mysub-7';

var msgHandler = function(thisTopic, thisSub, message, consumeCb) {
    console.log('Received message : ' + message);
    consumeCb.complete();
};

var pubCb = function(error) {
    if (error) {
        console.log('pub failed : ' + error);
    } else {
        console.log('pub succeed !');
    }
};

client.sub(topic, subId, 2, function(error) {
    if (error) {
        console.log('sub failed : ' + error);
    } else {
        console.log('sub succeed !');
        client.startDelivery(topic, subId, msgHandler);
        for (var i = 0; i < 10; i++) {
            client.pub(topic, "msg-" + i, pubCb);
        }
    }
});
