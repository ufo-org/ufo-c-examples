#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ufo_c/target/ufo_c.h"
#include "new_york/target/nyc.h"
#include "toronto/target/toronto.h"

int32_t ufo_fib_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes);
uint64_t *ufo_fib_new(UfoCore *ufo_system, size_t n, bool read_only, size_t min_load_count);
void ufo_fib_free(UfoCore *ufo_system, uint64_t *ptr);

uint64_t *normil_fib_new(size_t n);
void normil_fib_free(uint64_t *ptr);

Borough *nyc_fib_new(NycCore *system, size_t n, size_t min_load_count);
void nyc_fib_free(NycCore *system, Borough *ptr);

Village *toronto_fib_new(TorontoCore *system, size_t n, size_t min_load_count);
void toronto_fib_free(TorontoCore *system, Village *ptr);