#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "vars.h"

typedef struct {
    Vars vars;
    int last_exit_code;
} Executor;

void Executor_init(Executor* self);
void Executor_deinit(Executor* self);

typedef enum {
    ExecutionResult_Success,
    ExecutionResult_Failure,
    ExecutionResult_NeedMoreInput,
} ExecutionResult;
ExecutionResult Executor_execute(Executor* self, const char* cmd, size_t len);

const char* Executor_getVar(Executor* self, const char* name, size_t len);
void Executor_setVar(Executor* self, const char* name, const char* value, bool replace);

/// Returns `true` if `s` was inserted
bool Executor_setVarRawMove(Executor* self, char* s, bool replace);

void Executor_setVarRawCopy(Executor* self, const char* s, bool replace);
