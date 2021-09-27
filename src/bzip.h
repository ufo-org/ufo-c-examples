#include <stdint.h>

#include "ufo_c/target/ufo_c.h"

#include <bzlib.h>

typedef struct {
    size_t size;
    char *data;
} BZip2;

BZip2 *BZip2_new(UfoCore *ufo_system, char *filename);
void BZip2_free(UfoCore *ufo_system, BZip2 *object);