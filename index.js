var assert = require('assert');
var prompt = require('prompt');
var randomstring = require('randomstring');
var mongodbUri = require('mongodb-uri');

prompt.start();
prompt.get({
  properties: {
    mongouri: {},
    name: {message: 'stream name'}
  }
}, function (err, res) {
  assert.equal(null, err);

  require('./io')(res.mongouri, function (server) {
    var collectionName = 'stream_' + res.name;
    var collection = server.db.collection(collectionName);

    var uri = mongodbUri.parse(res.mongouri);
    var dbName = uri.database;

    if (uri.username) {
      if (uri.username.match(/^writeuser/)) {
        // This is for writing.
        (function sendMessage() {
          prompt.get(['message'], function (err, res) {
            assert.equal(null, err);

            collection.insertOne({message: res.message}, function() {
              sendMessage();
            });
          });
        })();
      } else if (uri.username.match(/^readuser/)) {
        // This is for reading.
        server.listen(collection, function (doc) {
          if (!doc) {
            console.log('stream restarted');
          }
          console.log('received: ' + doc.message);
        });
      } else {
        assert(false, 'invalid username')
      }
    } else {
      // Create the stream and set up the users. Requires admin access.
      server.startStream(res.name, function (collection) {
        console.log('stream started');

        // Create users to read/write to this stream.
        var roles = [
          {
            type: 'write',
            name: 'writerole_' + res.name,
            username: 'writeuser_' + res.name,
            actions: ['insert']
          },
          {
            type: 'read',
            name: 'readrole_' + res.name,
            username: 'readuser_' + res.name,
            actions: ['find']
          }
        ];

        roles.forEach(function (role) {
          server.db.command({
            createRole: role.name,
            privileges: [
              {
                resource: {
                  db: dbName,
                  collection: collectionName,
                },
                actions: role.actions
              }
            ],
            roles: []
          }, function () {
            var username = role.username;
            var password = randomstring.generate();
            var myRole = {
              role: role.name,
              db: dbName
            };
            server.db.removeUser(username, function() {
              server.db.addUser(username, password, {roles: [myRole]}, function (err, res) {
                assert.equal(null, err);

                console.log('user created for ' + role.type + ' stream');
                var mongouri = 'mongodb://' + username + ':' + password + '@localhost/' + dbName;
                console.log('mongouri: ' + mongouri);
              });
            });
          });
        });
      });
    }
  });
});