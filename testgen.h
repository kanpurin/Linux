#ifndef TESTGEN_H
#define TESTGEN_H

#define MAX_LINES 1024
#define MAX_COLS 512
#define MAX_NAME 256
#define MAX_PATH 256
#define MAX_EXPECTED 512

typedef enum {
    ASSERT_EXACT = 1,
    ASSERT_REGEX,
    ASSERT_RANGE,
    ASSERT_CONTAINS,
    ASSERT_NOT_CONTAINS
} AssertType;

typedef struct {
    char lines[MAX_LINES][MAX_COLS];
    int line_count;
    int cur_y;
    int cur_x;
    int insert_mode;
} EditorBuffer;

typedef struct {
    AssertType type;
    char expected[MAX_EXPECTED];
    long min;
    long max;
} Assertion;

typedef struct {
    char name[MAX_NAME];
    EditorBuffer procedure;
    Assertion assertion;
    char output_path[MAX_PATH];
} TestCase;

void init_buffer(EditorBuffer *buf);
int edit_buffer(EditorBuffer *buf, const char *title, const char *hint);
int input_text(const char *title, const char *label, char *out, int out_size);
int input_long_value(const char *title, const char *label, long *out);
AssertType select_assert_type(void);
void preview_testcase(const TestCase *tc);
int generate_test_script(const TestCase *tc, const char *path);

#endif
