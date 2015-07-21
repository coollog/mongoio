#include <bson.h>
#include <mongoc.h>
#include <stdio.h>

void mongoLogHandler(mongoc_log_level_t log_level,
                     const char* log_domain,
                     const char* message,
                     void* user_data) {}

void gen_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

int main (int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage: dig host[:port] db tunnelname\n");
        exit(0);
    }

    const char *hostAndPort = argv[1];
    const char *dbName = argv[2];
    const char *tunnelName = argv[3];

    char mongouri[strlen(hostAndPort) + strlen(dbName) + 12];
    strcpy(mongouri, "mongodb://");
    strcat(mongouri, hostAndPort);
    strcat(mongouri, "/");
    strcat(mongouri, dbName);

    printf("digging tunnel...\n");

    mongoc_init();
    bson_error_t error;
    mongoc_log_set_handler(mongoLogHandler, NULL);
    mongoc_client_t *client = mongoc_client_new(mongouri);
    mongoc_database_t *database = mongoc_client_get_database(client, dbName);
    if (!client) {
        fprintf(stderr, "failed to parse URI\n");
        return 1;
    }

    char collectionName[strlen(tunnelName) + 8];
    strcpy(collectionName, "stream_");
    strcat(collectionName, tunnelName);

    mongoc_collection_t *collection = mongoc_client_get_collection(client, dbName, collectionName);
    mongoc_collection_drop(collection, NULL);

    // Make the capped collection.
    bson_t *command = BCON_NEW("create", BCON_UTF8(collectionName),
                               "capped", BCON_BOOL(true),
                               "size", BCON_INT32(16777216),
                               "max", BCON_INT32(1024));

    if (!mongoc_client_command_simple(client, dbName, command, NULL, NULL, &error)) {
        printf ("Create capped collection error: %s\n", error.message);
        return 1;
    }

    bson_t *emptyDoc = bson_new();
    if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, emptyDoc, NULL, &error)) {
        printf("Insert error: %s\n", error.message);
        return 1;
    }

    printf("DUG TUNNEL %s\n", tunnelName);
    printf("opening holes...\n");

    char roleNameWrite[strlen(tunnelName) + 11];
    strcpy(roleNameWrite, "writerole_");
    strcat(roleNameWrite, tunnelName);
    const char* roleActionWrite = "insert";
    char roleUsernameWrite[strlen(tunnelName) + 11];
    strcpy(roleUsernameWrite, "writeuser_");
    strcat(roleUsernameWrite, tunnelName);
    char rolePasswordWrite[21];
    gen_random(rolePasswordWrite, 20);

    char roleNameRead[strlen(tunnelName) + 11];
    strcpy(roleNameRead, "readrole_");
    strcat(roleNameRead, tunnelName);
    const char* roleActionRead = "find";
    char roleUsernameRead[strlen(tunnelName) + 11];
    strcpy(roleUsernameRead, "readuser_");
    strcat(roleUsernameRead, tunnelName);
    char rolePasswordRead[21];
    gen_random(rolePasswordRead, 20);

    bson_t *createRoleCommandWrite =
        BCON_NEW("createRole", BCON_UTF8(roleNameWrite),
                 "privileges", "[", "{",
                    "resource", "{",
                        "db", BCON_UTF8(dbName),
                        "collection", BCON_UTF8(collectionName),
                    "}",
                    "actions", "[", BCON_UTF8(roleActionWrite), "]",
                 "}", "]",
                 "roles", "[", "]");
    bson_t *createRoleCommandRead =
        BCON_NEW("createRole", BCON_UTF8(roleNameRead),
                 "privileges", "[", "{",
                    "resource", "{",
                        "db", BCON_UTF8(dbName),
                        "collection", BCON_UTF8(collectionName),
                    "}",
                    "actions", "[", BCON_UTF8(roleActionRead), "]",
                 "}", "]",
                 "roles", "[", "]");

    mongoc_client_command_simple(client, dbName, createRoleCommandWrite, NULL, NULL, NULL);
    mongoc_client_command_simple(client, dbName, createRoleCommandRead, NULL, NULL, NULL);

    bson_t *rolesWrite =
        BCON_NEW("0", "{",
            "role", BCON_UTF8(roleNameWrite),
            "db", BCON_UTF8(dbName),
        "}");
    bson_t *rolesRead =
        BCON_NEW("0", "{",
            "role", BCON_UTF8(roleNameRead),
            "db", BCON_UTF8(dbName),
        "}");

    mongoc_database_remove_user(database, roleUsernameRead, NULL);
    mongoc_database_remove_user(database, roleUsernameWrite, NULL);

    if (!mongoc_database_add_user(database,
                                  roleUsernameWrite,
                                  rolePasswordWrite,
                                  rolesWrite,
                                  NULL,
                                  &error)) {
        printf ("Create user error: %s\n", error.message);
        return 1;
    }
    if (!mongoc_database_add_user(database,
                                  roleUsernameRead,
                                  rolePasswordRead,
                                  rolesRead,
                                  NULL,
                                  &error)) {
        printf ("Create user error: %s\n", error.message);
        return 1;
    }

    printf("write hole:\n");
    printf("mongodb://%s:%s@%s/%s\n", roleUsernameWrite, rolePasswordWrite, hostAndPort, dbName);

    printf("read hole:\n");
    printf("mongodb://%s:%s@%s/%s\n", roleUsernameRead, rolePasswordRead, hostAndPort, dbName);

    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_database_destroy(database);

    mongoc_cleanup();

    return 0;
}