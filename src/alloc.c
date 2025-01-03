#include "alloc.h"

#include <stdio.h>
#include <stdlib.h>

static void report(const size_t required_size) {
  fprintf(stderr, "Error: could not allocate %zu bytes of memory\n",
          required_size);
  exit(1);
}

void* mallocChecked(const size_t size) {
  void* result = malloc(size);
  if (!result) {
    report(size);
  }
  return result;
}

void* callocChecked(const size_t block_count, const size_t block_size) {
  void* result = calloc(block_count, block_size);
  if (!result) {
    report(block_count * block_size);
  }
  return result;
}

void* reallocChecked(void* ptr, const size_t size) {
  void* result = realloc(ptr, size);
  if (!result) {
    report(size);
  }
  return result;
}
