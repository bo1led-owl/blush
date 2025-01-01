#include "executor.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dyn_string.h"

ARRAY_LIST(char*, Literals)
ARRAY_LIST_IMPL(char*, Literals)

typedef enum {
    TokenKind_String
} TokenKind;

typedef struct {
    TokenKind kind;
    char* lit;
    size_t len;
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

static bool Tokenizer_nextTok(Tokenizer* self, Literals* lits, Token* result) {
    Tokenizer_eatWhile(self, isspace);

    int c;
    if ((c = Tokenizer_peekChar(self)) == EOF) {
        return false;
    }

    // =====
    // keyword handling will go here
    // =====

    if (isargch(c) || isquote(c)) {
        String literal;
        String_init(&literal);
        for (;;) {
            size_t prev_cur, len;
            if (isargch(c)) {
                prev_cur = self->cur;
                Tokenizer_eatWhile(self, isargch);
                len = self->cur - prev_cur;
            } else if (isquote(c)) {
                Tokenizer_eatChar(self);  // eat opening quote

                prev_cur = self->cur;
                Tokenizer_eatWhileNot(self, (char)c);
                len = self->cur - prev_cur;

                Tokenizer_eatChar(self);  // eat closing quote
            } else {
                break;
            }
            String_appendSlice(&literal, self->s + prev_cur, len);
            c = Tokenizer_peekChar(self);
        }
        String_append(&literal, '\0');
        size_t len = literal.size;
        char* lit = String_toOwnedSlice(&literal);
        Literals_append(lits, lit);
        *result = (Token){
            .kind = TokenKind_String,
            .lit = lit,
            .len = len,
        };
    } else {
        fprintf(stderr, "wtf\n");
        abort();
    }

    return true;
}

void execute(const char* cmd, const size_t len) {
    Tokenizer tokenizer;
    Tokenizer_init(&tokenizer, cmd, len);

    Literals lits;
    Literals_init(&lits);

    Token tok;
    while (Tokenizer_nextTok(&tokenizer, &lits, &tok)) {
        // logic here
    }

    Literals_append(&lits, NULL);
    pid_t pid = fork();
    if (pid == 0) {
        int res = execvp(lits.items[0], lits.items);
        if (res == -1) {
            perror("execvp");
            abort();
        }
    } else {
        int st;
        waitpid(pid, &st, 0);
    }

    for (size_t i = 0; i < lits.size; ++i) {
        free(lits.items[i]);
    }
    Literals_deinit(&lits);
}
