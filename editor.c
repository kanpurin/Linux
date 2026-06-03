#include <ncurses.h>
#include <string.h>
#include "testgen.h"

static int min_int(int a, int b) { return a < b ? a : b; }

void editor_init(EditorBuffer *b) {
    memset(b, 0, sizeof(*b));
    b->line_count = 1;
    b->cy = 0;
    b->cx = 0;
    b->insert_mode = 1;
}

static void insert_char(EditorBuffer *b, int ch) {
    char *line = b->lines[b->cy];
    int len = (int)strlen(line);
    if (len >= MAX_COLS - 1) return;
    if (b->cx > len) b->cx = len;
    memmove(&line[b->cx + 1], &line[b->cx], (size_t)(len - b->cx + 1));
    line[b->cx] = (char)ch;
    b->cx++;
}

static void backspace(EditorBuffer *b) {
    if (b->cx > 0) {
        char *line = b->lines[b->cy];
        int len = (int)strlen(line);
        memmove(&line[b->cx - 1], &line[b->cx], (size_t)(len - b->cx + 1));
        b->cx--;
        return;
    }
    if (b->cy > 0) {
        int prev_len = (int)strlen(b->lines[b->cy - 1]);
        int cur_len = (int)strlen(b->lines[b->cy]);
        if (prev_len + cur_len < MAX_COLS - 1) {
            strcat(b->lines[b->cy - 1], b->lines[b->cy]);
            for (int i = b->cy; i < b->line_count - 1; i++) {
                strcpy(b->lines[i], b->lines[i + 1]);
            }
            b->line_count--;
            b->cy--;
            b->cx = prev_len;
        }
    }
}

static void newline(EditorBuffer *b) {
    if (b->line_count >= MAX_LINES) return;
    char tail[MAX_COLS];
    char *line = b->lines[b->cy];
    int len = (int)strlen(line);
    if (b->cx > len) b->cx = len;
    strcpy(tail, &line[b->cx]);
    line[b->cx] = '\0';
    for (int i = b->line_count; i > b->cy + 1; i--) {
        strcpy(b->lines[i], b->lines[i - 1]);
    }
    strcpy(b->lines[b->cy + 1], tail);
    b->line_count++;
    b->cy++;
    b->cx = 0;
}

static void draw_editor(const EditorBuffer *b, const char *title, const char *help1, const char *help2) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    erase();
    mvprintw(0, 0, "testgen - %s", title);
    mvhline(1, 0, '-', cols);
    mvprintw(2, 0, "%s", help1);
    mvprintw(3, 0, "%s", help2);
    mvhline(4, 0, '-', cols);

    int view_h = rows - 7;
    int top = 0;
    if (b->cy >= view_h) top = b->cy - view_h + 1;
    for (int i = 0; i < view_h && top + i < b->line_count; i++) {
        mvprintw(5 + i, 0, "%4d  ", top + i + 1);
        addnstr(b->lines[top + i], cols - 7);
    }
    mvhline(rows - 2, 0, '-', cols);
    mvprintw(rows - 1, 0, "MODE:%s  i:insert  ESC:command  F2:next/save  F10:quit", b->insert_mode ? "INSERT" : "COMMAND");
    move(5 + b->cy - top, 6 + b->cx);
    refresh();
}

int editor_run(EditorBuffer *b, const char *title, const char *help1, const char *help2) {
    int ch;
    keypad(stdscr, TRUE);
    while (1) {
        draw_editor(b, title, help1, help2);
        ch = getch();
        if (ch == KEY_F(10)) return 0; /* quit */
        if (ch == KEY_F(2)) return 1;  /* next */
        if (ch == 27) { b->insert_mode = 0; continue; }
        if (!b->insert_mode && ch == 'i') { b->insert_mode = 1; continue; }

        if (ch == KEY_UP && b->cy > 0) {
            b->cy--;
            b->cx = min_int(b->cx, (int)strlen(b->lines[b->cy]));
        } else if (ch == KEY_DOWN && b->cy < b->line_count - 1) {
            b->cy++;
            b->cx = min_int(b->cx, (int)strlen(b->lines[b->cy]));
        } else if (ch == KEY_LEFT && b->cx > 0) {
            b->cx--;
        } else if (ch == KEY_RIGHT && b->cx < (int)strlen(b->lines[b->cy])) {
            b->cx++;
        } else if (b->insert_mode && (ch == KEY_BACKSPACE || ch == 127 || ch == 8)) {
            backspace(b);
        } else if (b->insert_mode && (ch == '\n' || ch == '\r' || ch == KEY_ENTER)) {
            newline(b);
        } else if (b->insert_mode && ch >= 32 && ch <= 126) {
            insert_char(b, ch);
        }
    }
}
