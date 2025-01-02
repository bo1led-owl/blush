#pragma once

#include <stdbool.h>

#include "array_list.h"
ARRAY_LIST_STRUCT(char*, Vars)

void Vars_init(Vars* self);
void Vars_deinit(Vars* self);
const char* Vars_get(const Vars* self, const char* key);
void Vars_set(Vars* self, const char* key, const char* value, bool replace);
