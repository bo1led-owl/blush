#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "array_list.h"

#define UNUSED(x) (void)(x)

ARRAY_LIST(char, CharBuf)
ARRAY_LIST_IMPL(char, CharBuf)

#define ANSI_LITERAL(code) "\x1b[" #code

static const char COLOR_RESET[] = ANSI_LITERAL(0m);
static const char COLOR_BOLD[] = ANSI_LITERAL(1m);
static const char COLOR_RED[] = ANSI_LITERAL(31m);
static const char COLOR_GREEN[] = ANSI_LITERAL(32m);

static const char CURSOR_UP[] = ANSI_LITERAL(A);
static const char CURSOR_DOWN[] = ANSI_LITERAL(B);
static const char CURSOR_FORWARD[] = ANSI_LITERAL(C);
static const char CURSOR_BACK[] = ANSI_LITERAL(D);
static const char CURSOR_SAVE[] = ANSI_LITERAL(s);
static const char CURSOR_LINE_START[] = ANSI_LITERAL(G);
static const char CURSOR_RESTORE[] = ANSI_LITERAL(u);

static const char SCROLL_UP[] = ANSI_LITERAL(S);
static const char DSR[] = ANSI_LITERAL(6n);  // Device Status Report

#define WRITE_CTRL_SEQ(name) write(STDOUT_FILENO, name, sizeof(name));

static struct termios orig_termios;
typedef struct {
    unsigned win_rows, win_cols;
    unsigned row, col;
} GlobalState;

GlobalState state;

static void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
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

    WRITE_CTRL_SEQ(DSR);
    char c;
    unsigned* cur_dim = &state.row;
    // terminal replies with `ESC[n;mR`, where `n` is row and `m` is col
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'R') {
        if (!isdigit(c)) {
            if (c == ';') {
                cur_dim = &state.col;
            }
            continue;
        }
        *cur_dim *= 10;
        *cur_dim += (unsigned)(c - '0');
    }

    // because the values are starting from 1 (WHY? WHY???)
    state.row -= 1;
    state.col -= 1;
}

static void executeCommand(const char* cmd, const size_t len) {
    // TODO
}

static void handle_winch(int sig) {
    UNUSED(sig);
    updateWindowSize();
    updateCursorPosition();
    signal(SIGWINCH, handle_winch);
}

static void deinit(void) {
    disableRawMode();
}

static void init(void) {
    atexit(deinit);
    signal(SIGWINCH, handle_winch);
    enableRawMode();
    updateWindowSize();
    updateCursorPosition();
}

typedef enum {
    PARSING_NORMAL,
    PARSING_ESC_SEQ,
    WRITE_CTRL_SEQ,
} ParsingState;

int main(void) {
    init();

    char c;
    ParsingState parse_state = PARSING_NORMAL;

    CharBuf line;
    CharBuf_init(&line);
    for (;;) {
        ssize_t read_res = read(STDIN_FILENO, &c, 1);
        if (read_res == -1 && errno == 4) {
            // errno == 4 means "Interrupted syscall"
            continue;
        }
        if (read_res <= 0 || c == 4) {
            break;
        }

        switch (parse_state) {
            case PARSING_NORMAL:
                switch (c) {
                    case 0x1B:
                        parse_state = PARSING_ESC_SEQ;
                        break;
                    case 8:    // backspace
                    case 127:  // delete
                        if (state.col <= line.size && state.col > 0) {
                            CharBuf_remove(&line, state.col - 1);
                            state.col -= 1;

                            WRITE_CTRL_SEQ(
                                ANSI_LITERAL(2K));  // clear whole line
                            WRITE_CTRL_SEQ(CURSOR_BACK);
                            WRITE_CTRL_SEQ(CURSOR_SAVE);
                            WRITE_CTRL_SEQ(CURSOR_LINE_START);
                            write(STDOUT_FILENO, line.items, line.size);
                            WRITE_CTRL_SEQ(CURSOR_RESTORE);
                        }
                        break;
                    case '\r':
                    case '\n':
                        executeCommand(line.items, line.size);
                        if (state.row >= state.win_rows - 1) {
                            WRITE_CTRL_SEQ(SCROLL_UP);
                        } else {
                            WRITE_CTRL_SEQ(CURSOR_DOWN);
                        }
                        CharBuf_clear(&line);
                        state.col = 0;
                        WRITE_CTRL_SEQ(CURSOR_LINE_START);
                        break;
                    default:
                        CharBuf_insert(&line, c, state.col);
                        WRITE_CTRL_SEQ(CURSOR_SAVE);
                        write(STDOUT_FILENO, line.items + state.col,
                              line.size - state.col);
                        WRITE_CTRL_SEQ(CURSOR_RESTORE);
                        WRITE_CTRL_SEQ(CURSOR_FORWARD);
                        state.col += 1;
                }
                break;
            case PARSING_ESC_SEQ:
                if (c == '[') {
                    parse_state = WRITE_CTRL_SEQ;
                } else {
                    parse_state = PARSING_NORMAL;
                }
                break;
            case WRITE_CTRL_SEQ:
                switch (c) {
                    case 'A':  // cursor up
                        break;
                    case 'B':  // cursor down
                        break;
                    case 'C':  // cursor forward
                        if (state.col + 1 <= line.size) {
                            state.col += 1;
                            WRITE_CTRL_SEQ(CURSOR_FORWARD);
                        }
                        break;
                    case 'D':  // cursor back
                        if (state.col > 0) {
                            state.col -= 1;
                            WRITE_CTRL_SEQ(CURSOR_BACK);
                        }
                        break;
                }
                parse_state = PARSING_NORMAL;
                break;
        }
    }

    CharBuf_deinit(&line);
    return 0;
}
