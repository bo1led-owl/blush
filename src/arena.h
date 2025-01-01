#pragma once

#include <stddef.h>

#define ARENA_BUF_SIZE 1024
#define ARENA_SIZE_TO_RETAIN 1024

typedef struct ArenaBuffer {
    struct ArenaBuffer* next;
    char* unallocated;
} ArenaBuffer;

typedef struct {
    ArenaBuffer* head;
    void* large_head;
} ArenaAllocator;

void ArenaAllocator_init(ArenaAllocator* self);
void ArenaAllocator_deinit(ArenaAllocator* self);
char* ArenaAllocator_alloc(ArenaAllocator* self, size_t size);
void ArenaAllocator_reset(ArenaAllocator* self);
