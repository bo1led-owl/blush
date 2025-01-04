#pragma once

#include <stdbool.h>

#include "array_list.h"
ARRAY_LIST_STRUCT(char*, Vars)

void Vars_init(Vars* self);
void Vars_deinit(Vars* self);
const char* Vars_get(const Vars* self, const char* key, size_t len);
void Vars_set(Vars* self, const char* key, size_t key_len, const char* value, size_t value_len,
              bool replace);

/// Returns `true` if `s` was inserted
bool Vars_setRawMove(Vars* self, char* s, bool replace);

void Vars_setRawCopy(Vars* self, const char* s, bool replace);
