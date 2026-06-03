#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "testgen.h"

AssertType select_assert_type(void) {
    int ch;
    while (1) {
        clear();
        mvprintw(0, 0, "Select assertion type");
        mvprintw(2, 0, "1. Exact match");
        mvprintw(3, 0, "2. Regex match");
        mvprintw(4, 0, "3. Numeric range");
        mvprintw(5, 0, "4. Contains text");
        mvprintw(6, 0, "5. Does not contain text");
        mvprintw(8, 0, "Press 1-5");
        refresh();
        ch = getch();
        if (ch >= '1' && ch <= '5') return (AssertType)(ch - '0');
    }
}

void preview_testcase(const TestCase *tc) {
    clear();
    mvprintw(0, 0, "Preview");
    mvprintw(2, 0, "Test name: %s", tc->name);
    mvprintw(3, 0, "Output: %s", tc->output_path);
    mvprintw(5, 0, "Procedure:");

    int y = 6;
    for (int i = 0; i < tc->procedure.line_count && y < LINES - 8; i++, y++) {
        mvprintw(y, 2, "%.*s", COLS - 4, tc->procedure.lines[i]);
    }

    y += 1;
    if (y < LINES - 5) {
        mvprintw(y++, 0, "Assertion:");
        switch (tc->assertion.type) {
            case ASSERT_EXACT: mvprintw(y++, 2, "Exact: %s", tc->assertion.expected); break;
            case ASSERT_REGEX: mvprintw(y++, 2, "Regex: %s", tc->assertion.expected); break;
            case ASSERT_RANGE: mvprintw(y++, 2, "Range: %ld..%ld", tc->assertion.min, tc->assertion.max); break;
            case ASSERT_CONTAINS: mvprintw(y++, 2, "Contains: %s", tc->assertion.expected); break;
            case ASSERT_NOT_CONTAINS: mvprintw(y++, 2, "Not contains: %s", tc->assertion.expected); break;
            default: break;
        }
    }

    mvprintw(LINES - 2, 0, "s: generate / q: quit");
    refresh();
}

int main(void) {
    setlocale(LC_ALL, "");
    TestCase tc;
    memset(&tc, 0, sizeof(tc));
    init_buffer(&tc.procedure);
    strncpy(tc.output_path, "test_generated.sh", sizeof(tc.output_path) - 1);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!input_text("Input test name", "Test name: ", tc.name, sizeof(tc.name))) goto finish;

    if (!edit_buffer(&tc.procedure,
        "Input procedure",
        "判定したい値は RESULT 変数に格納してください。例: RESULT=\"$(command)\"")) {
        goto finish;
    }

    tc.assertion.type = select_assert_type();

    if (tc.assertion.type == ASSERT_RANGE) {
        if (!input_long_value("Input expected range", "Min: ", &tc.assertion.min)) goto finish;
        if (!input_long_value("Input expected range", "Max: ", &tc.assertion.max)) goto finish;
    } else if (tc.assertion.type == ASSERT_REGEX) {
        if (!input_text("Input expected value", "Regex: ", tc.assertion.expected, sizeof(tc.assertion.expected))) goto finish;
    } else {
        if (!input_text("Input expected value", "Expected: ", tc.assertion.expected, sizeof(tc.assertion.expected))) goto finish;
    }

    input_text("Input output filename", "Output filename: ", tc.output_path, sizeof(tc.output_path));
    if (tc.output_path[0] == '\0') strncpy(tc.output_path, "test_generated.sh", sizeof(tc.output_path) - 1);

    while (1) {
        preview_testcase(&tc);
        int ch = getch();
        if (ch == 'q') break;
        if (ch == 's') {
            int rc = generate_test_script(&tc, tc.output_path);
            if (rc == 0) {
                mvprintw(LINES - 1, 0, "Generated: %s  Run chmod +x before execution. Press any key.", tc.output_path);
            } else {
                mvprintw(LINES - 1, 0, "Failed to generate. Press any key.");
            }
            refresh();
            getch();
            break;
        }
    }

finish:
    endwin();
    return 0;
}
