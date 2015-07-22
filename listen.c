#include <bson.h>
#include <mongoc.h>
#include <stdio.h>

void mongoLogHandler(mongoc_log_level_t log_level,
                     const char* log_domain,
                     const char* message,
                     void* user_data) {}

int main (int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: listen mongouri tunnelname\n");
        exit(0);
    }

    const char *mongouri = argv[1];
    const char *tunnelName = argv[2];
    mongoc_uri_t *mongourit = mongoc_uri_new(mongouri);
    const char *dbName = mongoc_uri_get_database(mongourit);
    printf("dbName: %s\n", dbName);
    mongoc_uri_destroy(mongourit);

    mongoc_init();
    bson_error_t error;
    mongoc_log_set_handler(mongoLogHandler, NULL);
    mongoc_client_t *client = mongoc_client_new(mongouri);
    if (!client) {
        fprintf(stderr, "failed to parse URI\n");
        return 1;
    }

    char *collectionName = malloc(strlen(tunnelName) + 8);
    strcpy(collectionName, "stream_");
    strcat(collectionName, tunnelName);
    printf("collectionName: %s\n", collectionName);

    mongoc_collection_t *collection = mongoc_client_get_collection(client, dbName, collectionName);
    free(collectionName);

    bson_t *query = BCON_NEW(
        "$query", "{", "}",
        "$orderby", "{", "$natural", BCON_INT32(-1), "}"
    );
    mongoc_cursor_t *cursor =
        mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);

    const bson_t *doc;
    while (!mongoc_cursor_next(cursor, &doc)) {
        return main(argc, argv);
    }
    mongoc_cursor_destroy(cursor);

    bson_destroy(query);

    bson_iter_t iterDocId;
    bson_iter_init(&iterDocId, doc);
    bson_iter_find(&iterDocId, "_id");

    while (1) {
        const bson_oid_t *docid = bson_iter_oid(&iterDocId);
        bson_t *query = BCON_NEW("_id", "{", "$gt", BCON_OID(docid), "}");
        bson_t *fields = BCON_NEW("message", BCON_INT32(1));
        mongoc_query_flags_t options = MONGOC_QUERY_TAILABLE_CURSOR | MONGOC_QUERY_AWAIT_DATA;
        mongoc_cursor_t *cursor =
            mongoc_collection_find(collection, options, 0, 0, 0, query, fields, NULL);

        const bson_t *doc;
        while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &doc)) {
            bson_iter_init(&iterDocId, doc);
            bson_iter_find(&iterDocId, "_id");

            bson_iter_t iterMessage;
            bson_iter_init(&iterMessage, doc);
            bson_iter_find(&iterMessage, "message");

            const char *message = bson_iter_utf8(&iterMessage, NULL);
            printf("%s", message);
        }

        if (mongoc_cursor_error(cursor, &error)) {
            printf("Cursor error: %d %s\n", error.code, error.message);
            return 1;
        }

        mongoc_cursor_destroy(cursor);
        bson_destroy(query);
        bson_destroy(fields);
    }

    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);

    mongoc_cleanup();

    return 0;
}