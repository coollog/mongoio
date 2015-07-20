var assert = require('assert');

assert.equal(4, process.argv.length, 'usage: yell mongouri tunnelname');

var mongouri = process.argv[2];
var tunnelName = process.argv[3];

require('./io')(mongouri, function (server) {
  var collectionName = 'stream_' + tunnelName;
  var collection = server.db.collection(collectionName);

  process.stdin.resume();
  process.stdin.setEncoding('utf8');

  console.log('YELLING INTO TUNNEL ' + tunnelName);

  process.stdin.on('data', function (line) {
    collection.insertOne({message: line}, function (err) {
      assert.equal(null, err);
    });
  });
});