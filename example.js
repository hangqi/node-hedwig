var hedwig = require('./build/default/hedwig.node');

var options = {
        'hedwig.cpp.default_server': "mustlonetaste.peking.corp.yahoo.com:9875:9876",
        'hedwig.cpp.max_msgqueue_size': 20
    };
//var client = new hedwig.Hedwig("mustlonetaste.peking.corp.yahoo.com:9875:9876");

var client = new hedwig.Hedwig(options, './log4cxx.properties');

var topic = 'topic_f';
var subId = 'sub_f';

var cnt = 0;
var msgHandler = function(thisTopic, thisSub, message, consumeCb) {
    cnt += 1;
    console.log('Received message : ' + JSON.stringify(message));
    consumeCb.complete();
    if (cnt == 10) {
        client.stopDelivery(thisTopic, thisSub);
    }
};

var pubCb = function(error) {
    if (error) {
        console.log('pub failed : ' + error);
    } else {
        console.log('pub succeed !');
    }
};


console.log(">>>> client start ... ");

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

