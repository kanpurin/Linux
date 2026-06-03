#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include "testgen.h"

#define NAV_QUIT 0
#define NAV_NEXT 1
#define NAV_BACK 2

typedef struct { const char *label; int value; } MenuItem;

static void status_message(const char *msg) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    mvhline(rows - 2, 0, ' ', cols);
    mvprintw(rows - 2, 0, "%s", msg);
    refresh();
}

static int buffer_contains_result(const EditorBuffer *b) {
    for (int i = 0; i < b->line_count; i++) {
        if (strstr(b->lines[i], "RESULT") != NULL) return 1;
    }
    return 0;
}

static void draw_single_line(const char *label, const char *buf, int cx) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    erase();
    mvprintw(0, 0, "testgen");
    mvhline(1, 0, '-', cols);
    mvprintw(3, 0, "%s", label);
    mvprintw(5, 0, "%s", buf);
    int len = (int)strlen(buf);
    int draw_x = cx;
    if (draw_x >= cols) draw_x = cols - 1;
    attron(A_UNDERLINE | A_BOLD);
    if (cx < len) mvaddch(5, draw_x, buf[cx]);
    else mvaddch(5, draw_x, ' ');
    attroff(A_UNDERLINE | A_BOLD);
    mvhline(rows - 2, 0, '-', cols);
    mvprintw(rows - 1, 0, "Enter/F2: OK   F9: back   F10: quit   Left/Right: move   Backspace: delete");
    refresh();
}

static int prompt_line_nav(const char *label, char *buf, int size) {
    int ch;
    int cx = (int)strlen(buf);
    curs_set(0);
    keypad(stdscr, TRUE);
    while (1) {
        draw_single_line(label, buf, cx);
        ch = getch();
        if (ch == KEY_F(10)) return NAV_QUIT;
        if (ch == KEY_F(9)) return NAV_BACK;
        if (ch == KEY_F(2) || ch == '\n' || ch == '\r' || ch == KEY_ENTER) return NAV_NEXT;
        if (ch == KEY_LEFT) {
            if (cx > 0) cx--;
        } else if (ch == KEY_RIGHT) {
            if (cx < (int)strlen(buf)) cx++;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (cx > 0) {
                int len = (int)strlen(buf);
                memmove(&buf[cx - 1], &buf[cx], (size_t)(len - cx + 1));
                cx--;
            }
        } else if (ch >= 32 && ch <= 126) {
            int len = (int)strlen(buf);
            if (len < size - 1) {
                memmove(&buf[cx + 1], &buf[cx], (size_t)(len - cx + 1));
                buf[cx] = (char)ch;
                cx++;
            }
        }
    }
}

static int menu_select_nav(const char *title, const MenuItem *items, int count, int current_value, int *out_value) {
    int selected = 0;
    for (int i = 0; i < count; i++) if (items[i].value == current_value) selected = i;
    int ch;
    keypad(stdscr, TRUE);
    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        erase();
        mvprintw(0, 0, "%s", title);
        mvhline(1, 0, '-', cols);
        for (int i = 0; i < count; i++) {
            if (i == selected) attron(A_UNDERLINE | A_BOLD);
            mvprintw(3 + i, 2, "%s", items[i].label);
            if (i == selected) attroff(A_UNDERLINE | A_BOLD);
        }
        mvhline(rows - 2, 0, '-', cols);
        mvprintw(rows - 1, 0, "Up/Down: move   Enter/F2: select   F9: back   F10: quit");
        refresh();
        ch = getch();
        if (ch == KEY_F(10)) return NAV_QUIT;
        if (ch == KEY_F(9)) return NAV_BACK;
        if (ch == KEY_UP && selected > 0) selected--;
        else if (ch == KEY_DOWN && selected < count - 1) selected++;
        else if (ch == KEY_F(2) || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            *out_value = items[selected].value;
            return NAV_NEXT;
        }
    }
}

static const char *assertion_name(AssertType t) {
    switch (t) {
    case ASSERT_EXACT: return "Exact match";
    case ASSERT_REGEX: return "Regex match";
    case ASSERT_RANGE: return "Numeric range";
    case ASSERT_CONTAINS: return "Contains string";
    case ASSERT_NOT_CONTAINS: return "Does not contain string";
    default: return "Unknown";
    }
}

static void preview(const TestCase *tc) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    erase();
    mvprintw(0, 0, "Preview");
    mvhline(1, 0, '-', cols);
    mvprintw(2, 0, "Test name: %s", tc->name);
    mvprintw(3, 0, "Output:    %s", tc->output_path);
    mvprintw(5, 0, "Procedure:");
    int y = 6;
    for (int i = 0; i < tc->procedure.line_count && y < rows - 7; i++, y++) {
        mvprintw(y, 0, "%4d  %s", i + 1, tc->procedure.lines[i]);
    }
    y++;
    if (y < rows - 4) mvprintw(y++, 0, "Assertion type: %s", assertion_name(tc->assertion.type));
    if (tc->assertion.type == ASSERT_RANGE) {
        if (y < rows - 4) mvprintw(y++, 0, "Range: %ld..%ld", tc->assertion.min, tc->assertion.max);
    } else {
        if (y < rows - 4) mvprintw(y++, 0, "Expected: %s", tc->assertion.expected);
    }
    mvhline(rows - 2, 0, '-', cols);
    mvprintw(rows - 1, 0, "F2/save: save   F9: back   F10: quit");
    refresh();
}

int main(void) {
    setlocale(LC_ALL, "");
    TestCase tc;
    memset(&tc, 0, sizeof(tc));
    strcpy(tc.output_path, "test_generated.sh");
    tc.assertion.type = ASSERT_EXACT;
    editor_init(&tc.procedure);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    int step = 0;
    int nav;
    while (1) {
        if (step == 0) {
            nav = prompt_line_nav("Test name:", tc.name, sizeof(tc.name));
            if (nav == NAV_QUIT) break;
            if (nav == NAV_NEXT) step = 1;
        } else if (step == 1) {
            nav = editor_run(&tc.procedure, "Enter Procedure",
                "Assign the value you want to test to RESULT.",
                "Example: RESULT=\"$(command)\"   /   command; RESULT=\"$?\"");
            if (nav == NAV_QUIT) break;
            if (nav == NAV_BACK) step = 0;
            else if (nav == NAV_NEXT) {
                if (buffer_contains_result(&tc.procedure)) step = 2;
                else {
                    status_message("ERROR: Procedure must assign the value to RESULT. Press any key.");
                    getch();
                }
            }
        } else if (step == 2) {
            MenuItem assertion_items[] = {
                {"Exact match", ASSERT_EXACT},
                {"Regex match", ASSERT_REGEX},
                {"Numeric range", ASSERT_RANGE},
                {"Contains string", ASSERT_CONTAINS},
                {"Does not contain string", ASSERT_NOT_CONTAINS}
            };
            int v = tc.assertion.type;
            nav = menu_select_nav("Select assertion type", assertion_items, 5, tc.assertion.type, &v);
            if (nav == NAV_QUIT) break;
            if (nav == NAV_BACK) step = 1;
            else { tc.assertion.type = (AssertType)v; step = 3; }
        } else if (step == 3) {
            if (tc.assertion.type == ASSERT_RANGE) {
                char minbuf[64];
                snprintf(minbuf, sizeof(minbuf), "%ld", tc.assertion.min);
                nav = prompt_line_nav("Minimum value:", minbuf, sizeof(minbuf));
                if (nav == NAV_QUIT) break;
                if (nav == NAV_BACK) { step = 2; continue; }
                tc.assertion.min = strtol(minbuf, NULL, 10);

                char maxbuf[64];
                snprintf(maxbuf, sizeof(maxbuf), "%ld", tc.assertion.max);
                nav = prompt_line_nav("Maximum value:", maxbuf, sizeof(maxbuf));
                if (nav == NAV_QUIT) break;
                if (nav == NAV_BACK) { step = 3; continue; }
                tc.assertion.max = strtol(maxbuf, NULL, 10);
                step = 4;
            } else if (tc.assertion.type == ASSERT_REGEX) {
                EditorBuffer rb;
                editor_set_single_line(&rb, tc.assertion.expected);
                nav = editor_run_regex(&rb, "Enter Regex",
                    "Edit regex directly. Press F3 to insert a regex part at the cursor.",
                    "Example: ^[0-9]+$ can be made with Start + Digits repeat + End.");
                if (nav == NAV_QUIT) break;
                if (nav == NAV_BACK) step = 2;
                else { editor_get_single_line(&rb, tc.assertion.expected, sizeof(tc.assertion.expected)); step = 4; }
            } else {
                nav = prompt_line_nav("Expected value / string:", tc.assertion.expected, sizeof(tc.assertion.expected));
                if (nav == NAV_QUIT) break;
                if (nav == NAV_BACK) step = 2;
                else step = 4;
            }
        } else if (step == 4) {
            nav = prompt_line_nav("Output script path:", tc.output_path, sizeof(tc.output_path));
            if (nav == NAV_QUIT) break;
            if (nav == NAV_BACK) step = 3;
            else step = 5;
        } else if (step == 5) {
            preview(&tc);
            int ch = getch();
            if (ch == KEY_F(10)) break;
            if (ch == KEY_F(9)) step = 4;
            else if (ch == KEY_F(2) || ch == 's') {
                int rc = generate_test_script(&tc, tc.output_path);
                mvprintw(LINES - 2, 0, rc == 0 ? "Saved. Press any key." : "Save failed. Press any key.");
                refresh();
                getch();
            }
        }
    }
    endwin();
    return 0;
}
