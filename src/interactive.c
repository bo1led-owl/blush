#include "interactive.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "dyn_string.h"
#include "executor.h"

// turn off the formatter because it can break some literals
// clang-format off
#define ANSI_LITERAL(code) "\x1b[" #code

#define COLOR_RESET ANSI_LITERAL(0m)
#define COLOR_BOLD ANSI_LITERAL(1m)
#define COLOR_BLACK ANSI_LITERAL(30m)
#define COLOR_RED ANSI_LITERAL(31m)
#define COLOR_GREEN ANSI_LITERAL(32m)
#define BG_WHITE ANSI_LITERAL(47m)
#define BG_BRIGHT_WHITE ANSI_LITERAL(107m)

#define CURSOR_UP ANSI_LITERAL(A)
#define CURSOR_DOWN ANSI_LITERAL(B)
#define CURSOR_FORWARD ANSI_LITERAL(C)
#define CURSOR_BACK ANSI_LITERAL(D)
#define CURSOR_SAVE ANSI_LITERAL(s)
#define CURSOR_LINE_START ANSI_LITERAL(G)
#define CURSOR_RESTORE ANSI_LITERAL(u)
#define CURSOR_TOPLEFT ANSI_LITERAL(;H)
#define CURSOR_NEXTLINE ANSI_LITERAL(E)
#define SCROLL_UP ANSI_LITERAL(S)
#define CLEAR_SCREEN ANSI_LITERAL(2J)
#define DSR ANSI_LITERAL(6n) /* Device Status Report */
// clang-format on

static struct termios orig_termios;
static struct {
    size_t win_rows, win_cols;
    size_t row, col;
    size_t line_start;
    enum {
        State_Normal,
        State_EscSeq,
        State_CtrlSeq,
    } read_state;
    bool awaiting_command;
    bool need_more_input;
    String command;
    String line;
    Executor executor;
} state;

static void stdoutWrite(const char* s, size_t n) {
    fwrite(s, 1, n, stdout);
}

static void stdoutFlush(void) {
    fflush(stdout);
}

static void stderrWrite(const char* s, size_t n) {
    fwrite(s, 1, n, stderr);
}

#define stdoutWriteLiteral(lit) stdoutWrite(lit, sizeof(lit))
#define stderrWriteLiteral(lit) stderrWrite(lit, sizeof(lit))

static void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void updateWindowSize(void) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    state.win_rows = w.ws_row;
    state.win_cols = w.ws_col;
}

static void updateCursorPosition(void) {
    state.row = 0;
    state.col = 0;

    stdoutWriteLiteral(DSR);
    stdoutFlush();
    char c;
    size_t* cur_dim = &state.row;
    // terminal replies with `ESC[n;mR`, where `n` is row and `m` is col
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'R') {
        if (!isdigit(c)) {
            if (c == ';') {
                if (*cur_dim == 0) {
                    *cur_dim = 1;
                }
                cur_dim = &state.col;
            }
            continue;
        }
        *cur_dim *= 10;
        *cur_dim += (size_t)(c - '0');
    }
    if (*cur_dim == 0) {
        *cur_dim = 1;
    }

    // because the values are starting from 1 (WHY? WHY???)
    state.row -= 1;
    state.col -= 1;
}

static void handle_sigint(int sig) {
    Executor_sendSignalToChild(&state.executor, sig);
    signal(sig, handle_sigint);
}

static void handle_winch(int sig) {
    UNUSED(sig);
    updateWindowSize();
    updateCursorPosition();
    signal(SIGWINCH, handle_winch);
}

static void deinit(void) {
    disableRawMode();
    Executor_deinit(&state.executor);
    String_deinit(&state.line);
}

static void init(void) {
    atexit(deinit);
    signal(SIGWINCH, handle_winch);
    enableRawMode();
    updateWindowSize();
    updateCursorPosition();

    signal(SIGINT, handle_sigint);

    state.read_state = State_Normal;
    state.awaiting_command = true;
    state.need_more_input = false;
    String_init(&state.line);

    Executor_init(&state.executor);

    Executor_setVarCStrs(&state.executor, "PS1", "$ ", false);
    Executor_setVarCStrs(&state.executor, "PS2", "> ", false);
}

static void moveToNextLine(void) {
    if (state.row >= state.win_rows - 1) {
        stdoutWriteLiteral(SCROLL_UP);
    }
    state.col = 0;
    stdoutWriteLiteral(CURSOR_NEXTLINE);
}

static void prompt(void) {
    const char* prompt;
    if (!state.need_more_input) {
        prompt = Executor_getVarCStr(&state.executor, "PS1");
    } else {
        prompt = Executor_getVarCStr(&state.executor, "PS2");
    }
    size_t len = strlen(prompt);
    stderrWrite(prompt, len);
    state.line_start = len;
    state.col += len;
}

static void readCharNormal(char c) {
    switch (c) {
        case 0x1B:  // escape character
            state.read_state = State_EscSeq;
            break;
        case 12:  // form feed
            stdoutWriteLiteral(CLEAR_SCREEN CURSOR_TOPLEFT);
            stdoutFlush();
            state.row = 0;
            state.col = 0;
            state.awaiting_command = true;
            break;
        case 8:    // backspace
        case 127:  // delete
            if (state.col - state.line_start <= state.line.size && state.col > state.line_start) {
                String_remove(&state.line, state.col - state.line_start - 1);
                state.col -= 1;

                // clang-format off
                printf(ANSI_LITERAL(%zuG), state.line_start + 1);
                // clang-format on
                stdoutWriteLiteral(ANSI_LITERAL(0K));  // clear to the end of the line
                stdoutWrite(state.line.items, state.line.size);

                // clang-format off
                printf(ANSI_LITERAL(%zuG), state.col + 1);
                // clang-format on

                stdoutFlush();
            }
            break;
        case '\t':
            // TODO: completions
            break;
        case '\r':
        case '\n':
            moveToNextLine();
            stdoutFlush();

            String_appendSlice(&state.command, state.line.items, state.line.size);

            disableRawMode();
            switch (Executor_execute(&state.executor, state.command.items, state.command.size)) {
                case ExecutionResult_Success:
                    state.need_more_input = false;
                    String_clear(&state.command);
                    break;
                case ExecutionResult_Failure:
                    fprintf(stderr, "Command not found\n");
                    state.need_more_input = false;
                    String_clear(&state.command);
                    break;
                case ExecutionResult_Error:
                    state.need_more_input = false;
                    String_clear(&state.command);
                    perror("Failed to execute command");
                    break;
                case ExecutionResult_NeedMoreInput:
                    state.need_more_input = true;
                    break;
            }
            enableRawMode();
            updateWindowSize();
            updateCursorPosition();

            String_clear(&state.line);
            if (state.col != 0) {
                stdoutWriteLiteral(BG_BRIGHT_WHITE COLOR_BLACK "#" COLOR_RESET);
                moveToNextLine();
            }
            stdoutFlush();
            state.awaiting_command = true;
            break;
        default:
            String_insert(&state.line, (char)c, state.col - state.line_start);
            stdoutWriteLiteral(CURSOR_SAVE);
            stdoutWrite(state.line.items + (state.col - state.line_start),
                        state.line.size - (state.col - state.line_start));
            stdoutWriteLiteral(CURSOR_RESTORE CURSOR_FORWARD);
            stdoutFlush();
            state.col += 1;
            break;
    }
}

void replLoop(void) {
    init();

    for (;;) {
        if (state.awaiting_command) {
            prompt();
            state.awaiting_command = false;
        }

        int c = getchar();
        if (c == EOF && errno == 4) {
            // errno == 4 means "Interrupted syscall"
            continue;
        }
        if (c == 3) {  // ctrl+C
            String_clear(&state.command);
            String_clear(&state.line);
            if (state.need_more_input) {
                state.need_more_input = false;
                stdoutWriteLiteral(CURSOR_LINE_START);
            } else {
                stdoutWriteLiteral(CURSOR_NEXTLINE);
            }
            stdoutFlush();

            state.awaiting_command = true;
            state.col = 0;
            continue;
        }
        if (c == 4 || (c == EOF && errno == 0)) {  // ctrl+D
            break;
        }

        // printf("%d ", c);

        switch (state.read_state) {
            case State_Normal:
                readCharNormal((char)c);
                break;
            case State_EscSeq:
                if (c == '[') {
                    state.read_state = State_CtrlSeq;
                } else {
                    state.read_state = State_Normal;
                }
                break;
            case State_CtrlSeq:
                switch (c) {
                    case 'A':  // cursor up
                        break;
                    case 'B':  // cursor down
                        break;
                    case 'C':  // cursor forward
                        if (state.col - state.line_start + 1 <= state.line.size) {
                            state.col += 1;
                            stdoutWriteLiteral(CURSOR_FORWARD);
                            stdoutFlush();
                        }
                        break;
                    case 'D':  // cursor back
                        if (state.col > state.line_start) {
                            state.col -= 1;
                            stdoutWriteLiteral(CURSOR_BACK);
                            stdoutFlush();
                        }
                        break;
                }
                state.read_state = State_Normal;
                break;
        }
    }
}
