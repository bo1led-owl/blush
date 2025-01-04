#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "dyn_string.h"
#include "executor.h"
#include "interactive.h"

static int execFile(const char* const* const argv, unsigned argc) {
    assert(argc > 0);
    assert(argc <= INT_MAX);

    const char* path = argv[0];
    FILE* f = fopen(path, "r");
    if (!f) {
        perror("Could not open input file");
        return 1;
    }

    int res = 0;
    size_t line = 1;
    size_t unterminated_char_line = 0;

    Executor executor;
    Executor_init(&executor);
    for (unsigned i = 0; i < argc; ++i) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", i);
        Executor_setVarCStrs(&executor, buf, argv[i], true);
    }

    String buf;
    String_init(&buf);

    int c;
    while ((c = fgetc(f)) != EOF || buf.size > 0) {
        if (c == EOF && unterminated_char_line != 0) {
            break;
        }

        if ((c == '\r' || c == '\n' || c == EOF) && buf.size > 0) {
            ExecutionResult res = Executor_execute(&executor, buf.items, buf.size);

            if (res == ExecutionResult_NeedMoreInput) {
                unterminated_char_line = line;
            } else {
                unterminated_char_line = 0;
            }

            switch (res) {
                case ExecutionResult_Success:
                case ExecutionResult_Error:
                case ExecutionResult_Failure:
                    String_clear(&buf);
                    break;
                case ExecutionResult_NeedMoreInput:
                    break;
            }

            switch (res) {
                case ExecutionResult_Failure:
                    fprintf(stderr, "%s: line %zu: Command not found\n", path, line);
                    break;
                case ExecutionResult_Error:
                    fprintf(stderr, "%s: line %zu: Failed to execute command: %s\n", path, line,
                            strerror(errno));
                    break;
                case ExecutionResult_Success:
                case ExecutionResult_NeedMoreInput:
                    break;
            }
            line += 1;
        }
        if (c != EOF) {
            String_append(&buf, (char)c);
        }
    }

    if (unterminated_char_line != 0) {
        fprintf(stderr,
                "Failed to execute command because of "
                "unterminated character on line %zu\n",
                unterminated_char_line);
        res = 1;
    }

    fclose(f);
    Executor_deinit(&executor);
    String_deinit(&buf);
    return res;
}

static int execString(const char* str) {
    Executor executor;
    Executor_init(&executor);

    int res = 0;
    switch (Executor_execute(&executor, str, strlen(str))) {
        case ExecutionResult_Success:
            break;
        case ExecutionResult_Failure:
            fprintf(stderr, "Command not found\n");
            res = 1;
            break;
        case ExecutionResult_Error:
            perror("Failed to execute command");
            res = 1;
            break;
        case ExecutionResult_NeedMoreInput:
            fprintf(stderr,
                    "Failed to execute command because of "
                    "unterminated character\n");
            res = 1;
            break;
    }

    Executor_deinit(&executor);
    return res;
}

int main(int argc, const char* const* argv) {
    if (argc == 1) {
        replLoop();
        return 0;
    } else if (strcmp(argv[1], "-c") == 0) {
        if (argc < 3) {
            fprintf(stderr, "No command passed after `-c`\n");
            return 2;
        }
        return execString(argv[2]);
    } else {
        if (argc > INT_MAX) {
            fprintf(stderr, "Too many arguments passed, max of %d supported\n", INT_MAX);
            return 2;
        }
        return execFile(argv + 1, (unsigned)(argc - 1));
    }
}
