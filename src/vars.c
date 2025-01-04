#include "vars.h"

#include <stdio.h>

#include "alloc.h"

typedef Vars VarsImpl;
ARRAY_LIST_SIGNATURES(char*, VarsImpl)
ARRAY_LIST_IMPL(char*, VarsImpl)

extern char** environ;

void Vars_init(Vars* self) {
    assert(self);

    VarsImpl_init(self);
    for (char** env = environ; *env != NULL; ++env) {
        const size_t len = strlen(*env);
        char* item = mallocChecked(len + 1);
        memcpy(item, *env, len + 1);
        VarsImpl_append(self, item);
    }
    VarsImpl_append(self, NULL);
}

void Vars_deinit(Vars* self) {
    assert(self);
    for (size_t i = 0; i < self->size; ++i) {
        free(self->items[i]);
    }
    VarsImpl_deinit(self);
}

static bool keyeq(const char* lhs, const size_t lhs_len, const char* rhs, const size_t rhs_len) {
    assert(lhs);
    assert(rhs);

    if (lhs_len != rhs_len) {
        return false;
    }

    assert(lhs_len == rhs_len);
    return memcmp(lhs, rhs, lhs_len) == 0;
}

const char* Vars_get(const Vars* self, const char* key, const size_t len) {
    assert(self);
    assert(self->items[self->size - 1] == NULL);

    for (size_t i = 0; i < self->size - 1; ++i) {
        const char* eqpos = strchr(self->items[i], '=');
        const size_t lhs_len = (size_t)(eqpos - self->items[i]);

        if (keyeq(self->items[i], lhs_len, key, len)) {
            return eqpos + 1;
        }
    }

    return NULL;
}

static void replaceValue(char** item, size_t key_len, const char* new_val, size_t val_len) {
    const size_t new_len = key_len + 1 + val_len;

    *item = reallocChecked(*item, new_len + 1);
    memcpy(*item + key_len + 1, new_val, val_len);
    (*item)[new_len] = '\0';
}

void Vars_set(Vars* self, const char* key, size_t key_len, const char* value, size_t value_len,
              bool replace) {
    assert(self);
    assert(self->items[self->size - 1] == NULL);

    for (size_t i = 0; i < self->size - 1; ++i) {
        const char* eqpos = strchr(self->items[i], '=');
        const size_t cur_key_len = (size_t)(eqpos - self->items[i]);
        if (keyeq(self->items[i], cur_key_len, key, key_len)) {
            if (replace) {
                replaceValue(&self->items[i], cur_key_len, value, value_len);
            }
            return;
        }
    }

    // value was not replaced, have to add one
    const size_t len = key_len + 1 + value_len;
    char* item = mallocChecked(len + 1);
    memcpy(item, key, key_len);
    item[key_len] = '=';
    memcpy(item + key_len + 1, value, value_len);
    item[len] = '\0';

    VarsImpl_insert(self, item, self->size - 2);
}

bool Vars_setRawMove(Vars* self, char* s, bool replace) {
    assert(self);
    assert(self->items[self->size - 1] == NULL);

    const size_t rhs_len = (size_t)(strchr(s, '=') - s);
    for (size_t i = 0; i < self->size - 1; ++i) {
        const char* eqpos = strchr(self->items[i], '=');
        const size_t lhs_len = (size_t)(eqpos - self->items[i]);
        if (keyeq(self->items[i], lhs_len, s, rhs_len)) {
            if (replace) {
                free(self->items[i]);
                self->items[i] = s;
            }
            return replace;
        }
    }

    // value was not replaced, have to add one
    VarsImpl_insert(self, s, self->size - 2);
    return true;
}

void Vars_setRawCopy(Vars* self, const char* s, bool replace) {
    assert(self);
    assert(self->items[self->size - 1] == NULL);

    const size_t s_key_len = (size_t)(strchr(s, '=') - s);
    for (size_t i = 0; i < self->size - 1; ++i) {
        const char* eqpos = strchr(self->items[i], '=');
        const size_t lhs_key_len = (size_t)(eqpos - self->items[i]);
        if (keyeq(self->items[i], lhs_key_len, s, s_key_len)) {
            if (replace) {
                size_t len = strlen(s);
                self->items[i] = reallocChecked(self->items[i], len + 1);
                memcpy(self->items[i], s, len + 1);
            }
            return;
        }
    }

    // value was not replaced, have to add one
    size_t len = strlen(s);
    char* item = mallocChecked(len + 1);
    memcpy(item, s, len + 1);
    VarsImpl_insert(self, item, self->size - 2);
}
