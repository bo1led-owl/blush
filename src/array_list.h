#pragma once

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "alloc.h"

#define ARRAY_LIST_STRUCT(T, NAME) \
    typedef struct NAME {          \
        T* items;                  \
        size_t size;               \
        size_t cap;                \
    } NAME;

#define ARRAY_LIST_SIGNATURES(T, NAME)                                 \
    void NAME##_init(NAME* arr);                                       \
    void NAME##_initWithCapacity(NAME* arr, size_t required_capacity); \
    void NAME##_initFromSlice(NAME* arr, const T* ptr, size_t len);    \
    void NAME##_swap(NAME* lhs, NAME* rhs);                            \
    void NAME##_deinit(NAME* arr);                                     \
    void NAME##_clear(NAME* arr);                                      \
    T* NAME##_toOwnedSlice(NAME* arr);                                 \
    void NAME##_ensureCapacityExact(NAME* arr, size_t required_cap);   \
    void NAME##_ensureCapacity(NAME* arr, size_t required_cap);        \
    void NAME##_resize(NAME* arr, size_t required_size);               \
    void NAME##_append(NAME* arr, T elem);                             \
    void NAME##_appendSlice(NAME* arr, const T* ptr, size_t len);      \
    void NAME##_appendN(NAME* arr, size_t count, T filler);            \
    void NAME##_insert(NAME* arr, T item, size_t index);               \
    T NAME##_pop(NAME* arr);                                           \
    T NAME##_remove(NAME* arr, size_t index);                          \
    void NAME##_removeSlice(NAME* arr, size_t begin, size_t end);      \
    void NAME##_moveElement(NAME* arr, size_t from, size_t to);

#define ARRAY_LIST_IMPL(T, NAME)                                                                  \
    void NAME##_init(NAME* arr) {                                                                 \
        assert(arr);                                                                              \
        *arr = (NAME){                                                                            \
            .items = NULL,                                                                        \
            .cap = 0,                                                                             \
            .size = 0,                                                                            \
        };                                                                                        \
    }                                                                                             \
    void NAME##_initWithCapacity(NAME* arr, size_t required_capacity) {                           \
        assert(arr);                                                                              \
        NAME##_init(arr);                                                                         \
        NAME##_ensureCapacity(arr, required_capacity);                                            \
    }                                                                                             \
    void NAME##_initFromSlice(NAME* arr, const T* ptr, size_t len) {                              \
        assert(arr);                                                                              \
        NAME##_initWithCapacity(arr, len);                                                        \
        NAME##_appendSlice(arr, ptr, len);                                                        \
    }                                                                                             \
    void NAME##_swap(NAME* lhs, NAME* rhs) {                                                      \
        assert(lhs);                                                                              \
        assert(rhs);                                                                              \
        NAME tmp = *lhs;                                                                          \
        *lhs = *rhs;                                                                              \
        *rhs = tmp;                                                                               \
    }                                                                                             \
    void NAME##_deinit(NAME* arr) {                                                               \
        assert(arr);                                                                              \
        free(arr->items);                                                                         \
        *arr = (NAME){0};                                                                         \
    }                                                                                             \
    void NAME##_clear(NAME* arr) {                                                                \
        assert(arr);                                                                              \
        arr->size = 0;                                                                            \
    }                                                                                             \
    T* NAME##_toOwnedSlice(NAME* arr) {                                                           \
        assert(arr);                                                                              \
        T* ptr = arr->items;                                                                      \
        NAME##_init(arr);                                                                         \
        return ptr;                                                                               \
    }                                                                                             \
    void NAME##_ensureCapacityExact(NAME* arr, size_t required_cap) {                             \
        assert(arr);                                                                              \
        if (arr->cap >= required_cap)                                                             \
            return;                                                                               \
        arr->cap = required_cap;                                                                  \
        arr->items = reallocChecked(arr->items, sizeof(T) * required_cap);                        \
    }                                                                                             \
    void NAME##_ensureCapacity(NAME* arr, size_t required_cap) {                                  \
        assert(arr);                                                                              \
        size_t new_cap = (arr->cap) ? arr->cap : 16;                                              \
        while (new_cap < required_cap) {                                                          \
            new_cap *= 2;                                                                         \
        }                                                                                         \
        NAME##_ensureCapacityExact(arr, new_cap);                                                 \
    }                                                                                             \
    void NAME##_append(NAME* arr, T elem) {                                                       \
        assert(arr);                                                                              \
        NAME##_ensureCapacity(arr, arr->size + 1);                                                \
        arr->items[arr->size] = elem;                                                             \
        arr->size += 1;                                                                           \
    }                                                                                             \
    void NAME##_appendSlice(NAME* arr, const T* ptr, size_t len) {                                \
        assert(arr);                                                                              \
        NAME##_ensureCapacity(arr, arr->size + len);                                              \
        memcpy(arr->items + arr->size, ptr, sizeof(T) * len);                                     \
        arr->size += len;                                                                         \
    }                                                                                             \
    void NAME##_appendN(NAME* arr, size_t count, T filler) {                                      \
        assert(arr);                                                                              \
        NAME##_ensureCapacity(arr, arr->size + count);                                            \
        for (size_t i = arr->size; i < arr->size + count; ++i) {                                  \
            arr->items[i] = filler;                                                               \
        }                                                                                         \
        arr->size += count;                                                                       \
    }                                                                                             \
    void NAME##_insert(NAME* arr, T item, size_t index) {                                         \
        assert(arr);                                                                              \
        NAME##_append(arr, item);                                                                 \
        NAME##_moveElement(arr, arr->size - 1, index);                                            \
    }                                                                                             \
    T NAME##_pop(NAME* arr) {                                                                     \
        assert(arr);                                                                              \
        arr->size -= 1;                                                                           \
        return arr->items[arr->size];                                                             \
    }                                                                                             \
    T NAME##_remove(NAME* arr, size_t index) {                                                    \
        assert(arr);                                                                              \
        assert(index < arr->size);                                                                \
        T deleted_elem = arr->items[index];                                                       \
        memmove(arr->items + index, arr->items + index + 1, (arr->size - index - 1) * sizeof(T)); \
        arr->size -= 1;                                                                           \
        return deleted_elem;                                                                      \
    }                                                                                             \
    void NAME##_removeSlice(NAME* arr, size_t begin, size_t end) {                                \
        assert(arr);                                                                              \
        if (begin >= end)                                                                         \
            return;                                                                               \
                                                                                                  \
        memmove(arr->items + begin, arr->items + end, sizeof(T) * (arr->size - end));             \
        arr->size -= end - begin;                                                                 \
    }                                                                                             \
    void NAME##_moveElement(NAME* arr, size_t from, size_t to) {                                  \
        assert(arr);                                                                              \
        if (from == to) {                                                                         \
            return;                                                                               \
        }                                                                                         \
        assert(from < arr->size);                                                                 \
        T elem = arr->items[from];                                                                \
        if (from < to) {                                                                          \
            memmove(arr->items + from, arr->items + from + 1, sizeof(T) * (to - from));           \
            arr->items[to] = elem;                                                                \
        } else {                                                                                  \
            memmove(arr->items + to + 1, arr->items + to, sizeof(T) * (from - to));               \
            arr->items[to] = elem;                                                                \
        }                                                                                         \
    }

#define ARRAY_LIST_DEFINITION(T, NAME) \
    ARRAY_LIST_STRUCT(T, NAME)         \
    ARRAY_LIST_SIGNATURES(T, NAME)

#define ARRAY_LIST_FULL(T, NAME)   \
    ARRAY_LIST_DEFINITION(T, NAME) \
    ARRAY_LIST_IMPL(T, NAME)
