#include <ncurses.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "testgen.h"

static void clamp_cursor(EditorBuffer *buf) {
    if (buf->line_count <= 0) buf->line_count = 1;
    if (buf->cur_y < 0) buf->cur_y = 0;
    if (buf->cur_y >= buf->line_count) buf->cur_y = buf->line_count - 1;
    int len = (int)strlen(buf->lines[buf->cur_y]);
    if (buf->cur_x < 0) buf->cur_x = 0;
    if (buf->cur_x > len) buf->cur_x = len;
}

void init_buffer(EditorBuffer *buf) {
    memset(buf, 0, sizeof(*buf));
    buf->line_count = 1;
    buf->cur_y = 0;
    buf->cur_x = 0;
    buf->insert_mode = 0;
}

static void insert_char(EditorBuffer *buf, int ch) {
    char *line = buf->lines[buf->cur_y];
    int len = (int)strlen(line);
    if (len >= MAX_COLS - 1) return;
    memmove(&line[buf->cur_x + 1], &line[buf->cur_x], len - buf->cur_x + 1);
    line[buf->cur_x] = (char)ch;
    buf->cur_x++;
}

static void split_line(EditorBuffer *buf) {
    if (buf->line_count >= MAX_LINES) return;

    char *line = buf->lines[buf->cur_y];
    char tail[MAX_COLS];
    strncpy(tail, &line[buf->cur_x], sizeof(tail) - 1);
    tail[sizeof(tail) - 1] = '\0';
    line[buf->cur_x] = '\0';

    for (int i = buf->line_count; i > buf->cur_y + 1; i--) {
        strncpy(buf->lines[i], buf->lines[i - 1], MAX_COLS);
    }
    strncpy(buf->lines[buf->cur_y + 1], tail, MAX_COLS);
    buf->line_count++;
    buf->cur_y++;
    buf->cur_x = 0;
}

static void backspace(EditorBuffer *buf) {
    if (buf->cur_x > 0) {
        char *line = buf->lines[buf->cur_y];
        int len = (int)strlen(line);
        memmove(&line[buf->cur_x - 1], &line[buf->cur_x], len - buf->cur_x + 1);
        buf->cur_x--;
        return;
    }

    if (buf->cur_y > 0) {
        int prev_len = (int)strlen(buf->lines[buf->cur_y - 1]);
        int cur_len = (int)strlen(buf->lines[buf->cur_y]);
        if (prev_len + cur_len < MAX_COLS) {
            strcat(buf->lines[buf->cur_y - 1], buf->lines[buf->cur_y]);
            for (int i = buf->cur_y; i < buf->line_count - 1; i++) {
                strncpy(buf->lines[i], buf->lines[i + 1], MAX_COLS);
            }
            buf->lines[buf->line_count - 1][0] = '\0';
            buf->line_count--;
            buf->cur_y--;
            buf->cur_x = prev_len;
        }
    }
}

static void draw_editor(const EditorBuffer *buf, const char *title, const char *hint, int top_line) {
    clear();
    mvprintw(0, 0, "%s", title);
    mvprintw(1, 0, "%s", hint);
    mvprintw(2, 0, "mode: %s | i:insert Esc:command Ctrl+S:next Ctrl+Q:quit", buf->insert_mode ? "INSERT" : "COMMAND");
    mvhline(3, 0, '-', COLS);

    int body_top = 4;
    int body_h = LINES - body_top - 1;
    for (int row = 0; row < body_h; row++) {
        int idx = top_line + row;
        if (idx >= buf->line_count) break;
        mvprintw(body_top + row, 0, "%4d %.*s", idx + 1, COLS - 6, buf->lines[idx]);
    }

    mvprintw(LINES - 1, 0, "lines:%d", buf->line_count);
    int screen_y = body_top + (buf->cur_y - top_line);
    int screen_x = 5 + buf->cur_x;
    move(screen_y, screen_x);
    refresh();
}

int edit_buffer(EditorBuffer *buf, const char *title, const char *hint) {
    int top_line = 0;
    int ch;
    keypad(stdscr, TRUE);

    while (1) {
        int body_h = LINES - 5;
        if (buf->cur_y < top_line) top_line = buf->cur_y;
        if (buf->cur_y >= top_line + body_h) top_line = buf->cur_y - body_h + 1;
        draw_editor(buf, title, hint, top_line);

        ch = getch();
        if (ch == 17) return 0;   /* Ctrl+Q */
        if (ch == 19) return 1;   /* Ctrl+S */

        if (!buf->insert_mode) {
            if (ch == 'i') buf->insert_mode = 1;
            else if (ch == KEY_UP || ch == 'k') buf->cur_y--;
            else if (ch == KEY_DOWN || ch == 'j') buf->cur_y++;
            else if (ch == KEY_LEFT || ch == 'h') buf->cur_x--;
            else if (ch == KEY_RIGHT || ch == 'l') buf->cur_x++;
            clamp_cursor(buf);
            continue;
        }

        if (ch == 27) {
            buf->insert_mode = 0;
        } else if (ch == KEY_UP) {
            buf->cur_y--;
        } else if (ch == KEY_DOWN) {
            buf->cur_y++;
        } else if (ch == KEY_LEFT) {
            buf->cur_x--;
        } else if (ch == KEY_RIGHT) {
            buf->cur_x++;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            backspace(buf);
        } else if (ch == '\n' || ch == '\r') {
            split_line(buf);
        } else if (isprint(ch)) {
            insert_char(buf, ch);
        }
        clamp_cursor(buf);
    }
}

int input_text(const char *title, const char *label, char *out, int out_size) {
    clear();
    mvprintw(0, 0, "%s", title);
    mvprintw(2, 0, "%s", label);
    echo();
    curs_set(1);
    move(2, (int)strlen(label));
    int rc = getnstr(out, out_size - 1);
    noecho();
    curs_set(0);
    return rc == ERR ? 0 : 1;
}

int input_long_value(const char *title, const char *label, long *out) {
    char buf[64];
    if (!input_text(title, label, buf, sizeof(buf))) return 0;
    char *end = NULL;
    long val = strtol(buf, &end, 10);
    if (end == buf || *end != '\0') return 0;
    *out = val;
    return 1;
}
