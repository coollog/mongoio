var assert = require('assert');

assert.equal(4, process.argv.length, 'usage: listen mongouri tunnelname');

var mongouri = process.argv[2];
var tunnelName = process.argv[3];

require('./io')(mongouri, function (server) {
  var collectionName = 'stream_' + tunnelName;
  var collection = server.db.collection(collectionName);

  server.listen(collection, function (doc) {
    try {
        process.stdout.write(doc.message);
    } catch (e) {}
  });
});