#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
#include <cstddef>
#endif

#define GB (1024UL * 1024UL * 1024UL)
#define MB (1024UL * 1024UL)
#define KB (1024UL)

typedef struct {
    char *benchmark;
    char *implementation;
    char *pattern;
    char *file;
    char *timing;
    size_t size;
    size_t sample_size;
    size_t min_load;
    size_t high_water_mark;
    size_t low_water_mark; 
    size_t writes;
    unsigned int seed;
} Arguments;

typedef void *AnySystem;
typedef void *AnyObject;
typedef void *AnySequence;

typedef struct { 
    size_t current; 
    bool end; 
    bool write; 
} SequenceResult;

typedef SequenceResult (*sequence_t)(Arguments *, AnySequence);

typedef void  *(*system_setup_t)   (Arguments *);
typedef void   (*system_teardown_t)(Arguments *, AnySystem);
typedef void  *(*object_creation_t)(Arguments *, AnySystem);
typedef void   (*object_cleanup_t) (Arguments *, AnySystem, AnyObject);
typedef void   (*execution_t)      (Arguments *, AnySystem, AnyObject, AnySequence, sequence_t, volatile int64_t *oubliette);
typedef size_t (*max_length_t)     (Arguments *, AnySystem, AnyObject);

