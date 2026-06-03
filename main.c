#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include "testgen.h"

static void prompt_line(const char *label, char *buf, int size) {
    int rows, cols;
    (void)cols;
    getmaxyx(stdscr, rows, cols);
    erase();
    mvprintw(0, 0, "testgen");
    mvhline(1, 0, '-', cols);
    mvprintw(3, 0, "%s", label);
    move(5, 0);
    echo();
    curs_set(1);
    getnstr(buf, size - 1);
    noecho();
    curs_set(0);
}

static int menu_assert_type(void) {
    int ch;
    while (1) {
        erase();
        mvprintw(0, 0, "Select assertion type");
        mvprintw(2, 0, "1. Exact match");
        mvprintw(3, 0, "2. Regex match");
        mvprintw(4, 0, "3. Numeric range");
        mvprintw(5, 0, "4. Contains string");
        mvprintw(6, 0, "5. Does not contain string");
        mvprintw(8, 0, "Select 1-5: ");
        refresh();
        ch = getch();
        if (ch >= '1' && ch <= '5') return ch - '0';
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
    for (int i = 0; i < tc->procedure.line_count && y < rows - 6; i++, y++) {
        mvprintw(y, 0, "%4d  %s", i + 1, tc->procedure.lines[i]);
    }
    y += 1;
    if (y < rows - 3) mvprintw(y++, 0, "Assertion type: %d", tc->assertion.type);
    if (tc->assertion.type == ASSERT_RANGE) {
        if (y < rows - 3) mvprintw(y++, 0, "Range: %ld..%ld", tc->assertion.min, tc->assertion.max);
    } else {
        if (y < rows - 3) mvprintw(y++, 0, "Expected: %s", tc->assertion.expected);
    }
    mvprintw(rows - 1, 0, "F2 save   F10 quit without save   s save   q quit");
    refresh();
}

int main(void) {
    setlocale(LC_ALL, "");
    TestCase tc;
    memset(&tc, 0, sizeof(tc));
    strcpy(tc.output_path, "test_generated.sh");
    editor_init(&tc.procedure);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    prompt_line("Test name:", tc.name, sizeof(tc.name));
    if (!editor_run(&tc.procedure, "Enter Procedure",
               "Assign the value you want to test to RESULT.",
               "Example: RESULT=\"$(command)\"   /   command; RESULT=\"$?\"")) {
        endwin();
        return 0;
    }

    tc.assertion.type = (AssertType)menu_assert_type();
    if (tc.assertion.type == ASSERT_RANGE) {
        char minbuf[64], maxbuf[64];
        prompt_line("Minimum value:", minbuf, sizeof(minbuf));
        prompt_line("Maximum value:", maxbuf, sizeof(maxbuf));
        tc.assertion.min = strtol(minbuf, NULL, 10);
        tc.assertion.max = strtol(maxbuf, NULL, 10);
    } else {
        prompt_line("Expected value / regex / string:", tc.assertion.expected, sizeof(tc.assertion.expected));
    }
    prompt_line("Output script path:", tc.output_path, sizeof(tc.output_path));

    preview(&tc);
    int ch;
    while ((ch = getch()) != 'q' && ch != KEY_F(10)) {
        if (ch == 's' || ch == KEY_F(2)) {
            int rc = generate_test_script(&tc, tc.output_path);
            mvprintw(LINES - 2, 0, rc == 0 ? "Saved." : "Save failed.");
            refresh();
        }
    }
    endwin();
    return 0;
}
