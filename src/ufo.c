#include "ufo.h"

#include <ctype.h>

#include "seq.h"
#include "fib.h"
#include "bzip.h"
#include "mmap.h"
#include "postgres.h"

#include "logging.h"

void *ufo_setup(Arguments *config) {
    UfoCore ufo_system = ufo_new_core("/tmp/", config->high_water_mark, config->low_water_mark);
    if (ufo_core_is_error(&ufo_system)) {
        REPORT("Cannot create UFO core system.\n");
        exit(1);
    }
    UfoCore *ptr = (UfoCore *) malloc(sizeof(UfoCore));
    *ptr = ufo_system;
    return ptr;
}

void ufo_teardown(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    ufo_core_shutdown(*ufo_system_ptr);
    free(ufo_system_ptr);
}

// Fib
void *ufo_fib_creation(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    return (void *) ufo_fib_new(ufo_system_ptr, config->size, config->writes == 0, config->min_load);
}

void ufo_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    ufo_fib_free(ufo_system_ptr, object);
}

// BZip
void *ufo_bzip_creation(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    return (void *) BZip2_ufo_new(ufo_system_ptr, config->file, config->writes == 0, config->min_load);
}

void ufo_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    BZip2_ufo_free(ufo_system_ptr, object);
}

// Seq
void *ufo_seq_creation(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    return (void *) seq_ufo_from_length(ufo_system_ptr, 1, config->size, 2, config->writes == 0, config->min_load);
}
void ufo_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    seq_ufo_free(ufo_system_ptr, object);
}

// Psql
void *ufo_psql_creation(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    return (void *) Players_ufo_new(ufo_system_ptr, config->writes == 0, config->min_load);
}
void ufo_psql_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    Players_ufo_free(ufo_system_ptr, object);
}

// MMap
void *ufo_mmap_creation(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    return (void *) MMap_ufo_new(ufo_system_ptr, config->file,  toupper, config->writes == 0, config->min_load);
}
void ufo_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    MMap_ufo_free(ufo_system_ptr, object);
}