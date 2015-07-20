// SETUP FILE FOR SOCKETIO

module.exports = function (mongouri, callback) {
  var fs = require('fs'),
      MongoClient = require('mongodb').MongoClient,
      assert = require('assert');

  (function setup() { // setup function, shouldn't need custom code
    (function setupMongo() { // connect to mongo db
      MongoClient.connect(mongouri, function (err, db) {
        if (err) {
          console.dir(err);
          process.exit();
        }

        /**
         * Set up the collection and tail it.
         * 'name' is name of collection, it will be prepended with 'stream_'.
         * callback(collection) - 'collection' is collection to tail.
         */
        function startStream(name, callback) {
          var collName = 'stream_' + name;
          db.collection(collName).drop(function() {
            db.createCollection(collName, {
              capped: true,
              size: 16777216,
              max: 1024
            }, function (err, collection) {
              assert.equal(null, err);

              collection.insertOne({}, function (err, r) {
                assert.equal(null, err);
                assert.equal(1, r.insertedCount);

                callback(collection);
              });
            });
          });
        }

        /**
         * Tail a stream.
         * 'collection' is collection to tail.
         * callback(doc) - 'doc' is next doc in stream. 'doc' is null if stream was interrupted and
         * restarted.
         */
        function listen(collection, callback) {
          var query = {};
          var latestCursor = collection.find(query).sort({$natural: -1}).limit(1);
          latestCursor.nextObject(function(err, latest) {
            assert.equal(null, err);

            (function tail(doc) {
              if (doc) {
                query._id = {$gt: doc._id};
              };
              var options = {
                tailable: true,
                awaitdata: true,
                numberOfRetries: Infinity
              };

              stream = collection.find(query, options).stream();

              stream.on('data', function (newdoc) {
                callback(newdoc);
                doc = newdoc;
              });
              stream.on('end', function () {
                callback(null);
                tail(doc);
              })
            })(latest);
          });
        };

        var server = {
          db: db,
          startStream: startStream,
          listen: listen
        };

        callback(server);
      });
    })();
  })();
}