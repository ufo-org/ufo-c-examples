#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libpq-fe.h>
#include "ufo_c/target/ufo_c.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

#define EXIT_OK 0
#define EXIT_ERROR 1

// ----------------------------------------------------------------------------

PGconn *connect_to_database(const char *connection_info) {
    PGconn *connection = PQconnectdb(connection_info);

    if (PQstatus(connection) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(connection));
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
        fprintf(stderr, "BEGIN command failed: %s", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    PQclear(result);
}

void end_transaction(PGconn *connection) {
    PGresult *result = PQexec(connection, "END");

    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "END command failed: %s", PQerrorMessage(connection));
        PQclear(result);
        PQfinish(connection);
        exit(EXIT_ERROR);
    }

    PQclear(result);
}

// ----------------------------------------------------------------------------

typedef struct Player {
    int    id;
    char   name[64];
    int    tds;
    double run; 
    int    mvp;
} Player;

void retrieve_from_table(PGconn *connection, uintptr_t start, uintptr_t end, Player *output) {

    char query[128];
    sprintf(query, "SELECT id, name, tds, run, mvp FROM league "
                   "WHERE id >= %li AND id < %li", start + 1, end + 1); // 1-indexed
    fprintf(stderr, "DEBUG: Executing %s", query);
    PGresult *result = PQexec(connection, query);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        printf("SELECT command failed: %s\n", PQerrorMessage(connection));
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

        fprintf(stderr, "DEBUG: Loading object "
                "id=%-6d name=%-64s tds=%-2d run=%-6.1f mvp=%-2d\n",  
                player->id, player->name, player->tds, player->run, player->mvp);
    }    

    PQclear(result);
}

size_t retrieve_size_of_table(PGconn *connection) {

    const char * query = "SELECT count(*) FROM league";
    fprintf(stderr, "DEBUG: Executing %s", query);
    PGresult *result = PQexec(connection, query);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        printf("SELECT count(..) command failed: %s\n", PQerrorMessage(connection));
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

size_t Player_count(UfoCore *ufo_system, Player *ptr) { // TODO use the data already given to the UFO object
    PGconn *database = connect_to_database("dbname = ufo");
    size_t count = retrieve_size_of_table(database);
    disconnect_from_database(database);
    return count;
}

Player *Player_new(UfoCore *ufo_system) {
    Data *data = (Data *) malloc(sizeof(Data));

    // Connect to database
    data->database = connect_to_database("dbname = ufo");
    if (NULL == data->database) {
        fprintf(stderr, "Cannot connect to database");
        return NULL;
    }
    
    // Create a mutex for controling access to database
    if (0 != pthread_mutex_init(&data->lock, NULL)) {
        fprintf(stderr, "Cannot create UFO object query lock");
        return NULL;
    }

    // Create UFO
    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(Player);
    parameters.element_ct = retrieve_size_of_table(data->database);
    parameters.min_load_ct = MIN_LOAD_COUNT;
    parameters.read_only = true;
    parameters.populate_data = data;
    parameters.populate_fn = Player_populate;
    
    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);

    // Check if UFO core returns an error of its own.
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object");
        return NULL;
    }

    // Return pointer to beginning of object inside UFO
    return (Player *) ufo_header_ptr(&ufo_object);
}

void Player_free(UfoCore *ufo_system, Player *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.", ptr); // TODO
        return;
    }
    ufo_free(ufo_object);    
    // FIXME disconnect from DB disconnect_from_database(PGconn *connection)
    // FIXME pthread_mutex_destroy(&data->lock);
    // FIXME free Data object
}

// ----------------------------------------------------------------------------

int main(int argc, char **argv) {
    if (PQisthreadsafe() != 1) { 
        fprintf(stderr, "Postgres is not thread safe.\n");
        exit(1);
    }

    // Initialize the UFO system
    UfoCore ufo_system = ufo_new_core("/tmp/ufos/", HIGH_WATER_MARK, LOW_WATER_MARK);    

    // Create a UFO object
    Player *players = Player_new(&ufo_system);
    size_t player_count = Player_count(&ufo_system, players);

    // Set up random sample;
    srand(42);
    size_t sample_size = 10;
    
    // Use the UFO object: select 10 random objects
    for (size_t i = 0; i < sample_size; i++) {
        int index = rand() % player_count;
        Player player = players[index];
        printf("id=%-6d name=%-64s tds=%-2d run=%-6.1f mvp=%-2d\n",  
               player.id, player.name, player.tds, player.run, player.mvp);
    }

    // Release the UFO object
    Player_free(&ufo_system, players);

    // Close down the UFO system before exiting
    ufo_core_shutdown(ufo_system);
}