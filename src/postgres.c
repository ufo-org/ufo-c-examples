#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libpq-fe.h>

#include "logging.h"
#include "postgres.h"
#include "ufo_c/target/ufo_c.h"

#define EXIT_OK 0
#define EXIT_ERROR 1

// ----------------------------------------------------------------------------

PGconn *connect_to_database(const char *connection_info) {
    PGconn *connection = PQconnectdb(connection_info);

    if (PQstatus(connection) != CONNECTION_OK) {
        REPORT("Connection to database failed: %s", PQerrorMessage(connection));
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    return connection;
}

void disconnect_from_database(PGconn *connection) {
    PQfinish(connection);
}

void start_transaction(PGconn *connection) {
    PGresult *result = PQexec(connection, "BEGIN");

    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        REPORT("BEGIN command failed: %s", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    PQclear(result);
}

void end_transaction(PGconn *connection) {
    PGresult *result = PQexec(connection, "END");

    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        REPORT("END command failed: %s", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    PQclear(result);
}

// ----------------------------------------------------------------------------

void retrieve_from_table(PGconn *connection, uintptr_t start, uintptr_t end, Player *output) {

    char query[128];
    sprintf(query, "SELECT id, name, tds, run, mvp FROM league "
                   "WHERE id >= %li AND id < %li", start + 1, end + 1); // 1-indexed
    LOG("Executing %s\n", query);
    PGresult *result = PQexec(connection, query);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        REPORT("SELECT command failed: %s\n", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    int retrieved_rows = PQntuples(result);

    for (int i = 0; i < retrieved_rows; i++) {          
        char *id   = PQgetvalue(result, i, 0);
        char *name = PQgetvalue(result, i, 1);
        char *tds  = PQgetvalue(result, i, 2);
        char *run  = PQgetvalue(result, i, 3);
        char *mvp  = PQgetvalue(result, i, 4);

        char *remainder;

        // Convert types and write out to output
        Player *player = &output[i];
        player->id = atoi(id);
        strcpy(player->name, name);
        player->tds = atoi(tds);
        player->run = strtod(run, &remainder);
        player->mvp = atoi(mvp);

        LOG("Loading object id=%-6d name=%-64s tds=%-2d run=%-6.1f mvp=%-2d\n",  
            player->id, player->name, player->tds, player->run, player->mvp);
    }    

    PQclear(result);
}

size_t retrieve_size_of_table(PGconn *connection) {

    const char * query = "SELECT count(*) FROM league";
    LOG("Executing %s\n", query);
    PGresult *result = PQexec(connection, query);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        REPORT("SELECT count(..) command failed: %s\n", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    char *count_string = PQgetvalue(result, 0, 0);
    size_t count = atol(count_string);

    PQclear(result);
    return count;
}

// ----------------------------------------------------------------------------

typedef struct Data {
    PGconn *database;
    pthread_mutex_t lock;
} Data;

int32_t Player_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    Player *objects = (Player *) target;
    Data *data = (Data *) user_data;
    
    pthread_mutex_lock(&data->lock);

    start_transaction(data->database);
    retrieve_from_table(data->database, start, end, objects);
    end_transaction(data->database);

    pthread_mutex_unlock(&data->lock);

    return 0;
}

size_t Player_ufo_count(UfoCore *ufo_system, Player *ptr) { // TODO use the data already given to the UFO object
    // Retrieve UFO object from pointer.
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        REPORT("Cannot execute count: %p not a UFO object. Returning "
               "size = 0.\n", ptr);
        return 0;
    }    
    
    // Grab the parameters to retrieve the length.
    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        REPORT("Cannot execute count: unable to access UFO "
               "parameters for %p. Returning size = 0.\n", ptr);
        ufo_free(ufo_object);
        return 0;
    }

    // Get the count from aprameters
    return parameters.element_ct;
}

Players *Players_ufo_new(UfoCore *ufo_system, bool read_only, size_t min_load_count) {
    Data *data = (Data *) malloc(sizeof(Data));

    // Connect to database
    data->database = connect_to_database("dbname = ufo"); // FIXME parameterize connection
    if (NULL == data->database) {
        fprintf(stderr, "Cannot connect to database.\n");
        return NULL;
    }
    
    // Get table length
    size_t length = retrieve_size_of_table(data->database);

    // Create a mutex for controling access to database
    if (0 != pthread_mutex_init(&data->lock, NULL)) {
        fprintf(stderr, "Cannot create UFO object query lock.\n");
        return NULL;
    }

    // Create UFO
    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(Player);
    parameters.element_ct = length;
    parameters.min_load_ct = min_load_count;
    parameters.read_only = read_only;
    parameters.populate_data = data;
    parameters.populate_fn = Player_populate;
    
    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);

    // Check if UFO core returns an error of its own.
    if (ufo_is_error(&ufo_object)) {
        REPORT("Cannot create UFO object.\n");
        return NULL;
    }

    // Return pointer to beginning of object inside UFO
    Players *players = (Players *) malloc(sizeof(Players));
    players->data = (Player *) ufo_header_ptr(&ufo_object);
    players->size = length;
    return players;
}

void Players_ufo_free(UfoCore *ufo_system, Players *players) {
    // Retrieve UFO object from pointer.
    UfoObj ufo_object = ufo_get_by_address(ufo_system, players->data);
    if (ufo_is_error(&ufo_object)) {
        REPORT("Cannot free %p: not a UFO object.\n", players);
        return;
    }    
    
    // Retrieve the parameters to finalize all the user objects within.
    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        REPORT("Cannot free %p: unable to access UFO parameters: "
               "Freeing object, but not closing DB connection and not freeing "
               "mutex.\n", players);
        ufo_free(ufo_object);
        return;
    }
    Data *data = (Data *) parameters.populate_data;

    // Kill the DB connection
    disconnect_from_database(data->database);

    // Kill the mutex
    result = pthread_mutex_destroy(&data->lock);
    if (result < 0) {
        REPORT("Cannot free %p: unable to access UFO parameters: "
               "Freeing object and closing DB connection, but not freeing "
               "mutex.\n", players);
        //Do not exit.
    }
    
    // Free the actual object
    ufo_free(ufo_object);

    // Free the wrapper
    free(players);
}

Players *Players_normil_new() {
    // Connect to database
    PGconn *connection = connect_to_database("dbname = ufo"); // FIXME parameterize connection
    if (NULL == connection) {
        fprintf(stderr, "Cannot connect to database.\n");
        return NULL;
    }

    char query[128];
    sprintf(query, "SELECT id, name, tds, run, mvp FROM league"); // 1-indexed
    LOG("Executing %s\n", query);
    PGresult *result = PQexec(connection, query);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        REPORT("SELECT command failed: %s\n", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    int retrieved_rows = PQntuples(result);
    Players *players = (Players *) malloc(sizeof(Players));
    players->size = retrieved_rows;
    players->data = (Player *) malloc(sizeof(Player) * players->size);

    for (int i = 0; i < retrieved_rows; i++) {          
        char *id   = PQgetvalue(result, i, 0);
        char *name = PQgetvalue(result, i, 1);
        char *tds  = PQgetvalue(result, i, 2);
        char *run  = PQgetvalue(result, i, 3);
        char *mvp  = PQgetvalue(result, i, 4);

        char *remainder;

        // Convert types and write out to output
        Player *player = &players->data[i];
        player->id = atoi(id);
        strcpy(player->name, name);
        player->tds = atoi(tds);
        player->run = strtod(run, &remainder);
        player->mvp = atoi(mvp);

        LOG("Loading object id=%-6d name=%-64s tds=%-2d run=%-6.1f mvp=%-2d\n",  
            player->id, player->name, player->tds, player->run, player->mvp);
    }

    PQclear(result);    

    disconnect_from_database(connection);
    return players;
}

void Players_normil_free(Players *players) {
    free(players->data);
    free(players);
}
