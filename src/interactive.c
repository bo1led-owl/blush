#include "interactive.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "dyn_string.h"
#include "executor.h"

#define ANSI_LITERAL(code) "\x1b[" #code

#define COLOR_RESET ANSI_LITERAL(0m)
#define COLOR_BOLD ANSI_LITERAL(1m)
#define COLOR_RED ANSI_LITERAL(31m)
#define COLOR_GREEN ANSI_LITERAL(32m)

#define CURSOR_UP ANSI_LITERAL(A)
#define CURSOR_DOWN ANSI_LITERAL(B)
#define CURSOR_FORWARD ANSI_LITERAL(C)
#define CURSOR_BACK ANSI_LITERAL(D)
#define CURSOR_SAVE ANSI_LITERAL(s)
#define CURSOR_LINE_START ANSI_LITERAL(G)
#define CURSOR_RESTORE ANSI_LITERAL(u)

#define SCROLL_UP ANSI_LITERAL(S)
// Device Status Report
#define DSR ANSI_LITERAL(6n)

#define WRITE_STRING(s, n) write(STDOUT_FILENO, s, n)
#define WRITE_LITERAL(lit) WRITE_STRING(lit, sizeof(lit))

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

    WRITE_LITERAL(DSR);
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

void replLoop(void) {
    init();

    char c;
    ParsingState parse_state = PARSING_NORMAL;

    String line;
    String_init(&line);
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
                            String_remove(&line, state.col - 1);
                            state.col -= 1;

                            // `2K` is "clear whole line"
                            WRITE_LITERAL(ANSI_LITERAL(2K) CURSOR_BACK CURSOR_SAVE CURSOR_LINE_START);
                            WRITE_STRING(line.items, line.size);
                            WRITE_LITERAL(CURSOR_RESTORE);
                        }
                        break;
                    case '\r':
                    case '\n':
                        if (state.row >= state.win_rows - 1) {
                            WRITE_LITERAL(SCROLL_UP);
                        } else {
                            WRITE_LITERAL(CURSOR_DOWN);
                        }
                        state.col = 0;
                        WRITE_LITERAL(CURSOR_LINE_START);

                        disableRawMode();
                        execute(line.items, line.size);
                        enableRawMode();
                        
                        String_clear(&line);
                        WRITE_LITERAL(CURSOR_LINE_START);
                        break;
                    default:
                        String_insert(&line, c, state.col);
                        WRITE_LITERAL(CURSOR_SAVE);
                        WRITE_STRING(line.items + state.col, line.size - state.col);
                        WRITE_LITERAL(CURSOR_RESTORE CURSOR_FORWARD);
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
                            WRITE_LITERAL(CURSOR_FORWARD);
                        }
                        break;
                    case 'D':  // cursor back
                        if (state.col > 0) {
                            state.col -= 1;
                            WRITE_LITERAL(CURSOR_BACK);
                        }
                        break;
                }
                parse_state = PARSING_NORMAL;
                break;
        }
    }

    String_deinit(&line);
}
