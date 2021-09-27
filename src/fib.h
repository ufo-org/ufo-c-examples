#include <stdint.h>
#include "ufo_c/target/ufo_c.h"

int32_t ufo_fib_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes);
uint64_t *ufo_fib_new(UfoCore *ufo_system, size_t n);
void ufo_fib_free(UfoCore *ufo_system, uint64_t *ptr);

uint64_t *normil_fib_new(size_t n);
void normil_fib_free(uint64_t *ptr);