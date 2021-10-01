#include "ufo.h"

#include <ctype.h>

#include "seq.h"
#include "fib.h"
#include "bzip.h"
#include "mmap.h"
#include "postgres.h"
#include "toronto/target/toronto.h"

#include "random.h"
#include "logging.h"

void *toronto_setup(Arguments *config) {
    TorontoCore toronto_system = toronto_new_core("/tmp/", config->high_water_mark, config->low_water_mark);
    if (toronto_is_error(&toronto_system)) {
        REPORT("Cannot create UFO core system.\n");
        exit(1);
    }
    TorontoCore *ptr = (TorontoCore *) malloc(sizeof(TorontoCore));
    *ptr = toronto_system;
    return ptr;
}
void toronto_teardown(Arguments *config, AnySystem system) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    toronto_shutdown(*toronto_system_ptr);
    free(toronto_system_ptr);
}
size_t toronto_max_length(Arguments *config, AnySystem system, AnyObject object) {
    Village *village = (Village *) object;
    VillageParameters parameters;
    village_params(village, &parameters);
    return parameters.element_ct;
}

// Fibonacci
void *toronto_fib_creation(Arguments *config, AnySystem system) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    return (void *) toronto_fib_new(toronto_system_ptr, config->size, config->min_load);
}
void toronto_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    toronto_fib_free(toronto_system_ptr, object);
}
void toronto_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Village *village = (Village *) object;    
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }
        if (result.write) {
            uint64_t value = (uint64_t) random_int(1000);
            village_write(village, result.current, &value);
        } else {
            uint64_t value;
            village_read(village, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// BZip
void *toronto_bzip_creation(Arguments *config, AnySystem system) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    return (void *) BZip2_toronto_new(toronto_system_ptr, config->file, config->min_load);
}
void toronto_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    BZip2_toronto_free(toronto_system_ptr, object);
}
void toronto_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Village *village = (Village *) object; 
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            char value = (char) random_int(126 - 32) + 32;
            village_write(village, result.current, &value);
        } else {
            char value;
            village_read(village, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// Seq
void *toronto_seq_creation(Arguments *config, AnySystem system) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    return (void *) seq_toronto_from_length(toronto_system_ptr, 1, config->size, 2, config->min_load);
}
void toronto_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    seq_toronto_free(toronto_system_ptr, object);
}
void toronto_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Village *village = (Village *) object;  
    int64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            int64_t value = random_int(1000);
            village_write(village, result.current, &value);
        } else {
            int64_t value;
            village_read(village, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// Psql
void *toronto_psql_creation(Arguments *config, AnySystem system) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    return (void *) Players_toronto_new(toronto_system_ptr, config->min_load);
}
void toronto_psql_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    Players_toronto_free(toronto_system_ptr, object);
}
void toronto_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Village *village = (Village *) object;  
    int64_t tds = 0;
    int64_t mvp = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            Player value;
            village_read(village, result.current, &value);
            value.tds = random_int(100);
            value.mvp = random_int(100);
            village_write(village, result.current, &value);
        } else {         
            Player value;               
            village_read(village, result.current, &value);
            tds += value.tds;
            mvp += value.mvp;
        }
    };
    *oubliette = tds + mvp;
}

// MMap
void *toronto_mmap_creation(Arguments *config, AnySystem system) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    return (void *) MMap_toronto_new(toronto_system_ptr, config->file, toupper, config->min_load);
}
void toronto_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    TorontoCore *toronto_system_ptr = (TorontoCore *) system;
    MMap_toronto_free(toronto_system_ptr, object);
}
void toronto_mmap_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Village *village = (Village *) object; 
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            char value = (char) random_int(126 - 32) + 32;
            village_write(village, result.current, &value);
        } else {
            char value;
            village_read(village, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}
