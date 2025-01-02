#include "executor.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dyn_string.h"

ARRAY_LIST_FULL(char*, Strings)

typedef enum {
    TokenKind_Whitespace,
    TokenKind_EqSign,
    TokenKind_String,
    TokenKind_VariableReference,
} TokenKind;

typedef struct {
    TokenKind kind;
} Token;

typedef struct {
    const char* s;
    size_t len;
    size_t cur;
} Tokenizer;

static void Tokenizer_init(Tokenizer* self, const char* input, const size_t len) {
    *self = (Tokenizer){
        .s = input,
        .len = len,
        .cur = 0,
    };
}

static int Tokenizer_peekChar(const Tokenizer* self) {
    if (self->cur >= self->len) {
        return EOF;
    }
    return self->s[self->cur];
}

static int Tokenizer_eatChar(Tokenizer* self) {
    int res;
    if ((res = Tokenizer_peekChar(self)) != EOF) {
        self->cur += 1;
    }
    return res;
}

static void Tokenizer_eatWhile(Tokenizer* self, int (*pred)(int)) {
    while (pred(Tokenizer_peekChar(self))) {
        self->cur += 1;
    }
}

static void Tokenizer_eatWhileNot(Tokenizer* self, char ch) {
    int c;
    while ((c = Tokenizer_peekChar(self)) != EOF && c != ch) {
        self->cur += 1;
    }
}

static int isquote(int c) {
    return c == '"' || c == '\'' || c == '`';
}

static int isargch(int c) {
    return isalnum(c) || c == '-' || c == '_' || c == '/' || c == '.';
}

static void Tokenizer_readArg(Tokenizer* self, String* lit) {
    int c;
    bool escaped = false;
    for (;;) {
        c = Tokenizer_peekChar(self);
        if (c == EOF) {
            break;
        }

        if (!escaped && c == '\\') {
            escaped = true;
        } else if (!escaped && isargch(c)) {
            String_append(lit, (char)c);
            self->cur += 1;
        } else if (escaped) {
            String_append(lit, (char)c);
            self->cur += 1;
        } else {
            break;
        }
    }
}

static bool Tokenizer_nextTok(Tokenizer* self, String* lit, Token* result) {
    int c;
    if ((c = Tokenizer_peekChar(self)) == EOF) {
        return false;
    }

    if (isspace(c)) {
        Tokenizer_eatWhile(self, isspace);
        *result = (Token){.kind = TokenKind_Whitespace};
    } else if (c == '=') {
        Tokenizer_eatChar(self);  // eat `=`
        *result = (Token){.kind = TokenKind_EqSign};
        return true;
    } else if (c == '$') {
        Tokenizer_eatChar(self);  // eat `$`

        size_t prev_cur = self->cur;
        Tokenizer_eatWhile(self, isalnum);
        size_t len = self->cur - prev_cur;
        String_appendSlice(lit, self->s + prev_cur, len);
        String_append(lit, '\0');
        *result = (Token){.kind = TokenKind_VariableReference};
    } else if (isargch(c) || isquote(c)) {
        for (;;) {
            if (isargch(c)) {
                Tokenizer_readArg(self, lit);
            } else if (isquote(c)) {
                size_t prev_cur, len;
                Tokenizer_eatChar(self);  // eat opening quote

                prev_cur = self->cur;
                Tokenizer_eatWhileNot(self, (char)c);
                len = self->cur - prev_cur;

                Tokenizer_eatChar(self);  // eat closing quote
                String_appendSlice(lit, self->s + prev_cur, len);
            } else {
                break;
            }
            c = Tokenizer_peekChar(self);
        }
        *result = (Token){.kind = TokenKind_String};
    } else {
        fprintf(stderr, "%c wtf\n", c);
        abort();
    }

    return true;
}

void Executor_init(Executor* self) {
    Vars_init(&self->vars);
}

void Executor_deinit(Executor* self) {
    Vars_deinit(&self->vars);
}

const char* Executor_getVar(Executor* self, const char* name) {
    return Vars_get(&self->vars, name);
}

void Executor_setVar(Executor* self, const char* name, const char* value, bool replace) {
    Vars_set(&self->vars, name, value, replace);
}

void Executor_setVarRaw(Executor* self, char* s, bool replace) {
    char* eqpos = strchr(s, '=');
    *eqpos = '\0';
    const char* name = s;
    const char* value = eqpos + 1;
    Executor_setVar(self, name, value, replace);
}

static void Executor_cd(Executor* self, size_t argc, char const* const* argv) {
    if (argc > 1) {
        fprintf(stderr, "Expected 1 or less arguments, got %zu\n", argc);
        return;
    }

    const char* path = NULL;
    if (argc == 0) {
        path = Executor_getVar(self, "HOME");
    } else if (argc == 1) {
        path = argv[0];
    }

    if (chdir(path) == -1) {
        perror("cd");
    }
}

static bool forkExec(char* const* args, char* const* env) {
    pid_t pid = fork();
    if (pid == 0) {
        int res = execve(args[0], args, env);
        if (res == -1) {
            exit(errno);
        }
    } else {
        int st;
        waitpid(pid, &st, 0);
        st = WEXITSTATUS(st);
        return st == 0;
    }

    assert(0);
    __builtin_unreachable();
}

ExecutionResult Executor_execute(Executor* self, const char* cmd, const size_t len) {
    ExecutionResult res = ExecutionResult_Success;

    Tokenizer tokenizer;
    Tokenizer_init(&tokenizer, cmd, len);

    String cur_arg, lit;
    String_init(&cur_arg);
    String_init(&lit);

    Strings args;
    Strings_init(&args);

    bool leading_assignments = true;
    bool parsing_assignment = false;
    bool null_arg = true;
    Token tok;

    Tokenizer_eatWhile(&tokenizer, isspace);
    while (Tokenizer_nextTok(&tokenizer, &lit, &tok)) {
        switch (tok.kind) {
            case TokenKind_Whitespace:
                String_append(&cur_arg, '\0');

                if (!parsing_assignment) {
                    leading_assignments = false;
                    Strings_append(&args, String_toOwnedSlice(&cur_arg));
                } else {
                    Executor_setVarRaw(self, cur_arg.items, true);
                    String_clear(&cur_arg);
                }
                parsing_assignment = false;
                null_arg = true;
                break;
            case TokenKind_EqSign:
                null_arg = false;
                if (leading_assignments) {
                    parsing_assignment = true;
                }
                String_append(&cur_arg, '=');
                break;
            case TokenKind_String:
                null_arg = false;
                String_appendSlice(&cur_arg, lit.items, lit.size);
                break;
            case TokenKind_VariableReference:
                String_append(&lit, '\0');
                const char* val = Executor_getVar(self, lit.items);
                if (val) {
                    null_arg = false;
                    String_appendSlice(&cur_arg, val, strlen(val));
                }
                break;
        }
        String_clear(&lit);
    }

    if (!null_arg) {
        String_append(&cur_arg, '\0');
        if (!parsing_assignment) {
            Strings_append(&args, String_toOwnedSlice(&cur_arg));
        } else {
            Executor_setVarRaw(self, cur_arg.items, true);
        }
    }

    String_deinit(&cur_arg);
    String_deinit(&lit);

    if (args.size == 0) {
        goto cleanup;
    }

    if (strcmp(args.items[0], "cd") == 0) {
        Executor_cd(self, args.size - 1, (char const* const*)(args.items + 1));
    } else {
        Strings_append(&args, NULL);
        const char* path = Executor_getVar(self, "PATH");
        char* exe = args.items[0];

        size_t exe_len = strlen(exe);
        if (exe_len == 0) {
            res = ExecutionResult_Failure;
        } else if (exe[0] == '.') {
            if (!forkExec(args.items, self->vars.items)) {
                res = ExecutionResult_Failure;
            }
        } else {
            String buf;
            String_init(&buf);
            for (;;) {
                const char* colon = strchr(path, ':');
                const size_t len = (colon) ? (size_t)(colon - path) : strlen(path);
                String_clear(&buf);
                String_appendSlice(&buf, path, len);
                if (buf.items[buf.size - 1] != '/') {
                    String_append(&buf, '/');
                }
                String_appendSlice(&buf, exe, exe_len);
                String_append(&buf, '\0');

                args.items[0] = buf.items;
                if (forkExec(args.items, self->vars.items)) {
                    break;
                } else if (!colon) {
                    res = ExecutionResult_Failure;
                    break;
                } else {
                    path = colon + 1;
                }
            }
            String_deinit(&buf);
            args.items[0] = exe;  // for nice freeing
        }
    }

cleanup:
    for (size_t i = 0; i < args.size; ++i) {
        if (args.items[i] != NULL) {
            free(args.items[i]);
        }
    }
    Strings_deinit(&args);

    return res;
}
