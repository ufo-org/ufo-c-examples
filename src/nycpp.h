#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "bench.h"
#include "postgres.h"

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <memory>
#include <stdlib.h>

template< typename T >
class NYCpp {
    public:
    NYCpp(size_t n, T* data): size(n), inner(data) {}
    ~NYCpp() { free(inner); }
    T operator [](int i) const {
        return inner[i];
    }
    T& operator [](int i) {
        return inner[i];
    }
    private:
    size_t size;
    T *inner;    
};


#endif

#ifdef __cplusplus
extern "C" {
#endif

void *nycpp_setup         (Arguments *config);
void  nycpp_teardown      (Arguments *config, AnySystem system);

void *nycpp_seq_creation  (Arguments *config, AnySystem system);
void *nycpp_fib_creation  (Arguments *config, AnySystem system);
void *nycpp_bzip_creation (Arguments *config, AnySystem system);
void *nycpp_seq_creation  (Arguments *config, AnySystem system);
void *nycpp_psql_creation (Arguments *config, AnySystem system);
void *nycpp_mmap_creation (Arguments *config, AnySystem system);
void *nycpp_col_creation  (Arguments *config, AnySystem system);

void  nycpp_seq_cleanup   (Arguments *config, AnySystem system, AnyObject object);
void  nycpp_fib_cleanup   (Arguments *config, AnySystem system, AnyObject object); 
void  nycpp_bzip_cleanup  (Arguments *config, AnySystem system, AnyObject object);
void  nycpp_seq_cleanup   (Arguments *config, AnySystem system, AnyObject object);
void  nycpp_psql_cleanup  (Arguments *config, AnySystem system, AnyObject object);
void  nycpp_mmap_cleanup  (Arguments *config, AnySystem system, AnyObject object);
void  nycpp_col_cleanup   (Arguments *config, AnySystem system, AnyObject object);

void  nycpp_seq_execution (Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void  nycpp_fib_execution (Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void  nycpp_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void  nycpp_seq_execution (Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void  nycpp_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void  nycpp_mmap_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void  nycpp_col_execution (Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);

#ifdef __cplusplus
}
#endif