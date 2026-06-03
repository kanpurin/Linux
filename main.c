#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include "testgen.h"

AssertType select_assert_type(void) {
    int ch;
    while (1) {
        clear();
        mvprintw(0, 0, "判定方式を選択");
        mvprintw(2, 0, "1. 完全一致");
        mvprintw(3, 0, "2. 正規表現一致");
        mvprintw(4, 0, "3. 数値範囲");
        mvprintw(5, 0, "4. 文字列を含む");
        mvprintw(6, 0, "5. 文字列を含まない");
        mvprintw(8, 0, "番号を押してください");
        refresh();
        ch = getch();
        if (ch >= '1' && ch <= '5') return (AssertType)(ch - '0');
    }
}

void preview_testcase(const TestCase *tc) {
    clear();
    mvprintw(0, 0, "プレビュー");
    mvprintw(2, 0, "テスト名: %s", tc->name);
    mvprintw(3, 0, "出力先: %s", tc->output_path);
    mvprintw(5, 0, "手順:");

    int y = 6;
    for (int i = 0; i < tc->procedure.line_count && y < LINES - 8; i++, y++) {
        mvprintw(y, 2, "%.*s", COLS - 4, tc->procedure.lines[i]);
    }

    y += 1;
    if (y < LINES - 5) {
        mvprintw(y++, 0, "判定:");
        switch (tc->assertion.type) {
            case ASSERT_EXACT: mvprintw(y++, 2, "完全一致: %s", tc->assertion.expected); break;
            case ASSERT_REGEX: mvprintw(y++, 2, "正規表現: %s", tc->assertion.expected); break;
            case ASSERT_RANGE: mvprintw(y++, 2, "範囲: %ld..%ld", tc->assertion.min, tc->assertion.max); break;
            case ASSERT_CONTAINS: mvprintw(y++, 2, "含む: %s", tc->assertion.expected); break;
            case ASSERT_NOT_CONTAINS: mvprintw(y++, 2, "含まない: %s", tc->assertion.expected); break;
            default: break;
        }
    }

    mvprintw(LINES - 2, 0, "s: 生成 / q: 終了");
    refresh();
}

int main(void) {
    TestCase tc;
    memset(&tc, 0, sizeof(tc));
    init_buffer(&tc.procedure);
    strncpy(tc.output_path, "test_generated.sh", sizeof(tc.output_path) - 1);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!input_text("テスト名入力", "テスト名: ", tc.name, sizeof(tc.name))) goto finish;

    if (!edit_buffer(&tc.procedure,
        "手順入力",
        "判定したい値は RESULT 変数に格納してください。例: RESULT=\"$(command)\"")) {
        goto finish;
    }

    tc.assertion.type = select_assert_type();

    if (tc.assertion.type == ASSERT_RANGE) {
        if (!input_long_value("期待範囲入力", "最小値: ", &tc.assertion.min)) goto finish;
        if (!input_long_value("期待範囲入力", "最大値: ", &tc.assertion.max)) goto finish;
    } else if (tc.assertion.type == ASSERT_REGEX) {
        if (!input_text("期待値入力", "正規表現: ", tc.assertion.expected, sizeof(tc.assertion.expected))) goto finish;
    } else {
        if (!input_text("期待値入力", "期待値: ", tc.assertion.expected, sizeof(tc.assertion.expected))) goto finish;
    }

    input_text("出力ファイル名入力", "出力ファイル名: ", tc.output_path, sizeof(tc.output_path));
    if (tc.output_path[0] == '\0') strncpy(tc.output_path, "test_generated.sh", sizeof(tc.output_path) - 1);

    while (1) {
        preview_testcase(&tc);
        int ch = getch();
        if (ch == 'q') break;
        if (ch == 's') {
            int rc = generate_test_script(&tc, tc.output_path);
            if (rc == 0) {
                mvprintw(LINES - 1, 0, "生成しました: %s  chmod +x して実行できます。何かキーを押してください。", tc.output_path);
            } else {
                mvprintw(LINES - 1, 0, "生成に失敗しました。何かキーを押してください。");
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
