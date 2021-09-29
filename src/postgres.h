#include <stdint.h>
#include "ufo_c/target/ufo_c.h"

typedef struct Player {
    int    id;
    char   name[64];
    int    tds;
    double run; 
    int    mvp;
} Player;

Player *Player_new(UfoCore *ufo_system, bool read_only, size_t min_load_count);
size_t Player_count(UfoCore *ufo_system, Player *ptr);
void Player_free(UfoCore *ufo_system, Player *ptr);