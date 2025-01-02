#include "vars.h"

#include <stdio.h>

typedef Vars VarsImpl;
ARRAY_LIST_SIGNATURES(char*, VarsImpl)
ARRAY_LIST_IMPL(char*, VarsImpl)

extern char** environ;

void Vars_init(Vars* self) {
    assert(self);

    VarsImpl_init(self);
    for (char** env = environ; *env != NULL; ++env) {
        const size_t len = strlen(*env);
        char* item = malloc(len + 1);
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

/// `s` should be "key=value\0", `k` should be null-terminated and contain only the key
static bool keyeq(const char* s, const char* k) {
    assert(s);
    assert(k);
    const char* eqpos = strchr(s, '=');
    assert(eqpos != NULL);
    const size_t s_key_len = (size_t)(eqpos - s);
    const size_t k_len = strlen(k);
    if (k_len != s_key_len) {
        return false;
    }

    for (size_t i = 0; i < k_len; ++i) {
        if (s[i] != k[i]) {
            return false;
        }
    }
    return true;
}

const char* Vars_get(const Vars* self, const char* key) {
    assert(self);
    assert(self->items[self->size - 1] == NULL);
    for (size_t i = 0; i < self->size - 1; ++i) {
        if (keyeq(self->items[i], key)) {
            return strchr(self->items[i], '=') + 1;
        }
    }

    return NULL;
}

static void replaceValue(char** item, const char* new_val) {
    const char* eqptr = strchr(*item, '=');
    const size_t key_len = (size_t)(eqptr - *item);
    const size_t new_value_len = strlen(new_val);
    const size_t old_value_len = strlen(eqptr);
    const size_t new_len = key_len + (old_value_len - new_value_len);

    *item = realloc(*item, new_len + 1);
    memcpy(*item + key_len + 1, new_val, new_value_len + 1);
}

void Vars_set(Vars* self, const char* key, const char* value, bool replace) {
    assert(self);
    assert(self->items[self->size - 1] == NULL);
    for (size_t i = 0; i < self->size - 1; ++i) {
        if (keyeq(self->items[i], key)) {
            if (replace) {
                replaceValue(&self->items[i], value);
            }
            return;
        }
    }

    // value was not replaced, have to add one
    const size_t len = strlen(key) + 1 + strlen(value);
    char* item = malloc(len + 1);
    snprintf(item, len + 1, "%s=%s", key, value);
    VarsImpl_insert(self, item, self->size - 2);
}
