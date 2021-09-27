#include "postgres.h"
#include "ufo_c/target/ufo_c.h"
#include <pthread.h>
#include <libpq-fe.h>

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

// ----------------------------------------------------------------------------

int main(int argc, char **argv) {
    if (PQisthreadsafe() != 1) { 
        fprintf(stderr, "Postgres is not thread safe.\n");
        exit(1);
    }

    // Initialize the UFO system
    UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);    
    if (ufo_core_is_error(&ufo_system)) {
        exit(1);
    }

    // Create a UFO object
    Player *players = Player_new(&ufo_system);
    if (players == NULL) {
        exit(2);
    }

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