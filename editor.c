#include <ncurses.h>
#include <string.h>
#include "testgen.h"

#define EDIT_NEXT 1
#define EDIT_BACK 2
#define EDIT_QUIT 0

static int min_int(int a, int b) { return a < b ? a : b; }

void editor_init(EditorBuffer *b) {
    memset(b, 0, sizeof(*b));
    b->line_count = 1;
    b->cy = 0;
    b->cx = 0;
    b->insert_mode = 1;
}

void editor_set_single_line(EditorBuffer *b, const char *s) {
    editor_init(b);
    strncpy(b->lines[0], s, MAX_COLS - 1);
    b->lines[0][MAX_COLS - 1] = '\0';
    b->cx = (int)strlen(b->lines[0]);
}

void editor_get_single_line(const EditorBuffer *b, char *out, int out_size) {
    if (out_size <= 0) return;
    strncpy(out, b->lines[0], (size_t)out_size - 1);
    out[out_size - 1] = '\0';
}

static void insert_text(EditorBuffer *b, const char *text) {
    while (*text) {
        char *line = b->lines[b->cy];
        int len = (int)strlen(line);
        if (len >= MAX_COLS - 1) return;
        if (b->cx > len) b->cx = len;
        memmove(&line[b->cx + 1], &line[b->cx], (size_t)(len - b->cx + 1));
        line[b->cx] = *text++;
        b->cx++;
    }
}

static void insert_char(EditorBuffer *b, int ch) {
    char tmp[2] = {(char)ch, '\0'};
    insert_text(b, tmp);
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

static void draw_editor(const EditorBuffer *b, const char *title, const char *help1, const char *help2, int regex_mode) {
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
    if (regex_mode) {
        mvprintw(rows - 1, 0, "MODE:%s  F2:next  F9:back  F10:quit  F3:insert regex part  ESC/i", b->insert_mode ? "INSERT" : "COMMAND");
    } else {
        mvprintw(rows - 1, 0, "MODE:%s  F2:next  F9:back  F10:quit  i:insert  ESC:command", b->insert_mode ? "INSERT" : "COMMAND");
    }

    int cursor_y = 5 + b->cy - top;
    int cursor_x = 6 + b->cx;
    if (cursor_y >= 5 && cursor_y < rows - 2 && cursor_x < cols) {
        int len = (int)strlen(b->lines[b->cy]);
        attron(A_UNDERLINE | A_BOLD);
        if (b->cx < len) mvaddch(cursor_y, cursor_x, b->lines[b->cy][b->cx]);
        else mvaddch(cursor_y, cursor_x, ' ');
        attroff(A_UNDERLINE | A_BOLD);
    }
    move(cursor_y, cursor_x);
    refresh();
}

typedef struct { const char *label; const char *text; } RegexPart;

static const char *select_regex_part(void) {
    static const RegexPart parts[] = {
        {"Start of line", "^"},
        {"End of line", "$"},
        {"Digits repeat", "[0-9]+"},
        {"Optional sign", "-?"},
        {"Decimal point", "\\."},
        {"Any chars", ".*"},
        {"One or more spaces", "[[:space:]]+"},
        {"Zero or more spaces", "[[:space:]]*"},
        {"Word", "[A-Za-z0-9_]+"},
        {"Date YYYY-MM-DD", "[0-9]{4}-[0-9]{2}-[0-9]{2}"},
        {"Time HH:MM:SS", "[0-9]{2}:[0-9]{2}:[0-9]{2}"},
        {"IPv4", "([0-9]{1,3}\\.){3}[0-9]{1,3}"},
        {"UUID", "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"}
    };
    int count = (int)(sizeof(parts) / sizeof(parts[0]));
    int selected = 0;
    int ch;
    keypad(stdscr, TRUE);
    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        erase();
        mvprintw(0, 0, "Insert regex part");
        mvhline(1, 0, '-', cols);
        for (int i = 0; i < count && 3 + i < rows - 4; i++) {
            if (i == selected) attron(A_UNDERLINE | A_BOLD);
            mvprintw(3 + i, 2, "%s", parts[i].label);
            if (i == selected) attroff(A_UNDERLINE | A_BOLD);
        }
        mvhline(rows - 3, 0, '-', cols);
        mvprintw(rows - 2, 0, "Insert: %s", parts[selected].text);
        mvprintw(rows - 1, 0, "Up/Down: move   Enter: insert   F9/Esc: cancel");
        refresh();
        ch = getch();
        if (ch == KEY_F(9) || ch == 27) return NULL;
        if (ch == KEY_UP && selected > 0) selected--;
        else if (ch == KEY_DOWN && selected < count - 1) selected++;
        else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) return parts[selected].text;
    }
}

static int editor_loop(EditorBuffer *b, const char *title, const char *help1, const char *help2, int regex_mode) {
    int ch;
    keypad(stdscr, TRUE);
    while (1) {
        draw_editor(b, title, help1, help2, regex_mode);
        ch = getch();
        if (ch == KEY_F(10)) return EDIT_QUIT;
        if (ch == KEY_F(9)) return EDIT_BACK;
        if (ch == KEY_F(2)) return EDIT_NEXT;
        if (regex_mode && ch == KEY_F(3)) {
            const char *part = select_regex_part();
            if (part) insert_text(b, part);
            continue;
        }
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

int editor_run(EditorBuffer *b, const char *title, const char *help1, const char *help2) {
    return editor_loop(b, title, help1, help2, 0);
}

int editor_run_regex(EditorBuffer *b, const char *title, const char *help1, const char *help2) {
    return editor_loop(b, title, help1, help2, 1);
}
