#pragma once

#include <stdlib.h>

void* mallocChecked(size_t size);
void* callocChecked(size_t n, size_t elem_size);
void* reallocChecked(void* ptr, size_t size);
