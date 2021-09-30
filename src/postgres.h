#pragma once
#include <stdint.h>
#include "ufo_c/target/ufo_c.h"
#include "new_york/target/nyc.h"

typedef struct Player {
    int    id;
    char   name[64];
    int    tds;
    double run; 
    int    mvp;
} Player;

typedef struct {
    size_t size;
    Player *data;
} Players;

Players *Players_ufo_new(UfoCore *ufo_system, bool read_only, size_t min_load_count);
void Players_ufo_free(UfoCore *ufo_system, Players *players);

Players *Players_normil_new();
void Players_normil_free(Players *players);

Borough *Players_nyc_new(NycCore *system, size_t min_load_count);
void Players_nyc_free(NycCore *system, Borough *players);