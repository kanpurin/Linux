#ifndef TESTGEN_H
#define TESTGEN_H

#define MAX_LINES 1024
#define MAX_COLS 512

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
    int cy;
    int cx;
    int insert_mode;
} EditorBuffer;

typedef struct {
    AssertType type;
    char expected[512];
    long min;
    long max;
} Assertion;

typedef struct {
    char name[256];
    EditorBuffer procedure;
    Assertion assertion;
    char output_path[256];
} TestCase;

void editor_init(EditorBuffer *b);
int editor_run(EditorBuffer *b, const char *title, const char *help1, const char *help2);
int generate_test_script(const TestCase *tc, const char *path);

#endif
