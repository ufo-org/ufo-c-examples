#include "ufo.h"

#include <ctype.h>

#include "seq.h"
#include "fib.h"
#include "bzip.h"
#include "mmap.h"
#include "postgres.h"
#include "col.h"

#include "new_york/target/nyc.h"

#include "random.h"
#include "logging.h"

void *ny_setup(Arguments *config) {
    NycCore nyc_system = nyc_new_core("/tmp/", config->high_water_mark, config->low_water_mark);
    if (nyc_is_error(&nyc_system)) {
        REPORT("Cannot create UFO core system.\n");
        exit(1);
    }
    NycCore *ptr = (NycCore *) malloc(sizeof(NycCore));
    *ptr = nyc_system;
    return ptr;
}
void ny_teardown(Arguments *config, AnySystem system) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    nyc_shutdown(*nyc_system_ptr);
    free(nyc_system_ptr);
}
size_t ny_max_length(Arguments *config, AnySystem system, AnyObject object) {
    Borough *borough = (Borough *) object;
    BoroughParameters parameters;
    borough_params(borough, &parameters);
    return parameters.element_ct;
}

// Fibonacci
void *ny_fib_creation(Arguments *config, AnySystem system) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    return (void *) nyc_fib_new(nyc_system_ptr, config->size, config->min_load);
}
void ny_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    nyc_fib_free(nyc_system_ptr, object);
}
void ny_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Borough *borough = (Borough *) object;    
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }
        if (result.write) {
            uint64_t value = (uint64_t) random_int(1000);
            borough_write(borough, result.current, &value);
        } else {
            uint64_t value;
            borough_read(borough, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// BZip
void *ny_bzip_creation(Arguments *config, AnySystem system) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    return (void *) BZip2_nyc_new(nyc_system_ptr, config->file, config->min_load);
}
void ny_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    BZip2_nyc_free(nyc_system_ptr, object);
}
void ny_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Borough *borough = (Borough *) object; 
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            char value = (char) random_int(126 - 32) + 32;
            borough_write(borough, result.current, &value);
        } else {
            char value;
            borough_read(borough, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// Seq
void *ny_seq_creation(Arguments *config, AnySystem system) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    return (void *) seq_nyc_from_length(nyc_system_ptr, 1, config->size, 2, config->min_load);
}
void ny_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    seq_nyc_free(nyc_system_ptr, object);
}
void ny_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Borough *borough = (Borough *) object;  
    int64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            int64_t value = random_int(1000);
            borough_write(borough, result.current, &value);
        } else {
            int64_t value;
            borough_read(borough, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// Psql
void *ny_psql_creation(Arguments *config, AnySystem system) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    return (void *) Players_nyc_new(nyc_system_ptr, config->min_load);
}
void ny_psql_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    Players_nyc_free(nyc_system_ptr, object);
}
void ny_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Borough *borough = (Borough *) object;  
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
            borough_read(borough, result.current, &value);
            value.tds = random_int(100);
            value.mvp = random_int(100);
            borough_write(borough, result.current, &value);
        } else {         
            Player value;               
            borough_read(borough, result.current, &value);
            tds += value.tds;
            mvp += value.mvp;
        }
    };
    *oubliette = tds + mvp;
}

// MMap
void *ny_mmap_creation(Arguments *config, AnySystem system) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    return (void *) MMap_nyc_new(nyc_system_ptr, config->file, toupper, config->min_load);
}
void ny_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NycCore *nyc_system_ptr = (NycCore *) system;
    MMap_nyc_free(nyc_system_ptr, object);
}
void ny_mmap_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Borough *borough = (Borough *) object; 
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            char value = (char) random_int(126 - 32) + 32;
            borough_write(borough, result.current, &value);
        } else {
            char value;
            borough_read(borough, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}

// Col
void *ny_col_creation(Arguments *config, AnySystem system) {
    NycCore *nyc_system = (NycCore *) system;
    int32_t **matrix = col_source_matrix_new(config->size, COL_COLUMNS_IN_EACH_ROW);
    return (void *) col_nyc_new(nyc_system, matrix, COL_SELECTED_COLUMN, config->size, config->min_load);
}
void ny_col_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NycCore *nyc_system = (NycCore *) system;
    Borough *nyc_object = (Borough *) object;

    BoroughParameters parameters;
    borough_params(nyc_object, &parameters);
    ColumnSpec *spec = (ColumnSpec *) parameters.populate_data;
    col_source_matrix_free(spec->source, spec->size);

    col_nyc_free(nyc_system, nyc_object);   
}
void ny_col_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Borough *borough = (Borough *) object; 
    int64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);     
        if (result.end) {
            break;
        }        
        if (result.write) {
            int32_t value = (int32_t) random_int(1000);
            borough_write(borough, result.current, &value);
        } else {
            int32_t value;
            borough_read(borough, result.current, &value);
            sum += value;
        }
    };
    *oubliette = sum;
}