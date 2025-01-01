#include "arena.h"

#include <assert.h>
#include <stdlib.h>

void ArenaAllocator_init(ArenaAllocator* self) {
    assert(self);
    self->head = NULL;
}

void ArenaAllocator_deinit(ArenaAllocator* self) {
    assert(self);
    ArenaBuffer* p = self->head;
    while (p) {
        void* to_free = p;
        p = p->next;
        free(to_free);
    }
}

static size_t getBufferSizeLeft(const ArenaBuffer* buf) {
    assert(buf);
    assert(buf->unallocated > (const char*)buf);
    return ARENA_BUF_SIZE - (size_t)(buf->unallocated - ((const char*)buf + sizeof(ArenaBuffer)));
}

static ArenaBuffer* newBuffer(ArenaBuffer* next) {
    ArenaBuffer* result = malloc(sizeof(ArenaBuffer) + ARENA_BUF_SIZE);
    *result = (ArenaBuffer){.next = next, .unallocated = (char*)(result + 1)};
    return result;
}

char* ArenaAllocator_alloc(ArenaAllocator* self, size_t size) {
    assert(self);
    if (size > ARENA_BUF_SIZE / 2) {
        void* new_large_head = malloc(sizeof(void*) + size);
        *(void**)new_large_head = self->large_head;
        self->large_head = new_large_head;
        return new_large_head;
    }

    if (!self->head || getBufferSizeLeft(self->head) < size) {
        self->head = newBuffer(self->head);
    }

    char* result = self->head->unallocated;
    self->head->unallocated += size;
    return result;
}

static size_t totalSize(const ArenaAllocator* self) {
    assert(self);
    ArenaBuffer* p = self->head;
    size_t buffer_count = 0;
    while (p) {
        buffer_count += 1;
        p = p->next;
    }
    return buffer_count * ARENA_BUF_SIZE;
}

void ArenaAllocator_reset(ArenaAllocator* self) {
    assert(self);
    {
        void** p = self->large_head;
        while (p) {
            void** next_p = *(void**)p;
            free(p);
            p = next_p;
        }
        self->large_head = NULL;
    }
    size_t total_size = totalSize(self);
    while (total_size - ARENA_BUF_SIZE > ARENA_SIZE_TO_RETAIN) {
        ArenaBuffer* next_head = self->head->next;
        free(self->head);
        self->head = next_head;
        total_size -= ARENA_BUF_SIZE;
    }
}
