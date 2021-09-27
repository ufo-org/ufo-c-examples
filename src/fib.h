#include <stdint.h>
#include "ufo_c/target/ufo_c.h"

static int32_t fib_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes);
uint64_t *fib_new(UfoCore *ufo_system, size_t n);
void fib_free(UfoCore *ufo_system, uint64_t *ptr);