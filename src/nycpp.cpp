#include "nycpp.h"
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "seq.h"
#include "fib.h"
#include "bzip.h"
#include "postgres.h"
#include "random.h"

#ifdef __cplusplus
}
#endif

void *nycpp_setup(Arguments *config) {
    return NULL;
}

void nycpp_teardown(Arguments *config, AnySystem system) {
    /* empty */
}

// Seq
void *nycpp_seq_creation(Arguments *config, AnySystem system) {   
    int64_t *sequence = seq_normil_from_length(1, config->size, 2);    
    NYCpp<int64_t> *nycpp = new NYCpp<int64_t>(config->size, sequence);
    return (void *) nycpp;
}

void nycpp_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NYCpp<int64_t> *nycpp = (NYCpp<int64_t> *) object;
    delete nycpp;
}

void nycpp_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    int64_t *data = (int64_t *) object;    
    int64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            data[result.current] = random_int(1000);
        } else {
            sum += data[result.current];
        }
    };
    *oubliette = sum;
}

// Fib
void *nycpp_fib_creation(Arguments *config, AnySystem system) {
    uint64_t *fib= normil_fib_new(config->size);    
    NYCpp<uint64_t> *nycpp = new NYCpp<uint64_t>(config->size, fib);
    return (void *) nycpp;
}
void nycpp_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NYCpp<uint64_t> *nycpp = (NYCpp<uint64_t> *) object;
    delete nycpp;
}
void nycpp_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    uint64_t *data = (uint64_t *) object;    
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }
        if (result.write) {
            data[result.current] = random_int(1000);
        } else {
            sum += data[result.current];
        }
    };
    *oubliette = sum;
}

// BZip
void *nycpp_bzip_creation(Arguments *config, AnySystem system) {
    BZip2 *bzip = BZip2_normil_new(config->file);  
    NYCpp<char> *nycpp = new NYCpp<char>(bzip->size, bzip->data);
    free(bzip);
    return (void *) nycpp;
}
void nycpp_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NYCpp<char> *nycpp = (NYCpp<char> *) object;
    delete nycpp;
}
void nycpp_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    uint64_t *data = (uint64_t *) object;    
    uint64_t sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            data[result.current] = random_int(126 - 32) + 32;
        } else {
            sum += data[result.current];
        }
    };
    *oubliette = sum;
}

// PSQL
void *nycpp_psql_creation(Arguments *config, AnySystem system) {
    Players *players = Players_normil_new();   
    NYCpp<Player> *nycpp = new NYCpp<Player>(players->size, players->data);
    free(players);
    return (void *) nycpp;
}
void nycpp_psql_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    NYCpp<Player> *nycpp = (NYCpp<Player> *) object;
    delete nycpp;
}
void nycpp_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    Players *players = (Players *) object;    
    int64_t tds = 0;
    int64_t mvp = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            break;
        }        
        if (result.write) {
            players->data[result.current].tds = random_int(100);
            players->data[result.current].mvp = random_int(100);
        } else {
            tds += players->data[result.current].tds;
            mvp += players->data[result.current].mvp;
        }
    };
    *oubliette = tds + mvp;
}
