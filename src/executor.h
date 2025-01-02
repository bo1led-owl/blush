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
ExecutionResult Executor_execute(Executor* self, const char* cmd, const size_t len);

const char* Executor_getVar(Executor* self, const char* name);
void Executor_setVar(Executor* self, const char* name, const char* value, bool replace);
