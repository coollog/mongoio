var assert = require('assert');
var randomstring = require('randomstring');

assert.equal(5, process.argv.length, 'usage: dig host[:port] db tunnelname');

var hostAndPort = process.argv[2];
var dbName = process.argv[3];
var tunnelName = process.argv[4];

var mongouri = 'mongodb://' + hostAndPort + '/' + dbName;

console.log('digging tunnel...');

require('./io')(mongouri, function (server) {
  var collectionName = 'stream_' + tunnelName;

  // Create the stream and set up the users. Requires admin access.
  server.startStream(tunnelName, function (collection) {
    console.log('DUG TUNNEL ' + tunnelName);
    console.log('opening holes...');

    // Create users to read/write to this stream.
    var roles = [
      {
        type: 'write',
        name: 'writerole_' + tunnelName,
        username: 'writeuser_' + tunnelName,
        actions: ['insert']
      },
      {
        type: 'read',
        name: 'readrole_' + tunnelName,
        username: 'readuser_' + tunnelName,
        actions: ['find']
      }
    ];

    roles.forEach(function (role, index) {
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
      }, function (err) {
        var username = role.username;
        var password = randomstring.generate();
        var myRole = {
          role: role.name,
          db: dbName
        };
        server.db.removeUser(username, function() {
          server.db.addUser(username, password, {roles: [myRole]}, function (err, res) {
            assert.equal(null, err);

            console.log(role.type + ' hole');
            console.log('mongodb://' + username + ':' + password + '@' + hostAndPort + '/' + dbName);

            if (index == 1) {
              process.exit(0);
            }
          });
        });
      });
    });
  });
});