#include <bson.h>
#include <mongoc.h>
#include <stdio.h>

void mongoLogHandler(mongoc_log_level_t log_level,
                     const char* log_domain,
                     const char* message,
                     void* user_data) {}

char *getLine(FILE *fp) {
    char *line;                 // Line being read
    int size;                   // #chars allocated
    int c, i;

    size = sizeof(double);                      // Minimum allocation
    line = malloc (size);
    for (i = 0;  (c = getc(fp)) != EOF; )  {
    if (i == size-1) {
        size *= 2;                          // Double allocation
        line = realloc (line, size);
    }
    line[i++] = c;
    if (c == '\n')                          // Break on newline
        break;
    }

    if (c == EOF && i == 0)  {                  // Check for immediate EOF
    free (line);
    return NULL;
    }

    line[i++] = '\0';                           // Terminate line
    line = realloc (line, i);                   // Trim excess storage

    return (line);
}

int main (int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: yell mongouri tunnelname\n");
        exit(0);
    }

    const char *mongouri = argv[1];
    const char *tunnelName = argv[2];
    mongoc_uri_t *mongourit = mongoc_uri_new(mongouri);
    const char *dbName = mongoc_uri_get_database(mongourit);
    mongoc_uri_destroy(mongourit);

    mongoc_init();
    bson_error_t error;
    mongoc_log_set_handler(mongoLogHandler, NULL);
    mongoc_client_t *client = mongoc_client_new(mongouri);
    if (!client) {
        fprintf(stderr, "failed to parse URI\n");
        return 1;
    }

    char collectionName[strlen(tunnelName) + 8];
    strcpy(collectionName, "stream_");
    strcat(collectionName, tunnelName);

    mongoc_collection_t *collection = mongoc_client_get_collection(client, dbName, collectionName);

    printf("YELLING INTO TUNNEL %s\n", tunnelName);

    char *line;
    while ((line = getLine(stdin)) != NULL) {
        bson_t *message = BCON_NEW("message", BCON_UTF8(line));
        if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, message, NULL, &error)) {
            printf("Insert error: %s\n", error.message);
            return 1;
        }
        printf("");
        bson_destroy(message);
        free(line);
    }

    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);

    mongoc_cleanup();

    return 0;
}